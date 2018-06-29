#include "tuntap++.hh"
#include <boost/python.hpp>

BOOST_PYTHON_MODULE(_pytuntap)
{
    using namespace tuntap;
    using namespace boost::python;

    std::string (tap::*tap_get_name)() const                = &tap::name;
    void        (tap::*tap_set_name)(std::string const &)   = &tap::name;
    std::string (tap::*tap_get_hwaddr)() const              = &tap::hwaddr;
    void        (tap::*tap_set_hwaddr)(std::string const &) = &tap::hwaddr;
    int         (tap::*tap_get_mtu)() const                 = &tap::mtu;
    void        (tap::*tap_set_mtu)(int)                    = &tap::mtu;

    std::string (tun::*tun_get_name)() const                = &tun::name;
    void        (tun::*tun_set_name)(std::string const &)   = &tun::name;
    int         (tun::*tun_get_mtu)() const                 = &tun::mtu;
    void        (tun::*tun_set_mtu)(int)                    = &tun::mtu;

    def("tuntap_version", tuntap_version);

    class_<tun, boost::noncopyable>("Tun", init<>())
        .def("release", &tun::release)
        .def("up", &tun::up)
        .def("down", &tun::down)
        .def("ip", &tun::ip)
        .def("nonblocking", &tun::nonblocking)
        .add_property("name", tun_get_name, tun_set_name)
        .add_property("mtu", tun_get_mtu, tun_set_mtu)
        .add_property("native_handle", &tun::native_handle)
    ;
    class_<tap, boost::noncopyable>("Tap", init<>())
        .def("release", &tap::release)
        .def("up", &tap::up)
        .def("down", &tap::down)
        .def("ip", &tap::ip)
        .def("nonblocking", &tap::nonblocking)
        .add_property("name", tap_get_name, tap_set_name)
        .add_property("hwaddr", tap_set_hwaddr, tap_set_hwaddr)
        .add_property("mtu", tap_set_mtu, tap_set_mtu)
        .add_property("native_handle", &tap::native_handle)
    ;
}
