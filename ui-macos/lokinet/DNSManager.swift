//
//  DNSManager.swift
//  lokinet
//
//  Copyright © 2019 Loki. All rights reserved.
//

import Foundation

func split(str: String?) -> [String] {
    let res = str?.components(separatedBy: NSCharacterSet.whitespacesAndNewlines) ?? []
    return res.filter({!$0.isEmpty})
}

class DNSManager {
    static let netSetup = URL(fileURLWithPath: "/usr/sbin/networksetup")

    let interface: String
    var oldDNSSettings: [String] = []

    func getOldSettings() -> [String] {
        let netprocess = Process()
        netprocess.executableURL = DNSManager.netSetup
        netprocess.arguments = ["-getdnsservers", interface]

        do {
            let pipe = Pipe()
            netprocess.standardOutput = pipe
            try netprocess.run()
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            let asStr = String(data: data, encoding: .ascii)

            if asStr?.contains("There aren't any DNS Servers") ?? true {
                return []
            } else {
                return split(str: asStr).filter({$0 != "127.0.0.1"})
            }
        } catch {
            return []
        }
    }

    func setNewSettings() throws {
        self.oldDNSSettings = getOldSettings()
        print("Overriding DNS Settings of \(self.oldDNSSettings)")

        let netprocess = Process()
        netprocess.executableURL = DNSManager.netSetup

        netprocess.arguments = ["-setdnsservers", self.interface, "127.0.0.1"]

        try netprocess.run()
    }

    func restoreOldSettings() {
        let netprocess = Process()
        netprocess.executableURL = DNSManager.netSetup

        netprocess.arguments = ["-setdnsservers", self.interface]

        if oldDNSSettings.isEmpty {
            // networkmsetup uses "networksetup -setdnsservers <interface> Empty" to reset
            netprocess.arguments?.append("Empty")
        } else {
            netprocess.arguments?.append(contentsOf: oldDNSSettings)
        }

        do {
            try netprocess.run()
            print("Resetting DNS Settings to \(self.oldDNSSettings)")
        } catch {
            // do nothing
        }
    }

    init(interface: String) {
        self.interface = interface
    }
}
