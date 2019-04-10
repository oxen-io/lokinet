import Foundation

enum LokiNetwork { // Used as a namespace
    
    // TODO: Handle errors
    
    static func initialize(isDebuggingEnabled: Bool) {
        if isDebuggingEnabled { _enable_debug_mode() }
        let filePath = Bundle.main.bundlePath + "/" + "liblokinet-configuration.ini"
        _llarp_ensure_config(filePath, nil, true, false)
        let context = _llarp_main_init(filePath, false)!
        _llarp_main_setup(context)
    }
}
