this directory contains the meat of the lokinet core implemenation

components:

[apple platform specific bits](apple)

[configuration bits](config)

[network consensus parts](consensus)

[compile time constants](constants)

[cryptography interface and implementations](crypto)

[DHT related code](dht)

[DNS client/server/parsing library](dns)

[event loop interface and implementations](ev)

[snode endpoint backend implementation](exit)

[onion endpoint frontend adapters](handlers)

[lokinet wire protocol](iwp)

[lokinet link layer interface types](link)

[link layer (node to node) messages](messages)

[platform agnostic-ish network api](net)

[onion path management](path)

[peer stats collection (likely to be refactored)](peerstats)

[quic over lokinet abstraction](quic)

[central god objects](router)

[routing (onion routed over paths) messages](routing)

[rpc client/server](rpc)

[onion endpoint backend implemenation](service)
    
[net simulation code (likely to be binned)](simulation)

[net simulation tooling (used by pybind)](tooling)

[utility function dumping ground (to be fixed up)](util)

[platform specific vpn bits with high level abstractions](vpn)

[windows platform bits](win32)
