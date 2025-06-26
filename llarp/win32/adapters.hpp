#pragma once
#include <llarp/win32/exception.hpp>

#include <iphlpapi.h>

#include <concepts>

namespace llarp::win32
{

    /// visit all adapters (not addresses). windows serves net info per adapter unlink posix
    /// which gives a list of all distinct addresses.
    template <std::invocable<PIP_ADAPTER_ADDRESSES> Visitor>
    void iter_adapters(Visitor&& visit, int af = AF_UNSPEC)
    {
        ULONG err;
        ULONG sz = 15000;  // MS-recommended so that it "never fails", but often fails with a
                           // too large error.
        std::unique_ptr<uint8_t[]> ptr;
        PIP_ADAPTER_ADDRESSES addr;
        int tries = 0;
        do
        {
            ptr = std::make_unique<uint8_t[]>(sz);
            addr = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(ptr.get());
            err = GetAdaptersAddresses(af, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX, nullptr, addr, &sz);
        } while (err == ERROR_BUFFER_OVERFLOW and ++tries < 4);

        if (err != ERROR_SUCCESS)
            throw llarp::win32::error{err, "GetAdaptersAddresses()"};

        for (; addr; addr = addr->Next)
            visit(addr);
    }
}  // namespace llarp::win32
