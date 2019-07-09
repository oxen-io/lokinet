import Foundation
import PromiseKit

enum LKDaemon {
    
    struct Configuration {
        let isDebuggingEnabled: Bool
        let directoryPath: String
        let configurationFileName: String
        let bootstrapFileURL: URL
        let bootstrapFileName: String
    }
    
    typealias LLARPContext = OpaquePointer
    
    static func configure(with configuration: Configuration) -> Promise<(configurationFilePath: String, context: LLARPContext)> {
        return Promise<(configurationFilePath: String, context: LLARPContext)> { seal in
            // Enable debugging mode if needed
            if configuration.isDebuggingEnabled { llarp_enable_debug_mode() }
            // Generate configuration file
            let configurationFilePath = configuration.directoryPath + "/" + configuration.configurationFileName
            llarp_ensure_config(configurationFilePath, configuration.directoryPath, true, false)
            // Download bootstrap file
            let downloadTask = URLSession.shared.dataTask(with: configuration.bootstrapFileURL) { data, _, error in
                guard let data = data else { return seal.reject(error ?? LKError.downloadingBootstrapFileFailed) }
                let bootstrapFilePath = configuration.directoryPath + "/" + configuration.bootstrapFileName
                do {
                    try data.write(to: URL(fileURLWithPath: bootstrapFilePath))
                } catch let error {
                    return seal.reject(error)
                }
                // Perform main setup
                guard let context = llarp_main_init(configurationFilePath, false) else { return seal.reject(LKError.llarpInitializationFailed) }
                llarp_main_setup(context)
                // Invoke completion handler
                seal.fulfill((configurationFilePath: configurationFilePath, context: context))
            }
            downloadTask.resume()
        }
    }
    
    static func start(with context: LLARPContext) {
        llarp_main_run(context)
    }
    
    static func stop() {
        // TODO: Implement
    }
}
