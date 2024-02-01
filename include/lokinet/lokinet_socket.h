#pragma once

#include "lokinet_context.h"
#include "lokinet_os.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /// poll many sockets for activity
    /// each pollfd.fd should be set to the socket id
    /// returns 0 on sucess
    int EXPORT lokinet_poll(struct pollfd* poll, nfds_t numsockets, struct lokinet_context* ctx);

    /// close a udp socket or a stream socket by its id
    void EXPORT lokinet_close_socket(int id, struct lokinet_context* ctx);

#ifdef __cplusplus
}
#endif
