import AppKit
import Foundation
import NetworkExtension
import SystemExtensions

let app = NSApplication.shared

let START = "--start"
let STOP = "--stop"

let HELP_STRING = "usage: lokinet {--start|--stop}"

class LokinetMain: NSObject, NSApplicationDelegate {
    var vpnManager = NETunnelProviderManager()
    var mode = START
    let netextBundleId = "org.lokinet.network-extension"

    func applicationDidFinishLaunching(_: Notification) {
        if mode == START {
            startNetworkExtension()
        } else if mode == STOP {
            tearDownVPNTunnel()
        } else {
            result(msg: HELP_STRING)
        }
    }

    func bail() {
        app.terminate(self)
    }

    func result(msg: String) {
        NSLog(msg)
        // TODO: does lokinet continue after this?
        bail()
    }

    func tearDownVPNTunnel() {
        NSLog("Stopping Lokinet")
        NETunnelProviderManager.loadAllFromPreferences { [self] (savedManagers: [NETunnelProviderManager]?, error: Error?) in
            if let error = error {
                self.result(msg: error.localizedDescription)
                return
            }

            if let savedManagers = savedManagers {
                for manager in savedManagers {
                    if (manager.protocolConfiguration as? NETunnelProviderProtocol)?.providerBundleIdentifier == self.netextBundleId {
                        manager.connection.stopVPNTunnel()
                        self.result(msg: "Lokinet Down")
                    }
                }
            }
            self.result(msg: "Lokinet is not up")
        }
    }

    func startNetworkExtension() {
        #if MACOS_SYSTEM_EXTENSION
            NSLog("Loading Lokinet network extension")
            // Start by activating the system extension
            let activationRequest = OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: netextBundleId, queue: .main)
            activationRequest.delegate = self
            OSSystemExtensionManager.shared.submitRequest(activationRequest)
        #else
            setupVPNTunnel()
        #endif
    }

    func setupVPNTunnel() {
        NSLog("Starting up Lokinet tunnel")
        NETunnelProviderManager.loadAllFromPreferences { [self] (savedManagers: [NETunnelProviderManager]?, error: Error?) in
            if let error = error {
                self.result(msg: error.localizedDescription)
                return
            }

            if let savedManagers = savedManagers {
                for manager in savedManagers {
                    if (manager.protocolConfiguration as? NETunnelProviderProtocol)?.providerBundleIdentifier == self.netextBundleId {
                        NSLog("Found saved VPN Manager")
                        self.vpnManager = manager
                    }
                }
            }
            let providerProtocol = NETunnelProviderProtocol()
            providerProtocol.serverAddress = "loki.loki" // Needs to be set to some non-null dummy value
            providerProtocol.username = "anonymous"
            providerProtocol.providerBundleIdentifier = self.netextBundleId
            providerProtocol.enforceRoutes = true
            // macos seems to have trouble when this is true, and reports are that this breaks and
            // doesn't do what it says on the tin in the first place.  Needs more testing.
            providerProtocol.includeAllNetworks = false
            self.vpnManager.protocolConfiguration = providerProtocol
            self.vpnManager.isEnabled = true
            // self.vpnManager.isOnDemandEnabled = true
            self.vpnManager.localizedDescription = "lokinet"
            self.vpnManager.saveToPreferences(completionHandler: { [self] error -> Void in
                if error != nil {
                    NSLog("Error saving to preferences")
                    self.result(msg: error!.localizedDescription)
                } else {
                    self.vpnManager.loadFromPreferences(completionHandler: { error in
                        if error != nil {
                            NSLog("Error loading from preferences")
                            self.result(msg: error!.localizedDescription)
                        } else {
                            do {
                                NSLog("Trying to start")
                                self.initializeConnectionObserver()
                                try self.vpnManager.connection.startVPNTunnel()
                            } catch let error as NSError {
                                self.result(msg: error.localizedDescription)
                            } catch {
                                self.result(msg: "There was a fatal error")
                            }
                        }
                    })
                }
            })
        }
    }

    func initializeConnectionObserver() {
        NotificationCenter.default.addObserver(forName: NSNotification.Name.NEVPNStatusDidChange, object: vpnManager.connection, queue: OperationQueue.main) { [self] _ -> Void in
            if self.vpnManager.connection.status == .invalid {
                self.result(msg: "VPN configuration is invalid")
            } else if self.vpnManager.connection.status == .disconnected {
                self.result(msg: "VPN is disconnected.")
            } else if self.vpnManager.connection.status == .connecting {
                NSLog("VPN is connecting...")
            } else if self.vpnManager.connection.status == .reasserting {
                NSLog("VPN is reasserting...")
            } else if self.vpnManager.connection.status == .disconnecting {
                NSLog("VPN is disconnecting...")
            } else if self.vpnManager.connection.status == .connected {
                self.result(msg: "VPN Connected")
            }
        }
    }
}

#if MACOS_SYSTEM_EXTENSION

    extension LokinetMain: OSSystemExtensionRequestDelegate {
        func request(_: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
            guard result == .completed else {
                NSLog("Unexpected result %d for system extension request", result.rawValue)
                return
            }
            NSLog("Lokinet system extension loaded")
            setupVPNTunnel()
        }

        func request(_: OSSystemExtensionRequest, didFailWithError error: Error) {
            NSLog("System extension request failed: %@", error.localizedDescription)
            self.bail()
        }

        func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
            NSLog("Extension %@ requires user approval", request.identifier)
        }

        func request(_ request: OSSystemExtensionRequest,
                     actionForReplacingExtension existing: OSSystemExtensionProperties,
                     withExtension extension: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction
        {
            NSLog("Replacing extension %@ version %@ with version %@", request.identifier, existing.bundleShortVersion, `extension`.bundleShortVersion)
            return .replace
        }
    }

#endif

let args = CommandLine.arguments

// If we are invoked with no arguments then exec the gui.  This is dumb, but there doesn't seem to
// be a nicer way to do this on Apple's half-baked platform because:
// - we have three "bundles" we need to manage: the GUI app, the system extension, and the Lokinet
//   app (this file) which loads the system extension.
// - if we embed the system extension directly inside the GUI then it fails to launch because the
//   electron GUI's requirements (needed for JIT) conflict with the ability to load a system
//   extensions.
// - if we embed Lokinet.app inside Lokinet-GUI.app and then the system extension inside Lokinet.app
//   then it works, but macos loses track of the system extension and doesn't remove it when you
//   remove the application.  (It breaks your system, leaving an impossible-to-remove system
//   extension, in just the same way it breaks if you don't use Finder to remove the Application.
//   Apple used to say (around 2 years ago as of writing) that they would fix this situation "soon",
//   but hasn't, and has stopped saying anything about it.)
// - if we try to use multiple executables (one to launch the system extension, one simple shell
//   script to execs the embedded GUI app) inside the Lokinet.app and make the GUI the default for
//   the application then Lokinet gets killed by gatekeeper because code signing only applies the
//   (required-for-system-extensions) provisioningprofile to the main binary in the app.
//
// So we are left needing *one* single binary that isn't the GUI but has to do double-duty for both
// exec'ing the binary and loading lokinet, depending on how it is called.
//
// But of course there is no way to specify command-line arguments to the default binary macOS runs,
// so we can't use a `--gui` flag or anything so abhorrent to macos purity, thus this nasty
// solution:
//   - no args -- exec the GUI
//   - `--start` -- load the system extension and start lokinet
//   - `--stop` -- stop lokinet
//
// macOS: land of half-baked implementations and nasty hacks to make anything work.

if args.count == 1 {
    let gui_path = Bundle.main.resourcePath! + "/../Helpers/Lokinet-GUI.app"
    if !FileManager.default.fileExists(atPath: gui_path) {
        NSLog("Could not find gui app at %@", gui_path)
        exit(1)
    }
    let gui_url = URL(fileURLWithPath: gui_path, isDirectory: false)
    let gui_app_conf = NSWorkspace.OpenConfiguration()
    let group = DispatchGroup()
    group.enter()
    NSWorkspace.shared.openApplication(at: gui_url, configuration: gui_app_conf,
            completionHandler: { (app, error) in
                if error != nil {
                    NSLog("Error launching gui: %@", error!.localizedDescription)
                } else {
                    NSLog("Lauched GUI");
                }
                group.leave()
            })
    group.wait()

} else if args.count == 2 {
    let delegate = LokinetMain()
    delegate.mode = args[1]
    app.delegate = delegate
    app.run()
} else {
    NSLog(HELP_STRING)
}
