import Foundation

final class LokiNetwork {
    private var isConfigured = false
    
    // MARK: Types
    typealias LLARPContext = OpaquePointer
    
    // MARK: Initialization
    static let shared = LokiNetwork()
    
    private init() { }
    
    // MARK: Configuration
    func configure(isDebuggingEnabled: Bool, completionHandler: @escaping (LLARPContext) -> Void) { // TODO: Handle errors
        // Prepare
        let bundlePath = Bundle.main.bundlePath
        // Enable debugging mode if needed
        if isDebuggingEnabled { _llarp_enable_debug_mode() }
        // Generate configuration file
        let configurationFilePath = bundlePath + "/" + "liblokinet-configuration.ini"
        _llarp_ensure_config(configurationFilePath, bundlePath, true, false)
        // Download bootstrap file
        let remoteBootstrapFilePath = "https://i2p.rocks/i2procks.signed"
        let downloadTask = URLSession.shared.dataTask(with: URL(string: remoteBootstrapFilePath)!) { [weak self] data, _, _ in
            let localBootstrapFilePath = bundlePath + "/" + "bootstrap.signed"
            try! data!.write(to: URL(fileURLWithPath: localBootstrapFilePath))
            // Perform main setup
            let context = _llarp_main_init(configurationFilePath, false)!
            _llarp_main_setup(context)
            self?.isConfigured = true
            // Invoke completion handler
            completionHandler(context)
        }
        downloadTask.resume()
    }
    
    func run(with context: LLARPContext) {
        guard isConfigured else { fatalError() }
        _llarp_main_run(context)
    }
}
