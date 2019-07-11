import Foundation
import PromiseKit

enum LKDaemon {

    private static let requestTerminationSignal: Int32 = 15

    struct Configuration {
        let isDebuggingEnabled: Bool
        let directoryPath: String
        let configurationFileName: String
        let bootstrapFileURL: URL
        let bootstrapFileName: String
    }
    
    typealias Context = OpaquePointer

    static func configure(with configuration: Configuration) -> Promise<(configurationFilePath: String, context: Context)> {
        return Promise<(configurationFilePath: String, context: Context)> { seal in
            // Enable debugging mode if needed
            if configuration.isDebuggingEnabled { llarp_enable_debug_mode() }
            // Generate configuration file
            let configurationFilePath = configuration.directoryPath + "/" + configuration.configurationFileName
            llarp_ensure_config(configurationFilePath, configuration.directoryPath, true, false)
            // Update connection progress
            LKUpdateConnectionProgress(0.4)
            // Download bootstrap file
            let downloadTask = URLSession.shared.dataTask(with: configuration.bootstrapFileURL) { data, _, error in
                guard let data = data else { return seal.reject(error ?? LKError.downloadingBootstrapFileFailed) }
                let bootstrapFilePath = configuration.directoryPath + "/" + configuration.bootstrapFileName
                do {
                    try data.write(to: URL(fileURLWithPath: bootstrapFilePath))
                } catch let error {
                    return seal.reject(error)
                }
                // Update connection progress
                LKUpdateConnectionProgress(0.6)
                // Perform main setup
                guard let context = llarp_main_init(configurationFilePath, false) else { return seal.reject(LKError.llarpInitializationFailed) }
                llarp_main_setup(context)
                // Update connection progress
                LKUpdateConnectionProgress(0.8)
                // Invoke completion handler
                seal.fulfill((configurationFilePath: configurationFilePath, context: context))
            }
            downloadTask.resume()
        }
    }
    
    static func start(with context: Context) -> Bool {
        let resultCode = llarp_main_run(context)
        let isSuccess = (resultCode == 0)
        if isSuccess { LKUpdateConnectionProgress(1.0) }
        return isSuccess
    }
    
    static func stop(with context: Context) {
        llarp_main_signal(context, requestTerminationSignal)
    }
}
