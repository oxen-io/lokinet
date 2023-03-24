#include <catch2/catch.hpp>
#include <llarp/layers/platform/platform_layer.hpp>
#include <memory>
#include <optional>
#include "layers/helpers.hpp"

struct PlatformLayer_Fixture
{
    std::shared_ptr<llarp::test::LayerMock_Router> mock_router;
};


TEST_CASE("platform layer map remote address", "[layer:platform]")
{
    

}
