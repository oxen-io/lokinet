#ifndef LLARP_UTIL_TIMERQUEUE_HPP
#define LLARP_UTIL_TIMERQUEUE_HPP

#include <util/meta/object.hpp>
#include <util/thread/threading.hpp>

#include <atomic>
#include <absl/time/time.h>
#include <nonstd/optional.hpp>
#include <map>
#include <utility>

namespace llarp
{
  namespace thread
  {
    template < typename Value >
    class TimerQueueItem;

    template < typename Value >
    class TimerQueue
    {
      static constexpr int INDEX_BITS_MIN     = 8;
      static constexpr int INDEX_BITS_MAX     = 24;
      static constexpr int INDEX_BITS_DEFAULT = 17;

     public:
      using Handle = int;

      static constexpr Handle INVALID_HANDLE = -1;

      class Key
      {
        const void* m_key;

       public:
        explicit Key(const void* key) : m_key(key)
        {
        }
        explicit Key(int value) : m_key(reinterpret_cast< const void* >(value))
        {
        }

        bool
        operator==(const Key& other) const
        {
          return m_key == other.m_key;
        }
        bool
        operator!=(const Key& other) const
        {
          return m_key != other.m_key;
        }
      };

     private:
      struct Node
      {
        int m_index{0};
        absl::Time m_time;
        Key m_key;
        Node* m_prev;
        Node* m_next;
        object::Buffer< Value > m_value;

        Node()
            : m_time()
            , m_key(nullptr)
            , m_prev(nullptr)
            , m_next(nullptr)
            , m_value()
        {
        }

        explicit Node(const absl::Time& time)
            : m_time(time)
            , m_key(nullptr)
            , m_prev(nullptr)
            , m_next(nullptr)
            , m_value()
        {
        }
      };

      using NodeMap     = std::map< absl::Time, Node* >;
      using MapIterator = typename NodeMap::iterator;

      const int m_indexMask;
      const int m_indexIterationMask;
      const int m_indexIterationInc;

      mutable util::Mutex m_mutex;

      std::vector< Node* > m_nodes GUARDED_BY(m_mutex);
      std::atomic< Node* > m_nextNode;
      NodeMap m_nodeMap GUARDED_BY(m_mutex);
      std::atomic_size_t m_size;

      void
      freeNode(Node* node)
      {
        node->m_index =
            ((node->m_index + m_indexIterationInc) & m_indexIterationMask)
            | (node->m_index & m_indexMask);

        if(!(node->m_index & m_indexIterationMask))
        {
          node->m_index += m_indexIterationInc;
        }
        node->m_prev = nullptr;
      }

      void
      putFreeNode(Node* node)
      {
        // destroy in place
        node->m_value.value().~Value();

        Node* nextFreeNode = m_nextNode;
        node->m_next       = nextFreeNode;
        while(!m_nextNode.compare_exchange_strong(nextFreeNode, node))
        {
          nextFreeNode = m_nextNode;
          node->m_next = nextFreeNode;
        }
      }

      void
      putFreeNodeList(Node* node)
      {
        if(node)
        {
          node->m_value.value().~Value();

          Node* end = node;
          while(end->m_next)
          {
            end = end->m_next;
            end->m_value.value().~Value();
          }

          Node* nextFreeNode = m_nextNode;
          end->m_next        = nextFreeNode;

          while(!m_nextNode.compare_exchange_strong(nextFreeNode, node))
          {
            nextFreeNode = m_nextNode;
            end->m_next  = nextFreeNode;
          }
        }
      }

      TimerQueue(const TimerQueue&) = delete;
      TimerQueue&
      operator=(const TimerQueue&) = delete;

     public:
      TimerQueue()
          : m_indexMask((1 << INDEX_BITS_DEFAULT) - 1)
          , m_indexIterationMask(~m_indexMask)
          , m_indexIterationInc(m_indexMask + 1)
          , m_nextNode(nullptr)
          , m_size(0)
      {
      }

      explicit TimerQueue(int indexBits)
          : m_indexMask((1 << indexBits) - 1)
          , m_indexIterationMask(~m_indexMask)
          , m_indexIterationInc(m_indexMask + 1)
          , m_nextNode(nullptr)
          , m_size(0)
      {
        assert(INDEX_BITS_MIN <= indexBits && indexBits <= INDEX_BITS_MAX);
      }

      ~TimerQueue()
      {
        removeAll();

        for(Node* node : m_nodes)
        {
          delete node;
        }
      }

      /// Add a new `value` to the queue, scheduled for `time`. If not null:
      /// - set `isAtHead` to true if the new item is at the front of the
      /// queue (eg the item with the lowest `time` value).
      /// - set `newSize` to be the length of the new queue.
      Handle
      add(absl::Time time, const Value& value, bool* isAtHead = nullptr,
          size_t* newSize = nullptr)
      {
        return add(time, value, Key(nullptr), isAtHead, newSize);
      }
      Handle
      add(absl::Time time, const Value& value, const Key& key,
          bool* isAtHead = nullptr, size_t* newSize = nullptr);

      Handle
      add(const TimerQueueItem< Value >& value, bool* isAtHead = nullptr,
          size_t* newSize = nullptr);

      /// Pop the front of the queue into `item` (if not null).
      bool
      popFront(TimerQueueItem< Value >* item = nullptr,
               size_t* newSize = nullptr, absl::Time* newMinTime = nullptr);

      /// Append all records which are less than *or* equal to `time`.
      void
      popLess(absl::Time time,
              std::vector< TimerQueueItem< Value > >* items = nullptr,
              size_t* newSize = nullptr, absl::Time* newMinTime = nullptr);
      void
      popLess(absl::Time time, size_t maxItems,
              std::vector< TimerQueueItem< Value > >* items = nullptr,
              size_t* newSize = nullptr, absl::Time* newMinTime = nullptr);

      bool
      remove(Handle handle, TimerQueueItem< Value >* item = nullptr,
             size_t* newSize = nullptr, absl::Time* newMinTime = nullptr)
      {
        return remove(handle, Key(nullptr), item, newSize, newMinTime);
      }

      bool
      remove(Handle handle, const Key& key,
             TimerQueueItem< Value >* item = nullptr, size_t* newSize = nullptr,
             absl::Time* newMinTime = nullptr);

      void
      removeAll(std::vector< TimerQueueItem< Value > >* items = nullptr);

      /// Update the `time` for the item referred to by the handle
      bool
      update(Handle handle, absl::Time time, bool* isNewTop = nullptr)
      {
        return update(handle, Key(nullptr), time, isNewTop);
      }
      bool
      update(Handle handle, const Key& key, absl::Time time,
             bool* isNewTop = nullptr);

      size_t
      size() const
      {
        return m_size;
      }

      bool
      isValid(Handle handle) const
      {
        return isValid(handle, Key(nullptr));
      }
      bool
      isValid(Handle handle, const Key& key) const
      {
        absl::ReaderMutexLock lock(&m_mutex);
        int index = (handle & m_indexMask) - 1;

        if(0 > index || index >= static_cast< int >(m_nodes.size()))
        {
          return false;
        }
        Node* node = m_nodes[index];

        if(node->m_index != handle || node->m_key != key)
        {
          return false;
        }

        return true;
      }

      nonstd::optional< absl::Time >
      nextTime() const
      {
        absl::ReaderMutexLock lock(&m_mutex);

        if(m_nodeMap.empty())
        {
          return {};
        }

        return m_nodeMap.begin()->first;
      }
    };

    template < typename Value >
    class TimerQueueItem
    {
     public:
      using Handle = typename TimerQueue< Value >::Handle;
      using Key    = typename TimerQueue< Value >::Key;

     private:
      absl::Time m_time;
      Value m_value;
      Handle m_handle;
      Key m_key;

     public:
      TimerQueueItem() : m_time(), m_value(), m_handle(0), m_key(nullptr)
      {
      }

      TimerQueueItem(absl::Time time, const Value& value, Handle handle)
          : m_time(time), m_value(value), m_handle(handle), m_key(nullptr)
      {
      }

      TimerQueueItem(absl::Time time, Value value, Handle handle,
                     const Key& key)
          : m_time(time)
          , m_value(std::move(value))
          , m_handle(handle)
          , m_key(key)
      {
      }

      // clang-format off
      absl::Time& time()      { return m_time; }
      absl::Time time() const { return m_time; }

      Value& value()             { return m_value; }
      const Value& value() const { return m_value; }

      Handle& handle()      { return m_handle; }
      Handle handle() const { return m_handle; }

      Key& key()             { return m_key; }
      const Key& key() const { return m_key; }
      // clang-format on
    };

    template < typename Value >
    typename TimerQueue< Value >::Handle
    TimerQueue< Value >::add(absl::Time time, const Value& value,
                             const Key& key, bool* isAtHead, size_t* newSize)
    {
      absl::WriterMutexLock lock(&m_mutex);

      Node* node;
      if(m_nextNode)
      {
        // Even though we lock, other threads might be freeing nodes
        node       = m_nextNode;
        Node* next = node->m_next;
        while(!m_nextNode.compare_exchange_strong(node, next))
        {
          node = m_nextNode;
          next = node->m_next;
        }
      }
      else
      {
        // The number of nodes cannot grow to a size larger than the range of
        // available indices.

        if((int)m_nodes.size() >= m_indexMask - 1)
        {
          return INVALID_HANDLE;
        }

        node = new Node;
        m_nodes.push_back(node);
        node->m_index =
            static_cast< int >(m_nodes.size()) | m_indexIterationInc;
      }
      node->m_time = time;
      node->m_key  = key;
      new(node->m_value.buffer()) Value(value);

      {
        auto it = m_nodeMap.find(time);

        if(m_nodeMap.end() == it)
        {
          node->m_prev    = node;
          node->m_next    = node;
          m_nodeMap[time] = node;
        }
        else
        {
          node->m_prev               = it->second->m_prev;
          it->second->m_prev->m_next = node;
          node->m_next               = it->second;
          it->second->m_prev         = node;
        }
      }

      ++m_size;
      if(isAtHead)
      {
        *isAtHead = m_nodeMap.begin()->second == node && node->m_prev == node;
      }

      if(newSize)
      {
        *newSize = m_size;
      }

      assert(-1 != node->m_index);
      return node->m_index;
    }

    template < typename Value >
    typename TimerQueue< Value >::Handle
    TimerQueue< Value >::add(const TimerQueueItem< Value >& value,
                             bool* isAtHead, size_t* newSize)
    {
      return add(value.time(), value.value(), value.key(), isAtHead, newSize);
    }

    template < typename Value >
    bool
    TimerQueue< Value >::popFront(TimerQueueItem< Value >* item,
                                  size_t* newSize, absl::Time* newMinTime)
    {
      Node* node = nullptr;

      {
        absl::WriterMutexLock lock(&m_mutex);
        auto it = m_nodeMap.begin();

        if(m_nodeMap.end() == it)
        {
          return false;
        }
        node = it->second;

        if(item)
        {
          item->time()   = node->m_time;
          item->value()  = node->m_value.value();
          item->handle() = node->m_index;
          item->key()    = node->m_key;
        }
        if(node->m_next != node)
        {
          node->m_prev->m_next = node->m_next;
          node->m_next->m_prev = node->m_prev;
          if(it->second == node)
          {
            it->second = node->m_next;
          }
        }
        else
        {
          m_nodeMap.erase(it);
        }

        freeNode(node);
        --m_size;

        if(m_size && newMinTime && !m_nodeMap.empty())
        {
          *newMinTime = m_nodeMap.begin()->first;
        }

        if(newSize)
        {
          *newSize = m_size;
        }
      }

      putFreeNode(node);
      return true;
    }

    template < typename Value >
    void
    TimerQueue< Value >::popLess(absl::Time time,
                                 std::vector< TimerQueueItem< Value > >* items,
                                 size_t* newSize, absl::Time* newMinTime)
    {
      Node* begin = nullptr;
      {
        absl::WriterMutexLock lock(&m_mutex);

        auto it = m_nodeMap.begin();

        while(m_nodeMap.end() != it && it->first <= time)
        {
          Node* const first = it->second;
          Node* const last  = first->m_prev;
          Node* node        = first;

          do
          {
            if(items)
            {
              items->emplace_back(it->first, node->m_value.value(),
                                  node->m_index, node->m_key);
            }
            freeNode(node);
            node = node->m_next;
            --m_size;
          } while(node != first);

          last->m_next = begin;
          begin        = first;

          auto condemned = it;
          ++it;
          m_nodeMap.erase(condemned);
        }

        if(newSize)
        {
          *newSize = m_size;
        }
        if(m_nodeMap.end() != it && newMinTime)
        {
          *newMinTime = it->first;
        }
      }
      putFreeNodeList(begin);
    }
    template < typename Value >
    void
    TimerQueue< Value >::popLess(absl::Time time, size_t maxItems,
                                 std::vector< TimerQueueItem< Value > >* items,
                                 size_t* newSize, absl::Time* newMinTime)
    {
      Node* begin = nullptr;

      {
        absl::WriterMutexLock lock(&m_mutex);

        auto it = m_nodeMap.begin();

        while(m_nodeMap.end() != it && it->first <= time && 0 < maxItems)
        {
          Node* const first = it->second;
          Node* const last  = first->m_prev;
          Node* node        = first;
          Node* prevNode    = first->m_prev;

          do
          {
            if(items)
            {
              items->emplace_back(it->first, node->m_value.value(),
                                  node->m_index, node->m_key);
            }
            freeNode(node);
            prevNode = node;
            node     = node->m_next;
            --m_size;
            --maxItems;
          } while(0 < maxItems && node != first);

          prevNode->m_next = begin;
          begin            = first;

          if(node == first)
          {
            auto condemned = it;
            ++it;
            m_nodeMap.erase(condemned);
          }
          else
          {
            node->m_prev = last;
            last->m_next = node;
            it->second   = node;
            break;
          }
        }

        if(newSize)
        {
          *newSize = m_size;
        }
        if(m_nodeMap.end() != it && newMinTime)
        {
          *newMinTime = it->first;
        }
      }
      putFreeNodeList(begin);
    }

    template < typename Value >
    bool
    TimerQueue< Value >::remove(Handle handle, const Key& key,
                                TimerQueueItem< Value >* item, size_t* newSize,
                                absl::Time* newMinTime)
    {
      Node* node = nullptr;
      {
        absl::WriterMutexLock lock(&m_mutex);
        int index = (handle & m_indexMask) - 1;
        if(index < 0 || index >= (int)m_nodes.size())
        {
          return false;
        }
        node = m_nodes[index];

        if(node->m_index != (int)handle || node->m_key != key
           || nullptr == node->m_prev)
        {
          return false;
        }

        if(item)
        {
          item->time()   = node->m_time;
          item->value()  = node->m_value.value();
          item->handle() = node->m_index;
          item->key()    = node->m_key;
        }

        if(node->m_next != node)
        {
          node->m_prev->m_next = node->m_next;
          node->m_next->m_prev = node->m_prev;

          auto it = m_nodeMap.find(node->m_time);
          if(it->second == node)
          {
            it->second = node->m_next;
          }
        }
        else
        {
          m_nodeMap.erase(node->m_time);
        }
        freeNode(node);
        --m_size;

        if(newSize)
        {
          *newSize = m_size;
        }

        if(m_size && newMinTime)
        {
          assert(!m_nodeMap.empty());

          *newMinTime = m_nodeMap.begin()->first;
        }
      }

      putFreeNode(node);
      return true;
    }

    template < typename Value >
    void
    TimerQueue< Value >::removeAll(
        std::vector< TimerQueueItem< Value > >* items)
    {
      Node* begin = nullptr;
      {
        absl::WriterMutexLock lock(&m_mutex);
        auto it = m_nodeMap.begin();

        while(m_nodeMap.end() != it)
        {
          Node* const first = it->second;
          Node* const last  = first->m_prev;
          Node* node        = first;

          do
          {
            if(items)
            {
              items->emplace_back(it->first, node->m_value.value(),
                                  node->m_index, node->m_key);
            }
            freeNode(node);
            node = node->m_next;
            --m_size;
          } while(node != first);

          last->m_next = begin;
          begin        = first;

          auto condemned = it;
          ++it;
          m_nodeMap.erase(condemned);
        }
      }
      putFreeNodeList(begin);
    }

    template < typename Value >
    bool
    TimerQueue< Value >::update(Handle handle, const Key& key, absl::Time time,
                                bool* isNewTop)
    {
      absl::WriterMutexLock lock(&m_mutex);
      int index = (handle & m_indexMask) - 1;

      if(index < 0 || index >= (int)m_nodes.size())
      {
        return false;
      }
      Node* node = m_nodes[index];

      if(node->m_index != handle || node->m_key != key)
      {
        return false;
      }

      if(node->m_prev != node)
      {
        node->m_prev->m_next = node->m_next;
        node->m_next->m_prev = node->m_prev;

        auto it = m_nodeMap.find(node->m_time);
        if(it->second == node)
        {
          it->second = node->m_next;
        }
      }
      else
      {
        m_nodeMap.erase(node->m_time);
      }
      node->m_time = time;

      auto it = m_nodeMap.find(time);

      if(m_nodeMap.end() == it)
      {
        node->m_prev    = node;
        node->m_next    = node;
        m_nodeMap[time] = node;
      }
      else
      {
        node->m_prev               = it->second->m_prev;
        it->second->m_prev->m_next = node;
        node->m_next               = it->second;
        it->second->m_prev         = node;
      }

      if(isNewTop)
      {
        *isNewTop = m_nodeMap.begin()->second == node && node->m_prev == node;
      }
      return true;
    }
  }  // namespace thread
}  // namespace llarp

#endif
