# "liblokinet" TCP-over-QUIC

In order for lokinet to work in an embedded version (which I will call "liblokinet" in this
document), which lokinet cannot create TUN device (either because the host OS doesn't support them,
or because lokinet needs to run without permissions to manage them) lokinet needs a solution for
sending TCP data from the device to a remote lokinet client (i.e. a snapp, a snode, or another
liblokinet client).  Since the vast majority of network connectivity relies on TCP stream
connections, not supporting them would be a severe limitation of a lokinet library that would make
it nearly useless.

Traditional "full" lokinet does not need to solve this problem: it creates virtual IPs on the TUN
interface that map to every looked-up `.loki` address and then the host system's in-kernel TCP layer
handles the intricacies of TCP including acknowledgement, retry, and so on.  While there are
user-space TCP implementations available, they are generally incomplete, unmaintained, or both,
which would mean substantial work and ongoing maintenance for us to adopt or reimplement such a
user-space TCP layer, for which we would most likely be the only user and contributor.

Instead this proposal is for lokinet to support a tunneled TCP stream mode where TCP traffic is
carried over lokinet via a subset of the
[QUIC](https://datatracker.ietf.org/doc/draft-ietf-quic-transport/) protocol.  Unlike TCP, QUIC has
several well-maintained user-space implementations which allow us to use, rather than create, a
well-maintained QUIC implementation.

## Overview

The high-level strategy of how we handle such a stream connection is to have TCP connections
established only within the local device.  A liblokinet application would invoke a lokinet call to
establish such a connection to proxy to a remote host by lokinet name and TCP port.  This would
first establish a lokinet connection to the remote host, then open a QUIC connection over it and
start listening for TCP connections on a local port.  When a new TCP connection is established on
this port lokinet will establish a new QUIC stream over the existing connection, specifying the
destination port while initializing the stream.  (The client is free to establish as many TCP
connections as it wants: each one becomes a separate QUIC stream).

The situation is similar for the receiving lokinet client: it would listen for incoming QUIC
connections on the local lokinet IP and, when establishing a QUIC stream, would establish a local
TCP connection to the requested port on the lokinet IP.  Any incoming stream data is then forwarded
into this TCP connection, and any responses are sent back via the QUIC stream.

## Example

For example, suppose `snap7.loki` is a lokinet snapp with a web server listening on port 80 and a
liblokinet client `omg42.loki` wants to connect to it to retrieve a cat photo.  With a full
lokinet client, the DNS request for `omg56789.loki` triggers creation of a virtual IP on the TUN
device, returns the IP to the system, and any TCP packets sent to this IP are forwarded to the
primary lokinet IP of `azfoj123.loki`, where an HTTP server is ready and waiting to provide cat
photos.

With a liblokinet client, this process will looks a little different: the client will first make a
call to the liblokinet library (rather than a DNS request) specifying the lokinet host name and TCP
port it wants to connect to (note that this is pseudo-code; the actual implementation calls will
have to deal with various details such as connection delays and timeouts that are omitted here):

    result = lokinet_stream_connect(lokinet_addr, port)
    if result->connection_established:
        http_get("http://" + result->local_address + ":" + result->local_port + "/cat.jpg")

Here `http_get` would need no knowledge of lokinet at all: it will simply connect via TCP to an
address such as `127.0.0.1:4716` for the HTTP request.  It will send the request, and receive it,
over this localhost TCP socket.

Internally, lokinet will have established a QUIC connection to the remote host, and started
listening for TCP connections on the localhost port.  When `http_get` establishes a TCP connection
on this local port it will create a QUIC stream on the established QUIC connection and forward all
stream data received from the TCP connection into the QUIC stream, and any data that comes back over
the QUIC stream will similarly be copied into the localhost TCP connection.

Effectively the data path of data send from the app on omg42.loki to the HTTP snapp on omg42.loki
looks like this:

    ┌omg42.loki────────────┐                             ┌snap7.loki───────────┐
    │ Main app thread      │                             │ HTTP                │
    │ TCP localhost:4567 ─>│─┐                           │ TCP 172.16.0.1:80 <─│─┐
    ├──────────────────────┤ │                           ╞═════════════════════╡ │
    │ liblokinet (in app)  │ │                           │ lokinet (on host)   │ │
    │ TCP localhost:4567 <─│─┘                         ┌>│─> QUIC UDP          │ │
    │           QUIC UDP ─>│───... Lokinet routers ...─┘ │ TCP 172.16.0.1:80 ─>│─┘
    └──────────────────────┘                             └─────────────────────┘

(These connections are all bi-direction, so any TCP stream data replied from omg42.loki follows the
same path in reverse.)

## Implementation details/notes

Implementation library: `ngtcp2` is a robust, maintained library that fits our needs well.

### Not a general QUIC server/client

The QUIC tunnel described here is *only* for Lokinet TCP streams; it is not intended to be
interoperable with general QUIC clients, which allows us some leeway to not support some aspects of
QUIC that are of no advantage over a lokinet conversation.

### No encryption

Since lokinet traffic is itself encrypted and private, the built-in TLS layers of QUIC are something
that we don't need or want.  Thus the QUIC implementation used will simply use no-op encryption to
pass data and avoid/ignore certificates.  (`ngtcp2`, in particular, allows pluggable authentication
to allow this).

### No address verification

QUIC recommends address verification (among other things, to avoid amplification attacks).  Lokinet
connections already provide this and so we can safely not use it.

### Stream establishing

Establishing a QUIC stream requires sending additional information during connection: namely the
target connection port.  Thus establishing a new stream will require some additional data to be
passed, likely as the initial stream data.  (Specification of how this data is to be encoded is not
yet specified).

### Incoming TCP-over-QUIC connections

Handling of incoming connections to a liblokinet client will require a similar process, but in
reverse:

- the client starts listening on a localhost TCP port
- the client makes a call to liblokinet to inform it of this available listening port
- incoming QUIC tunneled streams attempting to connect to that registered port are accepted and
  establish a new TCP connection as long as the stream stays open; data is forwarded between the two
  connections.
- Should the client require end-point verification liblokinet will provide a function that can look
  up the remote lokinet address based on the source port of the TCP connection.  (This is different
  from but analogous to a snapp doing a reverse DNS lookup on the source address to determine the
  remote address).

Note: to be externally reachable by other lokinet clients, a liblokinet client would have to publish
an introset; this introset would also include an additional flag indicating that TCP connections
must be tunneled through a TCP-over-QUIC connection.

Note 2: we additionally may want to signal during connection that new TCP connections back to us
should be done over a QUIC tunnel, which requires also adding a flag when establishing the
lokinet conversation.

### Non-tunneled incoming TCP connections

Without a controllable TCP stack we have no ability to accept these, however since the introset (and
conversation initiation) indicates that TCP should be tunneled, we should just drop these packets.

## Lokinet implementation notes

### Outbound connections -- liblokinet

The application makes a liblokinet library call such as

    lokinet_stream_result res;
    lokinet_outbound_stream(&res, "some-snapp.loki", 2345);

This initiates an outbound connection to the given lokinet remote, asking to connect to port 2345 on
the remote.  Plainquic begins listening on a random localhost port, and returns this via an entry in
`res`.  New connections establishes to this localhost port initiate new streams on the quic
connection which are tunneled to the remote end.

### Inbound connections -- liblokinet

The application needs to start listening on one or more TCP ports (e.g. on localhost, but doesn't
have to be) and then registers a callback with lokinet about the availability of this port for
incoming plainquic connections by setting up a callback:

```C
    int accept_inbound(const char *lokinet_addr, uint16_t port, sockaddr *addr, void *context) {
        // lokinet_addr is the remote lokinet client trying to establish a stream
        // port is the port they are trying to reach
        // If the client is allowed then set the local TCP socket address that the tunnel should
        // connect to in `addr` (which is big enough to allow either sockaddr_in or sockaddr_in6)
        sockaddr_in* a = (sockaddr_in*)addr;
        a->sin_family = AF_INET;
        a->sin_addr = INADDR_LOOPBACK;
        a->sin_port = htons(5678); // NB: Doesn't have to be the passed-in `port`
        return 0;
        // If this callback doesn't handle the requested port (will try other callbacks):
        return -1;
        // If this callback does handle it and the connection should be refused:
        return -2;
        // (Return values other than 0/-1/-2 are reserved and should not be used).
    }
    lokinet_inbound_stream(&accept_inbound, NULL /*context*/);
```
or, for the very simple case where connections should be available on some localhost port:
```C
    // All incoming tunneled connections for port 5678 should go to localhost:5678
    lokinet_inbound_stream_simple(5678);
```

When a new plainquic connection arrives, if such a callback has been registered it will be called to
determine whether the connection should be accepted and, if it is, where streams opened on that
connection should be sent.  (For the simple version, all inbound connections on port 5678 would be
accepted and would be forwarded to localhost:5678; inbound connections for other ports would be
refused).

Each new plainquic stream initiated by the remote connection then establishes a new TCP connection
to the IP/port set by the callback.

### Outbound connections - full lokinet

When attempting to connect to a client who has indicated in its introset that it requires plainquic
connections then plainquic will bind to and listen on the virtual (tun) TCP/IP port and establish a
plainquic connection to the remote liblokinet on the given port.  (Future connections to this port
will establish new streams on the existing connection).

Setting up the initial listener involves intercepting the initial TCP connection attempt (i.e. the
SYN packet), starting to listen on it while simultaneously initiating the plainquic connection over
lokinet.  It may work sufficiently well (investigation required) to simply drop this initial SYN
packet and let the initiator retry in a few moments to attack to the new listener which now goes
into the plainquic listener which establishes a new stream.

Thereafter the local application simply talks to this local listener and all stream data gets
tunneled over lokinet to the remote liblokinet.

### Inbound connections - full lokinet

This is fairly simple: when incoming quic-tunneled packets arrive we start up a plainquic server (if
not already running), deliver the packets into it, and it tunnels incoming stream data into TCP
connections to the primary lokinet IP (using the IP mapped to the lokinet endpoint as the source
address).


TODO:
- Add quic protocol type to llarp/service/protocol_types.hpp
- Convert stuff in plainquic code to use lokinet structures (e.g. logging, address encapsulation)
- Add handler for QUIC packets to llarp/handlers/tun.cpp that see that protocol type and forward the
  packet off to the quic server to handle.
- Get at the uvw event loop from the quic code so that we can put the plainquic stuff onto it rather
  than spinning up its own event loop.  I was thinking about something like:
  `virtual std::shared_ptr<void> get_uvw_loop() { return nullptr; }` in ev.h, and an override that
  returns the uvw event loop in the ev_libuv.h subclass (the type erasure through the shared_ptr<void>
  means ev.h doesn't have to depend on any uvw.h headers).  Then the quic code can just do something
  like:
    auto uv_loop = std::static_pointer_cast<uvw::Loop>(ev->get_uvw_loop());
    if (not uv_loop) { die("horribly"); }
- convert the crap in the `main` functions copied from plainquic test code to exposed library calls.
- decide whether we start up a quic server and/or client on demand, or just always start it.


Outgoing conns:
- Add "supported protocols" item to introset and (for liblokinet) leave off IPv4/v6 flags, but add
  quic protocol flag.

