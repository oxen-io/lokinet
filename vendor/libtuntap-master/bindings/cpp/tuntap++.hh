#pragma once
#ifndef LIBTUNTAP_ALY0MA60
#define LIBTUNTAP_ALY0MA60

#include <string>

#include <tuntap.h>

namespace tuntap {

class tun
{
 public:
  tun();
  ~tun();
  tun(tun const &) = delete;
  tun & operator = (tun const &) = delete;
  tun(tun &&);

  // Properties
  std::string name() const;
  void name(std::string const &);
  int mtu() const ;
  void mtu(int);
  t_tun native_handle() const;

  // Network
  void up();
  void down();
  void ip(std::string const &presentation, int netmask);

  // System
  void release();
  void nonblocking(bool);
 private:
  struct device* _dev;
};

class tap
{
 public:
  tap();
  ~tap();
  tap(tap const &) = delete;
  tap & operator = (tap const &) = delete;
  tap(tap &&);

  // Properties
  std::string name() const;
  void name(std::string const &);
  std::string hwaddr() const;
  void hwaddr(std::string const &);
  int mtu() const;
  void mtu(int);
  t_tun native_handle() const;

  // Network
  void up();
  void down();
  void ip(std::string const &presentation, int netmask);

  // System
  void release();
  void nonblocking(bool);
 private:
  struct device* _dev;
};

} /* tuntap */


#endif /* end of include guard: LIBTUNTAP_ALY0MA60 */
