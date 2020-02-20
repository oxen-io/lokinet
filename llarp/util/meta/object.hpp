#ifndef LLARP_OBJECT_HPP
#define LLARP_OBJECT_HPP

#include <util/thread/threading.hpp>

#include <nonstd/optional.hpp>
#include <vector>

namespace llarp
{
  namespace object
  {
    /// Provide a buffer, capable of holding a single `Value` object.
    /// This is useful for node-based data structures.
    /// Note:
    /// - This union explicitly does *not* manage the lifetime of the object,
    ///   explicit calls to the constructor/destructor must be made.
    template < typename Value >
    union Buffer {
     private:
      char m_buffer[sizeof(Value)];
      char m_align[alignof(Value)];

     public:
      Value*
      address()
      {
        return reinterpret_cast< Value* >(static_cast< void* >(m_buffer));
      }
      const Value*
      address() const
      {
        return reinterpret_cast< Value* >(static_cast< void* >(m_buffer));
      }

      char*
      buffer()
      {
        return m_buffer;
      }
      const char*
      buffer() const
      {
        return m_buffer;
      }

      Value&
      value()
      {
        return *reinterpret_cast< Value* >(this);
      }
      const Value&
      value() const
      {
        return *reinterpret_cast< const Value* >(this);
      }
    };

    template < typename Value >
    class Proxy
    {
      Buffer< Value > m_value;

      Proxy&
      operator=(const Proxy&) = delete;

     public:
      Proxy()
      {
        ::new(m_value.buffer()) Value();
      }

      Proxy(const Proxy& other)
      {
        ::new(m_value.buffer()) Value(other.value());
      }

      Proxy(const Value& value)
      {
        ::new(m_value.buffer()) Value(value);
      }

      // template < typename... Args >
      // Proxy(Args&&... args)
      // {
      //   ::new(m_value.buffer()) Value(std::forward< Args >(args)...);
      // }

      ~Proxy()
      {
        m_value.address()->~Value();
      }

      Value&
      value()
      {
        return m_value.value();
      }

      const Value&
      value() const
      {
        return m_value.value();
      }
    };

    template < typename Value >
    class Catalog;
    template < typename Value >
    class CatalogIterator;

    template < typename Value >
    class CatalogCleaner
    {
      Catalog< Value >* m_catalog;
      typename Catalog< Value >::Node* m_node;
      bool m_shouldDelete;

      CatalogCleaner(const CatalogCleaner&) = delete;
      CatalogCleaner&
      operator=(const CatalogCleaner&) = delete;

     public:
      explicit CatalogCleaner(Catalog< Value >* catalog)
          : m_catalog(catalog), m_node(nullptr), m_shouldDelete(false)
      {
      }

      ~CatalogCleaner();

      void
      manageNode(typename Catalog< Value >::Node* node, bool shouldDelete)
      {
        m_node         = node;
        m_shouldDelete = shouldDelete;
      }

      void
      releaseNode()
      {
        m_node = nullptr;
      }

      void
      release()
      {
        releaseNode();
        m_catalog = nullptr;
      }
    };

    /// A pooling catalog of objects, referred to by a 32-bit handle
    template < typename Value >
    class Catalog
    {
      enum
      {
        INDEX_MASK      = 0X007FFFFF,
        BUSY_INDICATOR  = 0x00800000,
        GENERATION_INC  = 0x01000000,
        GENERATION_MASK = 0XFF000000
      };

      struct Node
      {
        union Payload {
          Buffer< Value > m_buffer;
          Node* m_next;
        };
        Payload m_payload;
        int32_t m_handle;
      };

      std::vector< Node* > m_nodes GUARDED_BY(m_mutex);
      Node* m_next;
      std::atomic_size_t m_size;

      mutable util::Mutex m_mutex;

      friend class CatalogCleaner< Value >;
      friend class CatalogIterator< Value >;

      static Value*
      getValue(Node* node)
      {
        return node->m_payload.m_buffer.address();
      }

      void
      freeNode(Node* node)
      {
        node->m_handle += GENERATION_INC;
        node->m_handle &= ~BUSY_INDICATOR;

        node->m_payload.m_next = m_next;
        m_next                 = node;
      }

      Node*
      findNode(int32_t handle) const SHARED_LOCKS_REQUIRED(m_mutex)
      {
        int32_t index = handle & INDEX_MASK;

        if(0 > index || index >= static_cast< int32_t >(m_nodes.size())
           || !(handle & BUSY_INDICATOR))
        {
          return nullptr;
        }

        Node* node = m_nodes[index];

        return (node->m_handle == handle) ? node : nullptr;
      }

     public:
      Catalog() : m_next(nullptr), m_size(0)
      {
      }

      ~Catalog()
      {
        removeAll();
      }

      int32_t
      add(const Value& value)
      {
        int32_t handle;
        absl::WriterMutexLock l(&m_mutex);
        CatalogCleaner< Value > guard(this);
        Node* node;

        if(m_next)
        {
          node   = m_next;
          m_next = node->m_payload.m_next;

          guard.manageNode(node, false);
        }
        else
        {
          assert(m_nodes.size() < BUSY_INDICATOR);

          node = new Node;
          guard.manageNode(node, true);

          m_nodes.push_back(node);
          node->m_handle = static_cast< int32_t >(m_nodes.size() - 1);
          guard.manageNode(node, false);
        }

        node->m_handle |= BUSY_INDICATOR;
        handle = node->m_handle;

        // construct into the node.
        ::new(getValue(node)) Value(value);

        guard.release();

        ++m_size;
        return handle;
      }

      bool
      remove(int32_t handle, Value* value = nullptr)
      {
        absl::WriterMutexLock l(&m_mutex);
        Node* node = findNode(handle);

        if(!node)
        {
          return false;
        }

        Value* val = getValue(node);

        if(value)
        {
          *value = *val;
        }

        val->~Value();
        freeNode(node);

        --m_size;
        return true;
      }

      void
      removeAll(std::vector< Value >* output = nullptr)
      {
        absl::WriterMutexLock l(&m_mutex);

        for(Node* node : m_nodes)
        {
          if(node->m_handle & BUSY_INDICATOR)
          {
            Value* value = getValue(node);

            if(output)
            {
              output->push_back(*value);
            }
            value->~Value();
          }

          delete node;
        }
        m_nodes.clear();
        m_next = nullptr;
        m_size = 0;
      }

      bool
      replace(const Value& newValue, int32_t handle)
      {
        absl::WriterMutexLock l(&m_mutex);
        Node* node = findNode(handle);

        if(!node)
        {
          return false;
        }

        Value* value = getValue(node);

        value->~Value();
        // construct into the node.
        ::new(value) Value(newValue);
        return true;
      }

      nonstd::optional< Value >
      find(int32_t handle)
      {
        absl::ReaderMutexLock l(&m_mutex);
        Node* node = findNode(handle);

        if(!node)
        {
          return {};
        }

        return *getValue(node);
      }

      size_t
      size() const
      {
        return m_size;
      }

      /// introduced for testing only. verify the current state of the catalog.
      bool
      verify() const;
    };

    template < typename Value >
    class SCOPED_LOCKABLE CatalogIterator
    {
      const Catalog< Value >* m_catalog;
      size_t m_index;

      CatalogIterator(const CatalogIterator&) = delete;
      CatalogIterator&
      operator=(const CatalogIterator&) = delete;

     public:
      explicit CatalogIterator(const Catalog< Value >* catalog)
          SHARED_LOCK_FUNCTION(m_catalog->m_mutex)
          : m_catalog(catalog), m_index(-1)
      {
        m_catalog->m_mutex.ReaderLock();
        operator++();
      }

      ~CatalogIterator() UNLOCK_FUNCTION()
      {
        m_catalog->m_mutex.ReaderUnlock();
      }

      void
      operator++() NO_THREAD_SAFETY_ANALYSIS
      {
        m_index++;
        while(m_index < m_catalog->m_nodes.size()
              && !(m_catalog->m_nodes[m_index]->m_handle
                   & Catalog< Value >::BUSY_INDICATOR))
        {
          m_index++;
        }
      }

      explicit operator bool() const NO_THREAD_SAFETY_ANALYSIS
      {
        return m_index < m_catalog->m_nodes.size();
      }

      std::pair< int32_t, Value >
      operator()() const NO_THREAD_SAFETY_ANALYSIS
      {
        auto* node = m_catalog->m_nodes[m_index];
        return {node->m_handle, *Catalog< Value >::getValue(node)};
      }
    };

    template < typename Value >
    CatalogCleaner< Value >::~CatalogCleaner()
    {
      if(m_catalog && m_node)
      {
        if(m_shouldDelete)
        {
          // We call the destructor elsewhere.
          operator delete(m_node);
        }
        else
        {
          m_catalog->freeNode(m_node);
        }
      }
    }

    template < typename Value >
    bool
    Catalog< Value >::verify() const
    {
      absl::WriterMutexLock l(&m_mutex);

      if(m_nodes.size() < m_size)
      {
        return false;
      }

      size_t busyCount = 0;
      for(size_t i = 0; i < m_nodes.size(); i++)
      {
        if((m_nodes[i]->m_handle & INDEX_MASK) != i)
        {
          return false;
        }
        if(m_nodes[i]->m_handle & BUSY_INDICATOR)
        {
          busyCount++;
        }
      }

      if(m_size != busyCount)
      {
        return false;
      }

      size_t freeCount = 0;
      for(Node* p = m_next; p != nullptr; p = p->m_payload.m_next)
      {
        freeCount++;
      }

      if(freeCount + busyCount != m_nodes.size())
      {
        return false;
      }

      return true;
    }
  }  // namespace object
}  // namespace llarp

#endif
