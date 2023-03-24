#pragma once
#include <trompeloeil.hpp>
#include <llarp/layers/flow/flow_layer.hpp>


namespace llarp::layers::flow
{
    class FlowLayerMock : public FlowLayer
    {
    public:
        MAKE_MOCK1(offer_flow_traffic, void(FlowTraffic&&), override);
    };
}
