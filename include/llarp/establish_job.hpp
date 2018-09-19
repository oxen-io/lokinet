#ifndef LLARP_ESTABLISH_JOB_HPP
#define LLARP_ESTABLISH_JOB_HPP

namespace llarp
{
  struct OutboundLinkEstablishJob
  {
    RouterContact rc;

    OutboundLinkEstablishJob(const RouterContact& remote) : rc(remote)
    {
    }

    virtual ~OutboundLinkEstablishJob(){};

    virtual void
    Success() = 0;

    virtual void
    Failed() = 0;

    virtual void
    AttemptTimedout() = 0;

    virtual void
    Attempt() = 0;

    virtual bool
    ShouldRetry() const = 0;
  };
}  // namespace llarp

#endif
