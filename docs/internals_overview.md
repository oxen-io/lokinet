# preface

this document is a hybrid of documentation of the current lokinet protocol (at a high level and implementation detail level), the current issues of it (at both high level and implementation itself) and the desired outcome to be reached during a refactor.

## Glossary of Terms

Viewing lokinet through the lens of a traditional client-server model can introduce ambiguity. 
As such, any interactive sitaution is **always** referred to as being between an initiator and recipient instead.

An initiator is the entity that starts a bidirectional session or some kind of transactional interaction back and forth.
the recipient is the entity receiving this initiation from the initiator. 

if we were to use client-server architecture terminology:

* clients are always initiators and never are the recipient.
* Servers are able to be the initiator and a recipient.

If a server connects to a server this is a linguistic ambiguity that **MUST** be resolved to properly disambiguiate clients connecting to a server and a server connecting to a server as these are a fundamentally different type of interaction.

so when referring to alice (A) and bob (B):

* alice is **always** the initiator.
* bob is **always** the recipient.

---

# current state of the lokinet protocol and its implementation

this section documents the existing state of lokinet conceptually and on the implementation detail level.


## current high level conceptual overview of lokinet

lokinet at this time has a layered structure similar to the OSI stack. 

we have the following layers:

* link layer
* onion layer
* routing layer
* flow layer 

### Link Layer
<!--
XXX: undefined terms:
- router contact
- router/relay & client
- (non-)durable
- hop
- link-layer frame (referred to in the current impl as "message")
-->

The link layer is responsible for transmission of durable and non-durable messages between nodes.
This protocol must be able to handle transmission of link-layer frames larger than the MTU of users' internet connections. 
This means any protocol fulfilling this role requires fragmentation and durable delivery notification.
Link layer sessions are stateful node to node transacations where 2 nodes exchange link-layer frames in a durable delivery fashion where order of receival is NOT enforced by the recipient but order of transmission is enforced to the best of the sender's ability.
The link layer protocol dialect is chosen by the initiator of a link layer session from the list of dialects in the recipient's Router Contact.
UDP is chosen to encapsulate the link layer protocol's traffic; TCP requires strict ordering of delivery and we do not want that.
As a side-effect of link layer congestion, recipients can receive link-layer frames out of order if one or more fragments of a frame had to be re-transmitted.
The fact that frames were received out of order is useful, as higher layers can infer congestion and act accordingly without needing to be explicitly informed of congestion.
For example, when a TCP stack receives segments out of order it will tell the other end of the connection to slow down (backoff).


### Onion Layer

the onion layer is responsible for the actual stateful onion routing of the upper layer's messages (routing layer messages). it provides an opauqe transport for these messages with no assurance of durable delivery,order or authentication of data sent.
it is a "dumb" layer who ONLY anonymizes the origin of routing layer messages being sent to a lokinet router who is known publically (publishes an RC to the network and is accepted by consensus if it enabled on the network that is running like on staked mainnet). 
the creator of an anonymizing path on this layer can be either a lokinet client or lokinet serivce node as there is no way to enforce clients can only do this with the current cryptographic setup in use.
this layer MUST NOT add or remove metadata on traffic it carries that is non authenticatable from the perspective of the path creator (right now we cannot do that so we do not do that).

paths are bidirectional channels of potentually lossy communication that anonymize the origin of messages. 

the onion layer paths have 4 kinds of nodes involved:

* creator
* edge
* transit
* endpoint

#### path creator

this is the entity that selects the route this path will take, owns all ephemeral encryption keys to do symmetric encryption at all hops, and controls the lifetime of the path durring construction and optionally can in the future tear down a path they own prematurely (currently possible in the current path build format but unimplemented in all roles involved).
path creators can send upstream relay data messages (a kind of link layer message) down their paths that will decrypt one round at each hop resulting in the endpoint of the path to decrypt and get plaintext which is interpreted as a routing layer message.
upstream messages sent by the path creator are encrypted one round per hop with that hop's ehpehermally derived symmetric secret that was established during path build process.
path creators can recieve downstream ralay messages (a kind of link layer message) from the other end of the path (the path's endpoint) and then apply the rounds of decryption to get the plaintext sent to them.

#### path edge 

this is a specialized tranist node that is the first hop in an onion path (referred to as the "guard relay" in tor for those who are used to tor's nomeclature).
edge nodes are in an elevated position of trust from the perspective of the path creator as a mallicious edge can sielently drop all path builds or relay traffic regardles of direction and there is (currently) now way for the path creator to know that it is the edge doing it. 
additionally edges have the ability to know if a path creator is a lokinet client or not and to know their "real ip".

#### path transit 

this role is a non endpoint node in the path, it does one operation of symmetric encryption and sends data to the next hop given the direction it is going in.

#### path endpoint 

an endpoint node is the last transit hop in the onion path, its role is to process the routing message that has been sent to them by the anonymous path creator.
when we are carrying .loki traffic it is referred to as the pivot router.
when we are carrying .snode traffic it is referred to as the endpoint router.

### routing layer

the routing layer is is responsible for routing messages sent inside of onion layer paths to another destination be it another onion path (regardless of which router it is on) or a lokinet router (regardless of if it is the router the onion path's pivot router)
right now the destination is always another onion path on the same router (for .loki traffic) or the lokinet router itself (for .snode traffic to them)
this is at the moment a future proofing pseudo layer that can be expanded to accomidate more complex traffic routing like non trivial unicast traffic or any-source multicast traffic.

### flow layer

this layer is responsbile for sending traffic between .loki addressable destinations on lokinet.
the flow layer is encapsulated inside routing layer messages, anonymized by onion layer paths on each end.

an iniatiator will establish a flow to a recipiant, both ends not know each other's "real ip" but instead knowing the other's .loki address.

#### initiator life cycle

the initiator of a flow, will do the following in order:

* dht location calcuation 
* querying for introset
* selection of intro
* path building for selected intro
* flow negotiation
* transmission and handover


##### location calculation
<!--
// todo: explain introsets elsewhere
// todo: explain intros elsewhere
-->
determine where in dht hashring keyspace (L) is closest to find the recipiant's introset.
keyspace location is calcuated as the N routers who's identity key is closest to the blinded subkey used to sign the introset.
both the blinded subkey and router's identity keys are treated as dht locations as they are the same size
distance between dht locations is computed as XOR of those 2 dht locations.

##### querying dht

the initator will query a router in their current set of paths they have already built who's endpoint is closest to the previously derived location in keyspace. 
closenes is also calculated as the XOR between 2 dht locations.
queries when recieved by the lokinet router will proxy the query to the correct routers if they do not meet the requirements of dht keyspace locality that was assumed to be there by the sender of the query.
timeouts may occur during querying. 
there is no well defined resolution for timeouts at this time.

##### selection of intros

the initiator once getting the introset of the recipiant from querying the dht will then (after verifying and decrypting it into a plaintext form), select 1 intro and build an onion path to that router.
this selection process is not currently rigorusly defined and has no garuanteed constraints imposed on it.

##### path building for selected intro

given the intro selected, the initiator will build a path who's path endpoint is specified in the intro.
at this point the high level responsiblities start to blur into ambiguity between each other as it becomes increasingly intertwined insude implementation details of the remaining steps.

##### flow negotiation

the initator will use a oneshot protocol to establish a new bidirectional flow themself and the recipiant.
the recipiant will either accept or reject this message.
the message can drop at any time in any direction.
there is no well defined resolution for drop on either side.
the initiator will bundle any applicable authentication codes in this one shot messsage they sent to the recipiant.
this process uses a 2 phase key exchange to derive a shared secret for data transmission.
the one shot message's payload is encrypted and can be read only after the first phase of the keyexchange is done on the recipiant's end.
the recipaint validates the plaintext payload against the outer data, then computes the second phase of the key exchange, sends a reply with the result of the first phase.
the initiator receieves a reply encrypted using the first phase and the second phase of the key exchange happens on the recipiant's end and the resulting result is now know by both parties.

##### transmssion and handover

the initiator after establishing a flow and being accpted by the recpiant will then send and recieve data over this flow.
this flow has an indefinite lifetime given it has been used, this means that the onion path on each side of this flow will change.
this change is determinstic except when it's not in case of unknowable failures in either end's onion paths.
times out in transmission and handover is not well defined and has no rigorous paramters imposed on them.

#### recipiant life cycle

the recipiant of a flow, will do the following things in order

* reply to flow negeotation requests, reject or accept
* transmission on accepted flows

##### reply to flow negotiaton requests

the recipiant when getting a new flow establishment will either accept or reject this request for a new flow.
the result will be sent as a reply in the same kind of encapsulation as the request back down the same path it came from using additional information to route it back to the correct path it originated from on the pivot router.
authentication codes if provided by the initiator will be used to determine if this new flow will be accepted or rejected.

##### transmission of accepted flows

we get traffic on recipiant flows and sent it to the os.
for any os traffic for .loki addresses where we are a recipaint on the flow we will reuse the "best" active flow.
there is no rigorus definition for what constitutes a "best" or "active" flow at this time.
as a recipaint we are not responsible for reviving inactive flows, any traffic we get from the os for a .loki address were we are a recipaint flow will not trigger us to become an initiator to deliver that traffic.
the thresholds of how long we remember preivous historical recipiant flows is not well defined.

## here is what exists (concrete/code components) and which component does what of the above

the current implementation of lokinet is written in a bizare combination of modern C++ and monero tier C++.

the project structure is as follows:

* `/daemon` (holds all the main functions for executables)
* `/include` (public headers)
* `/llarp` (lokinet itself)


### /daemon

we have 3 primary executables:

* `lokinet.cpp` (lokinet for windows and linux)
* `lokinet.swift` (lokinet for macos)
* `lokinet-vpn.cpp` (lokinet rpc tool that controls exit node mappings)

### /include

this contains all of the C headers for embedded lokinet api.
if there is a public C++ api headers would go here.

### /llarp

this is where ALL of lokinet's implementation lives

there are a few areas that contain sort of related code:

* `/llarp/net` a dumping ground for "network" related code, a mix of platform dependant and platform indepedant code.
* `/llarp/vpn` a place where each platform's VPN code tries to live right now.
* `/llarp/ev` a place for the eventloop code, currently with one implementation, libuv. if a wasm port was to be made its eventloop would exist here.
* `/llarp/config` config loading, config parsing, config defaults, all things config related goes here.
* `/llarp/util` general dumping ground for misc helpers and such.
* `/llarp/dns` dns parsing library fused with assorted interfaces and their implementations for the dns subsystem.
* `/llarp/rpc` zmq api for oxend, lokinet rpc and endpoint(exit) auth rpc.

platform specific parts have been slowly isolated into their distinct locations:

* `/llarp/win32` most of the windows only code, omitting some of the vpn api stuff.
* `/llarp/android` android compat shims (`/jni` contains the android vpn api bindings for jni).
* `/llarp/apple` as much as the apple specific stuff.
* `/llarp/linux` right now contains linux only code like things for dbus.

finally, then there is `/llarp/router` which is home to a lot of our god objects and things that are owned by said god objects.

---

each of the layers in lokinet are implemented in the following areas of the code:

#### link layer

the link layer code exists in:

* `/llarp/iwp`
* `/llarp/link`
* `/llarp/messages`

iwp is our current wire protocol, that code is in `/llarp/iwp`.
the high level interface types exist in `/llarp/link` , sort of.
the link layer frame parser and each link layer frame kind is in `/llarp/messages`.

#### onion layer

the onion layer is mainly contained in:

* `/llarp/path`
* `/llarp/exit`
* `/llarp/service`

#### routing layer

the routing layer is present in:

* `/llarp/routing`
* `/llarp/exit`
* `/llarp/handlers`

### flow layer

the flow layer is present in:

* `/llarp/path`
* `/llarp/handlers`
* `/llarp/exit`
* `/llarp/service`

# problems in the current state of lokinet 

there are a few issues in the high level conceptual layout of lokinet.

## conceptual problems

there most major issues are in the link layer and the flow layer.

### Link Layer

the link layer has side effects that bubble up to upper layers lokinet has no ability to be aware of.
the congestion strategy in lokinet happens as a coincidental side effect in the implementation detail of how link layer frames are transported.
elevated packet loss causes retransmissions which will delay traffic to the next hop. 
this behavior is desirable but because it is done at the link layer and not the onion layer, it will affect all paths going across that node.
this behavior should be reimplemented on an upper level, propagating link layer session info upwards would help inform how and when to do this.

iwp is a homegrown protocol that nothing else speaks, it needs to be replaced.
DTLS + SCP would be a prime choice as it is what webrtc speaks, this would make a WASM port a possiblity.
QUIC would be great for durable transmission of messages but the capabilities of it to transmit lossy data is unclear at this time.

### Flow Layer

this layer needs to be redone at a high level in full so that the role it actually has is well defined, the timeout, limits and other constraints of the protocol are well defined, the structure of state machines involved are well defined, the failure cases are well defined, the exceptional failures are well defined and the interaction with the lower layers is well defined.
there is no well defined anything in the flow layer as nothing in it was designed, it was organically piled together. 
this layer is responsible for a very large majority of issues in lokinet (likely > 70% of user facing issues).


## implementation problems

* this codebase currently comes with 2 ½ kitchen sinks pre-installed.
* there are not enough strict separation between the concerns of components.
* there is an over dependance on inheritence in dubious and ambigous ways.
* there are home grown protocols in use for link layer comms.
* the platform specific code is not well contained in their own domain.
* the rpc server.
* the dht subsystem.
* there is no internal documentation of any kind.

### kitchen sinks

* `llarp::Router` 
* the service endpoint amalgum

#### llarp::Router

the contextualized god object is supposed to be `llarp::Context` but well over 90% of the functionality is done in `llarp::Router` and the former provides a psuedo wrapper with a semi-stable C++ api. this sucks, alot. this needs to be split up and refactored.

#### the service endpoint amalgum

this is the 1 ½ kitchen sink, a signle giant component that strattles the `llarp::handlers` and `llarp::service` that is in charge of the flow layer.
none of the flow layer was designed before hand and this code has had 2 or 3 partial failed refactors laying inside of it.
this code is the result of an additive organic dev process with zero design, foresight or rigor. 
it is a jumbled clusterfuck of obtuse partially completed components that merge into itself, fractally, with ambiguous names of functions, types and methods, with no useful documentation of any kind. (minus occational comments peppered with explitives).
did i mention it is responsible for generating at least 70% of the bugs users encounter?

### seperation of components

origianlly the intent was to have a straight C api for each componenet that would force each to have a super strict well defined relationship with other components.
this is no longer the case and there has been a slow continous drift into one big componenet.
recently as a small refactor has started we renamed this pile to be called `lokinet-amalgum` to mark this behavior.
other components were partitioned out of `lokinet-amalgum` during this small refactor in preparation for a refactor of a greater scope.

### inheritence of dubious ambiguity

there is a non zero number of types with names that are almost indistinguishable from others. (e.g. `Context`, `Handler`).
these types have an innapropriate level of inheritence depth, it goes up to 5 levels of it. i think.

### home grown protocols in link layer

we use an idotic home grown protocol called IWP (invisible wire protocol).
IWP is, at a high level, functionally identitical to SCTP over UDP+DTLS. 
IWP has no resemblence to SCTP over UDP+DTLS what so ever in the implementation details.
IWP is buggy, poorly designed, and poorly implemented.

### platform specific code not well contained

There are multiple inconsistent and incompatible ideoms for isolating platform sepcific code.

* use cmake to do private linkage for platform specific bits into its own library (preferred)
* defined a high level interface type that each platform implements (preferred)
* use preprocessor macros to isolate per compilation unit.
* use preprocessor macros to isolate in the headers.
* do not isolate platform specific code at all.

the last 3 ideoms are artifacts of early days of dev. (this is a bad excuse)

### the rpc server

the rpc server has 5 levels deep nested mutable lambas that capture each other.

### the dht subsystem

the dht subsystem is overly complex and has no reason or rhym on why it should. 
i dont actually know how it all works anymore.

### no documentation

the reason this file even exists should inform you on what this section's content should be.

