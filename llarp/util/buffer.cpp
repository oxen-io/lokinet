#include "buffer.hpp"

#include <oxenc/endian.h>

#include <cstdarg>
#include <cstdio>

namespace
{
    template <typename UInt>
    bool put(llarp_buffer_t& buf, UInt i)
    {
        if (buf.size_left() < sizeof(UInt))
            return false;
        oxenc::write_host_as_big(i, buf.cur);
        buf.cur += sizeof(UInt);
        return true;
    }

    template <typename UInt>
    bool read(llarp_buffer_t& buf, UInt& i)
    {
        if (buf.size_left() < sizeof(UInt))
            return false;
        i = oxenc::load_big_to_host<UInt>(buf.cur);
        buf.cur += sizeof(UInt);
        return true;
    }

}  // namespace

bool llarp_buffer_t::put_uint16(uint16_t i) { return put(*this, i); }

bool llarp_buffer_t::put_uint64(uint64_t i) { return put(*this, i); }

bool llarp_buffer_t::put_uint32(uint32_t i) { return put(*this, i); }

bool llarp_buffer_t::read_uint16(uint16_t& i) { return read(*this, i); }

bool llarp_buffer_t::read_uint32(uint32_t& i) { return read(*this, i); }

bool llarp_buffer_t::read_uint64(uint64_t& i) { return read(*this, i); }

std::vector<uint8_t> llarp_buffer_t::copy() const
{
    std::vector<uint8_t> copy;
    copy.resize(sz);
    std::copy_n(base, sz, copy.data());

    return copy;
}
