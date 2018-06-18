#include "pathfinder.hpp"

namespace llarp
{

    void llarp_pathfinder_get_route(struct llarp_pathfinder_context* pathfinder) {
        // what thread do pathfinder run in?
    }
}

llarp_pathfinder_context::llarp_pathfinder_context(llarp_router *p_router, struct llarp_dht_context* p_dht)
{
    this->router = p_router;
    this->dht = p_dht;
}

extern "C" {
    struct llarp_pathfinder_context *
    llarp_pathfinder_context_new(struct llarp_router* router,
                                 struct llarp_dht_context* dht) {
        return new llarp_pathfinder_context(router, dht);
    }
    
    void
    llarp_pathfinder_context_free(struct llarp_pathfinder_context* ctx) {
        delete ctx;
    }
}
