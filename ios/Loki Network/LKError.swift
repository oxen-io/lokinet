import Foundation

enum LKError : LocalizedError {
    case generic
    case incompleteConfigurationFile(missingEntry: String)
    
    var errorDescription: String {
        switch self {
        case .generic: return "An error occurred."
        case .incompleteConfigurationFile(let missingEntry): return "No configuration file entry found for: \"\(missingEntry)\"."
        }
    }
}
