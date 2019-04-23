import Foundation

final class LKDaemon {
    
    // MARK: Lifecycle
    static let shared = LKDaemon()
    
    private init() { }
    
    // MARK: Configuration
    func configure(isDebuggingEnabled: Bool = false, completionHandler: @escaping (Error?) -> Void) {
        // Prepare
        let directoryPath = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!.path
        // Enable debugging mode if needed
        if isDebuggingEnabled { llarp_enable_debug_mode() }
        // Generate configuration file
        let configurationFilePath = directoryPath + "/" + "lokinet-configuration.ini"
        llarp_ensure_config(configurationFilePath, directoryPath, true, false)
        // Download bootstrap file
        let remoteBootstrapFilePath = "https://i2p.rocks/i2procks.signed"
        let downloadTask = URLSession.shared.dataTask(with: URL(string: remoteBootstrapFilePath)!) { data, _, error in
            guard let data = data else { return completionHandler(error) }
            let localBootstrapFilePath = directoryPath + "/" + "bootstrap.signed"
            try! data.write(to: URL(fileURLWithPath: localBootstrapFilePath))
            // Perform main setup
            guard let context = llarp_main_init(configurationFilePath, false) else { return completionHandler(LKError(description: "LLARP initialization failed.")) }
            llarp_main_setup(context)
            // Run
            llarp_main_run(context)
            // Invoke completion handler
            completionHandler(nil)
        }
        downloadTask.resume()
    }
}
