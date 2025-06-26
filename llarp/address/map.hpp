#pragma once

#include "address.hpp"

#include <llarp/util/formattable.hpp>
#include <llarp/util/thread/threading.hpp>

namespace llarp
{
    template <typename LocalAddrT>
    struct address_map
    {
      protected:
        std::unordered_map<LocalAddrT, NetworkAddress> _local_to_remote;
        std::unordered_map<NetworkAddress, LocalAddrT> _remote_to_local;
        std::unordered_map<std::string, NetworkAddress> _name_to_remote;

        using Lock_t = util::NullLock;
        mutable util::NullMutex addr_mutex;

      public:
        /** This functions exactly as std::unordered_map's ::insert_or_assign method. If a key equivalent
            to `local` or `remote` already exists, then they will be assigned to the corresponding value.
            Otherwise, the values will be inserted.

            The returned `bool` is true if the insertion took place and `false` if assignment occurred.
        */
        bool insert_or_assign(const LocalAddrT& local, const NetworkAddress& remote)
        {
            Lock_t l{addr_mutex};

            auto [_1, ins1] = _local_to_remote.insert_or_assign(local, remote);
            auto [_2, ins2] = _remote_to_local.insert_or_assign(remote, local);
            auto [_3, ins3] = _name_to_remote.insert_or_assign(remote.name(), remote);

            return ins1 && ins2 && ins3;
        }

        std::optional<NetworkAddress> get_remote(const LocalAddrT& local) const
        {
            Lock_t l{addr_mutex};

            if (auto itr = _local_to_remote.find(local); itr != _local_to_remote.end())
                return itr->second;
            return std::nullopt;
        }

        std::optional<NetworkAddress> get_remote(const std::string& name) const
        {
            Lock_t l{addr_mutex};

            if (auto itr = _name_to_remote.find(name); itr != _name_to_remote.end())
                return itr->second;
            return std::nullopt;
        }
        std::optional<NetworkAddress> operator[](const LocalAddrT& local) const { return get_remote(local); }

        std::optional<LocalAddrT> get_local(const NetworkAddress& remote) const
        {
            Lock_t l{addr_mutex};

            if (auto itr = _remote_to_local.find(remote); itr != _remote_to_local.end())
                return itr->second;
            return std::nullopt;
        }
        std::optional<LocalAddrT> operator[](const NetworkAddress& remote) const { return get_local(remote); }

        std::optional<LocalAddrT> get_local(const std::string& name) const
        {
            Lock_t l{addr_mutex};

            if (auto itr = _name_to_remote.find(name); itr != _name_to_remote.end())
                return get_local(itr->second);
            return std::nullopt;
        }

        bool has_local(const LocalAddrT& local) const
        {
            Lock_t l{addr_mutex};

            return _local_to_remote.contains(local);
        }

        bool has_remote(const NetworkAddress& remote) const
        {
            Lock_t l{addr_mutex};

            return _remote_to_local.contains(remote);
        }

        void unmap(const NetworkAddress& remote)
        {
            Lock_t l{addr_mutex};

            if (auto it = _remote_to_local.find(remote); it != _remote_to_local.end())
            {
                _local_to_remote.erase(it->second);
                _remote_to_local.erase(it);
            }
            _name_to_remote.erase(remote.name());
        }

        void unmap(const LocalAddrT& local)
        {
            Lock_t l{addr_mutex};

            if (auto it_a = _local_to_remote.find(local); it_a != _local_to_remote.end())
            {
                if (auto it_b = _remote_to_local.find(it_a->second); it_b != _remote_to_local.end())
                {
                    _name_to_remote.erase(it_b->first.name());
                    _remote_to_local.erase(it_b);
                }
                _local_to_remote.erase(it_a);
            }
        }

        void unmap(const std::string& name)
        {
            Lock_t l{addr_mutex};

            if (auto it_a = _name_to_remote.find(name); it_a != _name_to_remote.end())
            {
                if (auto it_b = _remote_to_local.find(it_a->second); it_b != _remote_to_local.end())
                {
                    _local_to_remote.erase(it_b->second);
                    _remote_to_local.erase(it_b);
                }
                _name_to_remote.erase(it_a);
            }
        }
    };
}  //  namespace llarp
