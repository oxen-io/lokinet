# refactor notes

## FLow Layer

The flow layer needs to be refactored in full, it has to be seperated into smaller more defined components.

This refactor has the following "phases"

1. review relations
---

* list the current components inside the flow layer
* list the responsibilities of each component
* list the relations between each component

---
2. modify relations
---

* depulicate aggregate responsibilities over all components
* re organize responsibilities into distinct "osi stack" like layers
* re structure components into layers such that they only talk to layers above and below

---
3. spec out interfaces
---

* given the new component relations define new compilation targets (liblokinet-whatever) and set up the high level deps
* in each component define interface types for each responsibility with their relations outside the component itself
* define internal component relations
* define unit tests for each responsibility in each component

---
4. refactor
---

* rearrange the code from the existing pile into the new structure
* add unit tests before moving code into place to ensure it does what i think it does


## Concrete Plan

---
1. define new interfaces and structure
---

* define new cmake libraries that split up lokinet-amalgum more
* define new interface types in headers in each new split up library

---
2. wire up old implementation to new interface
---

* add unit tests that can fail when wired up to old code
* wire up old code to new interface
* verify working state

---
3. add new implementation to new interface
---

* implement new interface so that unit tests pass with new implementation
* verify working state



### new interface types (1)

the component to refactor is `llarp::service::Context`

the types that will get a new interface type are:

`llarp::service::Context` -> `llarp::service::I_ServiceContext`

`llarp::service::Endpoint` -> `llarp::service::I_Endpoint`

`llarp::handlers` namespace will be deprecated and no longer publicly exposed

an amalgum interface for obtaining all platform specific details, like event loop, vpn platform, etc: `llarp::service::I_PlatformDetails`

`llarp::service::I_PlatformDetails` will be given to `llarp::service::I_ServiceContext` to handle accessing platform specifics,
this is used to rectify the `llarp::service::Endpoint` / `llarp::handlers::*Endpoint` split that existed before.

all other components will use the `llarp::service::I_ServiceContext` to get an instance of `llarp::service::I_Endpoint` given `(router_id, path_id)` tuple for traffic, or access/visit all owned instances.

the `llarp::service::I_Endpoint` interface will be called from `llarp::routing::IMessageHandler` implementations to bridge onion layer and flow layer and acts as the refactor's hard boundary on the high level.


### refactor boundary (1.5)

preface:

`llarp::routing::IMessageHandler` is responsible for going from onion to flow layer.

`llarp::path::IHopHandler` is responsible for going from flow to onion layer.

`llarp::path::Path` implements the `llarp::path::IHopHandler` interface.

`llarp::path::PathSet` implements the `llarp::routing::IMessageHandler` interface.

`llarp::path::PathSet` owns many `llarp::path::Path`, this association itself is held in `llarp::path::PathContext`.

`llarp::path::PathSet` it is the original component that was ambigious in responsibilities and scope.
it is the core pain point causing previous refactors to explode and fail to complete.
this is mainly due to `llarp::path::PathSet` implementing the `llarp::routing::IMessageHandler` interface, this makes the boundary between components start to blur.
**EVERY** other refactor was to refactor pathset, but all of them failed as it explodes in scope when you do that.
we will have `llarp::path::PathSet` be deprecated but will be retained as a consession to limit the scope of the refactor.

`llarp::routing::IMessageHandler` and below CAN NOT have their public interfaces modified at all.
this includes ALL `llarp::path::*` types.


### wire-up point (2)

the wire-up of the new interfaces is done in the `llarp::path::*` types.

when initializing lokinet, the new process for `llarp::service::Context` is replaced by the following:

* configure / setup `llarp::service::I_PlatformDetails`
* instantiate a `llarp::service::I_ServiceContext` with `llarp::service::I_PlatformDetails` and `llarp::config::Config`, throws on fail. this also will start anything it needs to async.

teardown just calls destructor of `llarp::service::I_ServiceContext`.


---

misc notes:

`llarp::service::Endpoint` contains:

* outbound context / send context (internal detail, MUST be removed after refactor is over)
* `llarp::routing::IMessageHandler` (MUST NOT change in any way)
* protocol message / frame types (may change, depending on what uses it externally to `llarp::service` types)

`llarp::handlers::TunEndpoint` contains:

* platform wire-ups from `llarp::service::Endpoint` "backend"
* ip allocation code
* configuration entry point in practice, but this is backwards as it is accessed via `llarp::service::Endpoint` and is a virtual method.
