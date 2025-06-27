#pragma once

#include <llarp/crypto/constants.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/buffer.hpp>

namespace llarp
{
    struct PubKey : public AlignedBuffer<PUBKEYSIZE>
    {
        using AlignedBuffer<PUBKEYSIZE>::AlignedBuffer;

        bool from_hex(const std::string& str);

        std::string to_string() const;

        // FIXME TODO revisit this
        PubKey& operator=(const uint8_t* ptr);
    };
}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::PubKey> : public hash<llarp::AlignedBuffer<PUBKEYSIZE>>
    {};
}  //  namespace std
