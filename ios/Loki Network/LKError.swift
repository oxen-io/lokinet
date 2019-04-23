import Foundation

struct LKError : LocalizedError {
    let errorDescription: String
    
    init(description: String) {
        errorDescription = description
    }
}
