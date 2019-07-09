
enum LKError : Error {
    case loadingTunnelProviderConfigurationFailed
    case savingTunnelProviderConfigurationFailed
    case startingLokiNetFailed
    case downloadingBootstrapFileFailed
    case llarpInitializationFailed
    case openingTunnelFailed(destination: String)
    
    var message: String {
        switch self {
        case .loadingTunnelProviderConfigurationFailed: return "Couldn't load tunnel provider configuration"
        case .savingTunnelProviderConfigurationFailed: return "Couldn't save tunnel provider configuration"
        case .startingLokiNetFailed: return "Couldn't start LokiNet"
        case .downloadingBootstrapFileFailed: return "Couldn't download bootstrap file"
        case .llarpInitializationFailed: return "Couldn't initialize LLARP"
        case .openingTunnelFailed(let destination): return "Couldn't open tunnel to \(destination)"
        }
    }
}
