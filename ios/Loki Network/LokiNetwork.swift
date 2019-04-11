import Foundation

enum LokiNetwork { // Used as a namespace
    
    // TODO: Handle errors
    
    static func initialize(isDebuggingEnabled: Bool) {
        // Enable debugging mode if needed
        if isDebuggingEnabled { _enable_debug_mode() }
        // Convenience
        let bundlePath = Bundle.main.bundlePath
        // Create or overwrite the configuration file in the bundle folder
        let configurationFilePath = bundlePath + "/" + "liblokinet-configuration.ini"
        _llarp_ensure_config(configurationFilePath, nil, true, false)
        // Convenience
        var configurationFileContents = try! String(contentsOf: URL(fileURLWithPath: configurationFilePath), encoding: .utf8)
        func updateSetting(withKey key: String, fileName: String) {
            let filePath = bundlePath + "/" + fileName
            configurationFileContents = configurationFileContents.replacingOccurrences(of: "\(key)=\(fileName)", with: "\(key)=\(filePath)")
        }
        // Download the bootstrap file and update the configuration file accordingly
        let remoteBootstrapFilePath = "https://i2p.rocks/i2procks.signed"
        let downloadTask = URLSession.shared.dataTask(with: URL(string: remoteBootstrapFilePath)!) { (data, _, _) in
            let localBootstrapFilePath = bundlePath + "/" + "bootstrap.signed"
            try! data!.write(to: URL(fileURLWithPath: localBootstrapFilePath))
            updateSetting(withKey: "add-node", fileName: "bootstrap-signed")
            // Generate the required files and update the configuration file accordingly
            _llarp_find_or_create_encryption_file(...)
            // TODO: Update the configuration file
            // Perform the main setup
            let context = _llarp_main_init(configurationFilePath, false)!
        }
        downloadTask.resume()
        // Manually set the locations of files that are included in the bundle instead of generated
//        var configurationFileContents = try! String(contentsOf: URL(fileURLWithPath: configurationFilePath), encoding: .utf8)
//        func pointToBundle(key: String, fileName: String) {
//            let filePath = bundlePath + "/" + fileName
//            configurationFileContents = configurationFileContents.replacingOccurrences(of: "\(key)=\(fileName)", with: "\(key)=\(filePath)")
//        }
        //        DONE: pointToBundle(key: "add-node", fileName: "bootstrap.signed")
//        pointToBundle(key: "encryption-privkey", fileName: "encryption.private")
//        pointToBundle(key: "ident-privkey", fileName: "identity.private")
//        pointToBundle(key: "network profiles", fileName: "profiles.dat")
//        pointToBundle(key: "contact-file", fileName: "self.signed")
//        pointToBundle(key: "transport-privkey", fileName: "transport.private")
//        try! configurationFileContents.write(toFile: configurationFilePath, atomically: true, encoding: .utf8)
//        // Perform the main LLARP setup
//        let context = _llarp_main_init(configurationFilePath, false)!
//        _llarp_main_setup(context)
    }
}
