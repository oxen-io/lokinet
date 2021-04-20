#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <queue>
#include <functional>
#include <string_view>
#include <type_traits>
#include <oxenmq/variant.h>
#include <vector>
#include <optional>
#include <uvw/async.h>

namespace llarp::quic
{
  class Connection;

  using bstring_view = std::basic_string_view<std::byte>;

  // Shortcut for a const-preserving `reinterpret_cast`ing c.data() from a std::byte to a uint8_t
  // pointer, because we need it all over the place in the ngtcp2 API and I'd rather deal with
  // std::byte's out here for type safety.
  template <
      typename Container,
      typename = std::enable_if_t<
          sizeof(typename std::remove_reference_t<Container>::value_type) == sizeof(uint8_t)>>
  inline auto*
  u8data(Container&& c)
  {
    using u8_sameconst_t = std::conditional_t<
        std::is_const_v<std::remove_pointer_t<decltype(c.data())>>,
        const uint8_t,
        uint8_t>;
    return reinterpret_cast<u8_sameconst_t*>(c.data());
  }

  // Type-safe wrapper around a int64_t stream id.  Default construction is ngtcp2's special
  // "no-stream" id.
  struct StreamID
  {
    int64_t id{-1};
    bool
    operator==(const StreamID& s) const
    {
      return s.id == id;
    }
    bool
    operator!=(const StreamID& s) const
    {
      return s.id != id;
    }
    bool
    operator<(const StreamID& s) const
    {
      return s.id < id;
    }
    bool
    operator<=(const StreamID& s) const
    {
      return s.id <= id;
    }
    bool
    operator>(const StreamID& s) const
    {
      return s.id > id;
    }
    bool
    operator>=(const StreamID& s) const
    {
      return s.id >= id;
    }
  };

  // Application error code we close with if the data handle throws
  inline constexpr uint64_t STREAM_ERROR_EXCEPTION = (1ULL << 62) - 2;

  // Error code we send to a stream close callback if the stream's connection expires; this is *not*
  // sent over quic, hence using a value >= 2^62 (quic's maximum serializable integer).
  inline constexpr uint64_t STREAM_ERROR_CONNECTION_EXPIRED = (1ULL << 62) + 1;

  std::ostream&
  operator<<(std::ostream& o, const StreamID& s);
}  // namespace llarp::quic

namespace std
{
  template <>
  struct hash<llarp::quic::StreamID>
  {
    size_t
    operator()(const llarp::quic::StreamID& s) const
    {
      return std::hash<decltype(s.id)>{}(s.id);
    }
  };
}  // namespace std

namespace llarp::quic
{
  // Class for an established stream (a single connection has multiple streams): we have a
  // fixed-sized ring buffer for holding outgoing data, and a callback to invoke on received data.
  // To construct a Stream call `conn.open_stream()`.
  class Stream : public std::enable_shared_from_this<Stream>
  {
   public:
    // Returns the StreamID of this stream
    const StreamID&
    id() const
    {
      return stream_id;
    }

    // Sets the size of the outgoing data buffer.  This may *only* be used if the buffer is
    // currently entirely empty; otherwise a runtime_error is thrown.  The minimum buffer size is
    // 2048, the default is 64kiB.  A value of 0 puts the Stream into user-provided buffer mode
    // where only the version of `append` taking ownership of a char* is permitted.
    void
    set_buffer_size(size_t size);

    // Returns the size of the buffer (including both pending and free space).  If using
    // user-provided buffer mode then this is the sum of all held buffers.
    size_t
    buffer_size() const;

    // Returns the number of free bytes available in the outgoing stream data buffer. Always 0 in
    // user-provided buffer mode.
    size_t
    available() const
    {
      return is_closing || buffer.empty() ? 0 : buffer.size() - size;
    }

    // Returns the number of bytes currently referenced in the buffer (i.e. pending or
    // sent-but-unacknowledged).
    size_t
    used() const
    {
      return size;
    }

    // Returns the number of bytes of the buffer that have been sent but not yet acknowledged and
    // thus are still required.
    size_t
    unacked() const
    {
      return unacked_size;
    }

    // Returns the number of bytes of the buffer that have not yet been sent.
    size_t
    unsent() const
    {
      return used() - unacked();
    }

    // Try to append all of the given bytes to the outgoing stream data buffer.  Returns true if
    // successful, false (without appending anything) if there is insufficient space.  If you want
    // to append as much as possible then use `append_any` instead.
    bool
    append(bstring_view data);
    bool
    append(std::string_view data)
    {
      return append(bstring_view{reinterpret_cast<const std::byte*>(data.data()), data.size()});
    }

    // Append bytes to the outgoing stream data buffer, allowing partial consumption of data if the
    // entire provided data cannot be appended.  Returns the number of appended bytes (which will be
    // less than the total provided if the provided data is larger than `available()`).  If you want
    // an all-or-nothing append then use `append` instead.
    size_t
    append_any(bstring_view data);
    size_t
    append_any(std::string_view data)
    {
      return append_any(bstring_view{reinterpret_cast<const std::byte*>(data.data()), data.size()});
    }

    // Takes ownership of the given buffer pointer, queuing it to be sent after any existing buffers
    // and freed once fully acked.  You *must* have called `set_buffer_size(0)` (or set the
    // endpoint's default_stream_buffer_size to 0) in order to use this.
    void
    append_buffer(const std::byte* buf, size_t length);

    // Starting closing the stream and prevent any more outgoing data from being appended.  If
    // `error_code` is provided then we close immediately with the given code; if std::nullopt (the
    // default) we close gracefully by sending a FIN bit.
    void
    close(std::optional<uint64_t> error_code = std::nullopt);

    // Returns true if this Stream is closing (or already closed).
    bool
    closing() const
    {
      return is_closing;
    }

    // Callback invoked when data is received
    using data_callback_t = std::function<void(Stream&, bstring_view)>;

    // Callback invoked when the stream is closed
    using close_callback_t = std::function<void(Stream&, std::optional<uint64_t> error_code)>;

    // Callback invoked when free stream buffer space becomes available.  Should return true if the
    // callback is finished and can be discarded, false if the callback is still needed.  If
    // returning false then it *must* have filled the stream's outgoing buffer (this is asserted in
    // a debug build).
    using unblocked_callback_t = std::function<bool(Stream&)>;

    // Callback to invoke when we receive some incoming data; there's no particular guarantee on the
    // size of the data, just that this will always be called in sequential order.
    data_callback_t data_callback;

    // Callback to invoke when the connection has closed.  If the close was an abrupt stream close
    // initiated by the remote then `error_code` will be set to whatever code the remote side
    // provided; for graceful closing or locally initiated closing the error code will be null.
    close_callback_t close_callback;

    // Queues a callback to be invoked when space becomes available for writing in the buffer.  The
    // callback should true if it completed, false if it still needs more buffer space.  If multiple
    // callbacks are queued they are invoked in order, space permitting.  The stored std::function
    // will not be moved or copied after being invoked (i.e. if invoked multiple times it will
    // always be invoked on the same instance).
    //
    // Available callbacks should only be used when the buffer is full, typically immediately after
    // an `append_any` call that returns less than the full write.  Similarly a false return from an
    // unblock function (which keeps the callback alive) should satisfy the same condition.
    //
    // In user-provided buffer mode the callback will be invoked after any data has been acked: it
    // is up to the caller to look at used()/buffer_size()/etc. to decide what to do.  As described
    // above, return true to remove this callback, false to keep it and try again after the next
    // ack.
    void
    when_available(unblocked_callback_t unblocked_cb);

    // Calls io_ready() on the stream's connection to scheduling sending outbound data
    void
    io_ready();

    // Schedules processing of the "when_available" callbacks
    void
    available_ready();

    // Lets you stash some arbitrary data in a shared_ptr; this is not used internally.
    void
    data(std::shared_ptr<void> data);

    // Variation of data() that holds the pointer in a weak_ptr instead of a shared_ptr.  (Note that
    // setting this replaces data() and vice versa).
    void
    weak_data(std::weak_ptr<void> data);

    // Retrieves the stashed data, with a static_cast to the desired type.  This is used for
    // retrieval of both shared or weak data types (if held as a weak_ptr it is lock()ed first).
    template <typename T>
    std::shared_ptr<T>
    data() const
    {
      return std::static_pointer_cast<T>(
          std::holds_alternative<std::shared_ptr<void>>(user_data)
              ? var::get<std::shared_ptr<void>>(user_data)
              : var::get<std::weak_ptr<void>>(user_data).lock());
    }

    // Returns a reference to the connection that owns this stream
    Connection&
    get_connection()
    {
      return conn;
    }

    ~Stream();

   private:
    friend class Connection;

    Stream(
        Connection& conn,
        data_callback_t data_cb,
        close_callback_t close_cb,
        size_t buffer_size,
        StreamID id = {-1});
    Stream(Connection& conn, StreamID id, size_t buffer_size);

    // Non-copyable, non-movable; we manage it via a unique_ptr held by its Connection
    Stream(const Stream&) = delete;
    const Stream&
    operator=(const Stream&) = delete;
    Stream(Stream&&) = delete;
    Stream&
    operator=(Stream&&) = delete;

    Connection& conn;

    // Callback(s) to invoke once we have the requested amount of space available in the buffer.
    std::queue<unblocked_callback_t> unblocked_callbacks;
    void
    handle_unblocked();  // Processes the above if space is available

    // Called to advance the number of acknowledged bytes (freeing up that space in the buffer for
    // appending data).
    void
    acknowledge(size_t bytes);

    // Returns a view into unwritten stream data.  This returns a vector of string_views of the data
    // to write, in order.  After writing any of the provided data you must call `wrote()` to signal
    // how much of the given data was consumed (to advance the next pending() call).
    std::vector<bstring_view>
    pending();

    // Called to signal that bytes have been written and should now be considered sent (but still
    // unacknowledged), thereby advancing the initial data position returned by the next `pending()`
    // call.  Should typically be called after `pending()` to signal how much of the pending data
    // was actually used.
    void
    wrote(size_t bytes);

    // Called by the owning Connection to do a "hard" close of a stream during Connection
    // destruction: unlike a regular close this doesn't try to transmit a close over the wire (which
    // won't work since the Connection is dead), it just fires the close callback and cleans up.
    void
    hard_close();

    // ngtcp2 stream_id, assigned during stream creation
    StreamID stream_id{-1};

    // ring buffer of outgoing stream data that has not yet been acknowledged.  This cannot be
    // resized once used as ngtcp2 will have pointers into the data.  If this is empty then we are
    // in user-provided buffer mode.
    std::vector<std::byte> buffer{65536};

    // user-provided buffers; only used when `buffer` is empty (via a `set_buffer_size(0)` or a 0
    // size given in the constructor).
    std::deque<std::pair<std::unique_ptr<const std::byte[]>, size_t>> user_buffers;

    // Offset of the first used byte in the circular buffer, will always be in [0, buffer.size()).
    // For user-provided buffers this is the starting offset in the currently sending user-provided
    // buffer.
    size_t start{0};

    // Number of sent-but-unacked packets in the buffer (i.e. [start, start+unacked_size) are sent
    // but not yet acked).
    size_t unacked_size{0};

    // Number of used bytes in the buffer; thus start+size is the next write location and
    // [start+unacked_size, start+size) is the range of not-yet-sent bytes.  (Note that this
    // description is ignoring the circularity of the buffer).
    size_t size{0};

    bool is_new{true};
    bool is_closing{false};
    bool sent_fin{false};
    bool is_shutdown{false};

    // Async trigger we use to schedule when_available callbacks (so that we can make them happen in
    // batches rather than after each and every packet ack).
    std::shared_ptr<uvw::AsyncHandle> avail_trigger;

    std::variant<std::shared_ptr<void>, std::weak_ptr<void>> user_data;
  };

}  // namespace llarp::quic
