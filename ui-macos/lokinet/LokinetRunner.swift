//
//  LokinetRunner.swift
//  lokinet
//
//  Copyright © 2019 Loki. All rights reserved.
//

import Foundation

class LokinetRunner {
    static let PATH_KEY = "lokinetPath"
    static let DEFAULT_PATH = URL(fileURLWithPath: "/usr/local/bin/lokinet")

    var lokinetPath: URL?
    var process = Process()
    let dnsManager: DNSManager
    weak var window: LokinetLog?

    init(window: LokinetLog, interface: String) {
        self.dnsManager = DNSManager(interface: interface)
        self.window = window
        configure()
    }

    func configure() {
        let defaults = UserDefaults.standard;

        self.lokinetPath = defaults.url(forKey: LokinetRunner.PATH_KEY) ?? LokinetRunner.DEFAULT_PATH
        defaults.set(self.lokinetPath, forKey: LokinetRunner.PATH_KEY)
    }

    func enableDNS() {
        do {
            try dnsManager.setNewSettings()
        } catch {
            self.window?.presentError(error)
        }
    }

    func start() {
        process.executableURL = self.lokinetPath
        process.arguments = ["--colour=false"]
        let outputPipe = Pipe()
        process.standardOutput = outputPipe
        process.standardError = outputPipe

        do {
            try process.run()
        } catch {
            self.window?.presentError(error)
        }

        guard let reader = StreamReader(fh: outputPipe.fileHandleForReading) else {
            let err = NSError(domain: "lokinet", code: 0, userInfo: ["msg": "Failed to read from filehandle"])
            self.window?.presentError(err)
            return
        }

        DispatchQueue.global(qos: .background).async {
            for line in reader {
                DispatchQueue.main.async {
                    self.window?.append(string: line)
                }
            }
        }

        enableDNS()
    }

    deinit {
        if process.isRunning {
            process.terminate()
            process.waitUntilExit()
        }
    }
}
