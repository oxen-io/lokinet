import Foundation

enum LokiNetwork { // Used as a namespace
    
    static func initialize() {
        _enable_debug_mode()
        let filePath = Bundle.main.bundlePath + "/" + "liblokinet-configuration.ini"
        _llarp_ensure_config(filePath, nil, true, false)
        _llarp_main_init(filePath, false)
    }
}
