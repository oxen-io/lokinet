#pragma once

// C-linkage wrappers for interacting with a lokinet context, so that we can call them from Swift
// code (which currently doesn't support C++ interoperability at all).

#ifdef __cplusplus
extern "C"
{
#endif

#include <unistd.h>
#include <sys/socket.h>

  /// C callback function for us to invoke when we need to write a packet
  typedef void(packet_writer_callback)(int af, const void* data, size_t size, void* ctx);

  /// C callback function to invoke once we are ready to start receiving packets
  typedef void(start_reading_callback)(void* ctx);

  /// C callback that bridges things into NSLog
  typedef void(ns_logger_callback)(const char* msg);

  /// Initializes a lokinet instance by initializing various objects and loading the configuration
  /// (if {config_dir}/lokinet.ini exists).  Does not actually start lokinet (call llarp_apple_start
  /// for that).
  ///
  /// Returns NULL if there was a problem initializing/loading the configuration, otherwise returns
  /// an opaque void pointer that should be passed into the other llarp_apple_* functions.
  ///
  /// \param logger a logger callback that we pass log messages to to relay them (i.e. via NSLog).
  ///
  /// \param config_dir the lokinet configuration directory where lokinet.ini can be and the various
  /// other lokinet state files go.
  ///
  /// \param default_bootstrap the path to the default bootstrap.signed included in installation,
  /// which will be used if no explicit bootstrap is set in the config file.
  ///
  /// \param ip - char buffer where we will write the primary tunnel IP address as a string such as
  /// "172.16.0.0".  Will write up to 16 characters (including the null terminator).  This will be
  /// the tunnel IP from the lokinet.ini, if it exists and specifies a range, otherwise we'll
  /// configure lokinet to use a currently-unused range and return that.
  ///
  /// \param netmask the tunnel netmask as a string such as "255.255.0.0".  Will write up to 16
  /// characters (including the null terminator).
  ///
  /// \param dns the DNS address that should be configured to query lokinet, as a string such as
  /// "172.16.0.1".  Will write up to 16 characters (including the null terminator).
  void*
  llarp_apple_init(
      ns_logger_callback ns_logger,
      const char* config_dir,
      const char* default_bootstrap,
      char* ip,
      char* netmask,
      char* dns);

  /// Starts the lokinet instance in a new thread.
  ///
  /// \param packet_writer C function callback that will be called when we need to write a packet to
  /// the packet tunnel.  Will be passed AF_INET or AF_INET6, a void pointer to the data, the size
  /// of the data in bytes, and the opaque callback_context pointer.
  ///
  /// \param start_reading C function callback that will be called when lokinet is setup and ready
  /// to start receiving packets from the packet tunnel.  This should set up the read handler to
  /// deliver packets via llarp_apple_incoming.  This is called with a single argument of the opaque
  /// callback_context pointer.
  ///
  /// \param callback_context Opaque pointer that is passed into the packet_writer and start_reading
  /// callback, intended to allow context to be passed through to the callbacks.  This code does
  /// nothing with this pointer aside from passing it through to callbacks.
  ///
  /// \returns 0 on succesful startup, -1 on failure.
  int
  llarp_apple_start(
      void* lokinet,
      packet_writer_callback packet_writer,
      start_reading_callback start_reading,
      void* callback_context);

  /// Called to deliver an incoming packet from the apple layer into lokinet; returns 0 on success,
  /// -1 if the packet could not be parsed, -2 if there is no current active VPNInterface associated
  /// with the lokinet (which generally means llarp_apple_start wasn't called or failed, or lokinet
  /// is in the process of shutting down).
  int
  llarp_apple_incoming(void* lokinet, const void* bytes, size_t size);

  /// Stops a lokinet instance created with `llarp_apple_initialize`.  This waits for lokinet to
  /// shut down and rejoins the thread.  After this call the given pointer is no longer valid.
  void
  llarp_apple_shutdown(void* lokinet);

#ifdef __cplusplus
}  // extern "C"
#endif
