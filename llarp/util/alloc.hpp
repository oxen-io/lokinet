#ifndef LLARP_UTIL_ALLOC_HPP
#define LLARP_UTIL_ALLOC_HPP
#include <memory>
#include <experimental/memory_resource>
#include <bitset>
#include <array>

namespace llarp
{
  namespace util
  {
    /// simple single threaded allocatable super type template
    template < typename Value_t, std::size_t maxEntries >
    struct AllocPool
    {
      using Ptr_t = Value_t *;

      AllocPool()
      {
        mem = new Memory();
      }

      ~AllocPool()
      {
        delete mem;
      }

      Ptr_t
      NewPtr()
      {
        void *ptr = mem->allocate();
        return new(ptr) Value_t();
      }

      void
      DelPtr(Ptr_t p)
      {
        p->~Value_t();
        mem->deallocate(p);
      }

      bool
      Full() const
      {
        return mem->full();
      }

      bool
      HasRoomFor(size_t numItems)
      {
        return mem->hasRoomFor(numItems);
      }

     private:
      struct Memory
      {
        uint8_t _buffer[maxEntries * sizeof(Value_t)];
        std::bitset< maxEntries > _allocated = {0};
        std::size_t _pos                     = 0;

        bool
        full() const
        {
          return _allocated.size() == _allocated.count();
        }

        bool
        hasRoomFor(size_t num)
        {
          return _allocated.count() + num <= _allocated.size();
        }

        void
        deallocate(void *ptr)
        {
          if(ptr == nullptr)
            throw std::bad_alloc();
          uint8_t *v_ptr         = (uint8_t *)ptr;
          const std::size_t _idx = (v_ptr - _buffer) / sizeof(Value_t);
          _allocated.reset(_idx);
        }

        [[nodiscard]] void *
        allocate()
        {
          const std::size_t _started = _pos;
          while(_allocated.test(_pos))
          {
            _pos = (_pos + 1) % maxEntries;
            if(_pos == _started)
            {
              // we are full
              throw std::bad_alloc();
            }
          }
          _allocated.set(_pos);
          Value_t *ptr = (Value_t *)&_buffer[_pos * sizeof(Value_t)];
          _pos         = (_pos + 1) % maxEntries;
          return ptr;
        }
      };

      Memory *mem;
    };

  }  // namespace util
}  // namespace llarp

#endif