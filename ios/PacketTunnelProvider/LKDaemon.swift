import Foundation

enum LKDaemon {
    
    struct Configuration {
        let isDebuggingEnabled: Bool
        let directoryPath: String
        let configurationFileName: String
        let bootstrapFileURL: URL
        let bootstrapFileName: String
    }
    
    typealias LLARPContext = OpaquePointer
    
    static func configure(with configuration: Configuration, completionHandler: @escaping (Result<(configurationFilePath: String, context: LLARPContext), Error>) -> Void) {
        // Enable debugging mode if needed
        if configuration.isDebuggingEnabled { llarp_enable_debug_mode() }
        // Generate configuration file
        let configurationFilePath = configuration.directoryPath + "/" + configuration.configurationFileName
        llarp_ensure_config(configurationFilePath, configuration.directoryPath, true, false)
        // Download bootstrap file
        let downloadTask = URLSession.shared.dataTask(with: configuration.bootstrapFileURL) { data, _, error in
            guard let data = data else { return completionHandler(.failure(error ?? "Couldn't download bootstrap file.")) }
            let bootstrapFilePath = configuration.directoryPath + "/" + configuration.bootstrapFileName
            do {
                try data.write(to: URL(fileURLWithPath: bootstrapFilePath))
            } catch let error {
                completionHandler(.failure(error))
            }
            // Perform main setup
            guard let context = llarp_main_init(configurationFilePath, false) else { return completionHandler(.failure("Couldn't initialize LLARP.")) }
            llarp_main_setup(context)
            // Invoke completion handler
            completionHandler(.success((configurationFilePath: configurationFilePath, context: context)))
        }
        downloadTask.resume()
    }
    
    static func start(with context: LLARPContext) {
        llarp_main_run(context)
    }
    
    static func stop() {
        // TODO: Implement
    }
}
