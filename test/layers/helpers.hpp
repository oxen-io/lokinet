#pragma once
#include <llarp/layers/platform/platform_layer.hpp>
#include <llarp/layers/flow/flow_layer.hpp>

#include <llarp/router/router.hpp>
#include <memory>
#include "llarp/layers/flow/flow_addr.hpp"

namespace llarp::test
{
     
    struct LayerMock_Router : public Router
    {
    public:
        std::unique_ptr<const layers::Layers>
        create_layers() override;
    };
}  // namespace llarp::test

namespace
{

    llarp::layers::flow::FlowAddr
    make_random_flow_addr()
    {
        llarp::layers::flow::FlowAddr addr{};
        addr.Randomize();
        // todo: ed25519 clamp
        return addr; 
    }
    
}
