#pragma once

#include <llarp/util/aligned.hpp>

namespace llarp
{

    inline constexpr size_t HOPID_SIZE = 16;

    struct HopID final : public AlignedBuffer<HOPID_SIZE>
    {
        using AlignedBuffer<HOPID_SIZE>::AlignedBuffer;

        static HopID make_random();
    };

}  // namespace llarp

template <>
struct std::hash<llarp::HopID> : hash<llarp::AlignedBuffer<llarp::HopID::SIZE>>
{};
