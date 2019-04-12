import Foundation

enum LokiNetwork { // Used as a namespace
    
    // TODO: Handle errors
    
    static func initialize(isDebuggingEnabled: Bool) {
        // Prepare
        let bundlePath = Bundle.main.bundlePath
        // Enable debugging mode if needed
        if isDebuggingEnabled { _llarp_enable_debug_mode() }
        // Generate configuration file
        let configurationFilePath = bundlePath + "/" + "liblokinet-configuration.ini"
        _llarp_ensure_config(configurationFilePath, bundlePath, true, false)
        // Download bootstrap file
        let remoteBootstrapFilePath = "https://i2p.rocks/i2procks.signed"
        let downloadTask = URLSession.shared.dataTask(with: URL(string: remoteBootstrapFilePath)!) { data, _, _ in
            let localBootstrapFilePath = bundlePath + "/" + "bootstrap.signed"
            try! data!.write(to: URL(fileURLWithPath: localBootstrapFilePath))
            // Perform main setup
            let context = _llarp_main_init(configurationFilePath, false)!
            _llarp_main_setup(context)
        }
        downloadTask.resume()
    }
}
