# DNS in Lokinet

Lokinet uses dns are its primary interface for resolving, mapping and querying resources inside of lokinet.
This was done not because DNS is *good* protocol, but because there is almost no relevent userland applications that are incapable of interacting with DNS, across every platform.
Using DNS in lokinet allows for the most zero config setup possible with the current set of standard protocols.

Lokinet provides 2 internal gtld, `.loki` and `.snode`

## .snode

The `.snode` gtld is used to address a lokinet router in the form of `<zbase32 encoded public ed25519 identity key>.snode`.
Traffic bound to a `.snode` tld will have its source authenticatable only if it originates from another valid lokinet router.
Clients can also send traffic to and from addresses mapped to `.snode` addresses, but the source address on the service node side is ephemeral.
In both cases, ip traffic to addresses mapped to `.snode` addresses will have the destination ip rewritten by the lokinet router to be its local interface ip, this ensures traffic stays on the lokinet router' interface for snode traffic and preventing usage as an exit node.

## .loki

The `.loki` gtld is used to address anonymously published routes to lokinet clients on the network.

<!-- (todo: keyblinding info) -->

## What RR are provided?

All `.loki` domains by default have the following dns rr synthesized by lokinet:

* `A` record for initiating address mapping
* `MX` record pointing to the synthesizesd `A` record
* free wildcard entries for all of the above.

Wildard entries are currently only pointing

All `.snode` domains have by defult just an `A` record for initiating address mapping.

Additionally both `.loki` and `.snode` can optionally provide multiple `SRV` records to advertise existence of services on or off of the name.

<!-- (//todo: document and verify srv record limitations) -->
