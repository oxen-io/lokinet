#pragma once

#include "session.hpp"

#include <llarp/address/address.hpp>
#include <llarp/util/thread/threading.hpp>

namespace llarp
{
    /**
        OutboundSessionType objects are held in shared_ptr's, and getters will return the shared_ptr
        or nullptr if not found. MAKE SURE TO CHECK ON RETURN!!
    */
    struct session_map
    {
      protected:
        std::unordered_map<session_tag, NetworkAddress> _session_lookup;
        std::unordered_map<NetworkAddress, std::shared_ptr<session::BaseSession>> _sessions;

        using Lock_t = util::NullLock;
        mutable util::NullMutex session_mutex;

      public:
        /** Returns the number of sessions currently mapped to remote addresses
         */
        size_t count() const
        {
            Lock_t l{session_mutex};
            return _sessions.size();
        }

        /** Called by owning object to apply a callback to every session currently mapped
         */
        void for_each(std::function<void(session::BaseSession&)> hook);

        /** Called by owning object to tick OutboundSessions. InboundSession objects are not PathHandlers, so they have
            no concept of tick functionality
         */
        void tick_outbounds(std::chrono::milliseconds now);

        /** Called by owning object to clear all Sessions mapped
         */
        void clear_sessions();

        /** This functions exactly as std::unordered_map's ::insert_or_assign method. If a key equivalent
            to `remote` already exists in the container, `sesh` is assigned to the mapped type. If the key
            does NOT exist, `sesh` is inserted as the value corresponding to the key `remote`.

            The returned `bool` is true if the insertion took place and `false` if assignment occurred. The
            iterator is the shared_ptr that was inserted or assigned
        */
        std::pair<std::shared_ptr<session::BaseSession>, bool> insert_or_assign(
            NetworkAddress remote, std::shared_ptr<session::BaseSession> sesh);

        std::optional<NetworkAddress> get_remote(const session_tag& tag) const;

        std::shared_ptr<session::BaseSession> get_session(const NetworkAddress& remote) const;
        std::shared_ptr<session::BaseSession> operator[](const session_tag& tag) const { return get_session(tag); }

        std::shared_ptr<session::BaseSession> get_session(const session_tag& tag) const;
        std::shared_ptr<session::BaseSession> operator[](const NetworkAddress& remote) const
        {
            return get_session(remote);
        }

        void unmap(const session_tag& tag);
        void unmap(const NetworkAddress& remote);

        bool have_session(const session_tag& tag) const;
        bool have_session(const NetworkAddress& remote) const
        {
            Lock_t l{session_mutex};
            return _sessions.count(remote);
        }
    };
}  //  namespace llarp
