#ifndef LLARP_STOPWATCH_HPP
#define LLARP_STOPWATCH_HPP

#include <nonstd/optional.hpp>
#include <absl/time/clock.h>

namespace llarp
{
  namespace util
  {
    class Stopwatch
    {
      nonstd::optional< absl::Time > m_start;
      nonstd::optional< absl::Time > m_stop;

     public:
      Stopwatch() = default;

      void
      start()
      {
        assert(!m_start);
        assert(!m_stop);
        m_start.emplace(absl::Now());
      }

      void
      stop()
      {
        assert(m_start);
        assert(!m_stop);
        m_stop.emplace(absl::Now());
      }

      bool
      done() const
      {
        return m_start && m_stop;
      }

      absl::Duration
      time() const
      {
        assert(m_start);
        assert(m_stop);
        return m_stop.value() - m_start.value();
      }
    };

  }  // namespace util
}  // namespace llarp

#endif
