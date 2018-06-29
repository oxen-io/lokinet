#include "tuntap++.hh"

#include <iostream>
#include <string>
#include <algorithm>

namespace tuntap {

tun::tun()
    : _dev{tuntap_init()}
{
    tuntap_start(_dev, TUNTAP_MODE_TUNNEL, TUNTAP_ID_ANY);
}

tun::~tun()
{
    tuntap_destroy(_dev);
}

tun::tun(tun &&t)
    : _dev(nullptr)
{
    std::swap(t._dev, this->_dev);
}

void
tun::release()
{
    tuntap_release(_dev);
}

std::string
tun::name() const
{
    return tuntap_get_ifname(_dev);
}

void
tun::name(std::string const &s)
{
    tuntap_set_ifname(_dev, s.c_str());
}

t_tun
tun::native_handle() const
{
    return TUNTAP_GET_FD(this->_dev);
}

void
tun::up()
{
    tuntap_up(_dev);
}

void
tun::down()
{
    tuntap_down(_dev);
}

int
tun::mtu() const
{
    return tuntap_get_mtu(_dev);
}

void
tun::mtu(int m)
{
    tuntap_set_mtu(_dev, m);
}

void
tun::ip(std::string const &s, int netmask)
{
    tuntap_set_ip(_dev, s.c_str(), netmask);
}

void
tun::nonblocking(bool b)
{
    tuntap_set_nonblocking(_dev, int(b));
}

tap::tap()
    : _dev{tuntap_init()}
{
    tuntap_start(_dev, TUNTAP_MODE_ETHERNET, TUNTAP_ID_ANY);
}

tap::~tap()
{
    tuntap_destroy(_dev);
}

tap::tap(tap &&t)
    : _dev(nullptr)
{
    std::swap(t._dev, this->_dev);
}


void
tap::release()
{
    tuntap_release(_dev);
}

std::string
tap::name() const
{
    return tuntap_get_ifname(_dev);
}

void
tap::name(std::string const &s)
{
    tuntap_set_ifname(_dev, s.c_str());
}

std::string
tap::hwaddr() const
{
    return tuntap_get_hwaddr(_dev);
}

void
tap::hwaddr(std::string const &s)
{
    tuntap_set_hwaddr(_dev, s.c_str());
}

t_tun
tap::native_handle() const
{
    return TUNTAP_GET_FD(this->_dev);
}

void
tap::up()
{
    tuntap_up(_dev);
}

void
tap::down()
{
    tuntap_down(_dev);
}

int
tap::mtu() const
{
    return tuntap_get_mtu(_dev);
}

void
tap::mtu(int m)
{
    tuntap_set_mtu(_dev, m);
}

void
tap::ip(std::string const &s, int netmask)
{
    tuntap_set_ip(_dev, s.c_str(), netmask);
}

void
tap::nonblocking(bool b)
{
    tuntap_set_nonblocking(_dev, int(b));
}

} /* tuntap */
