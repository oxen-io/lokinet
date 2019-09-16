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

    let oldDNSSettings: [String]
    let interface: String

    static func getOldSettings(interface: String) -> [String] {
        let netprocess = Process()
        netprocess.executableURL = DNSManager.netSetup
        netprocess.arguments = ["-getdnsservers", interface]

        do {
            let pipe = Pipe()
            netprocess.standardOutput = pipe
            try netprocess.run()
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            let asStr = String(data: data, encoding: .ascii)

            return split(str: asStr).filter({$0 != "127.0.0.1"})
        } catch {
            return []
        }
    }

    func setNewSettings() throws {
        let netprocess = Process()
        netprocess.executableURL = DNSManager.netSetup

        netprocess.arguments = ["-setdnsservers", self.interface]
        netprocess.arguments?.append("127.0.0.1")
        netprocess.arguments?.append(contentsOf: oldDNSSettings)

        try netprocess.run()
    }

    func restoreOldSettings() {
        let netprocess = Process()
        netprocess.executableURL = DNSManager.netSetup

        netprocess.arguments = ["-setdnsservers", self.interface]
        netprocess.arguments?.append(contentsOf: oldDNSSettings)

        do {
            try netprocess.run()
            print("Overriding DNS Settings of \(self.oldDNSSettings)")
        } catch {
            // do nothing
        }
    }

    init(interface: String) {
        self.interface = interface
        self.oldDNSSettings = DNSManager.getOldSettings(interface: interface)
        print("Overriding DNS Settings of \(self.oldDNSSettings)")
    }

    deinit {
        restoreOldSettings()
    }
}
