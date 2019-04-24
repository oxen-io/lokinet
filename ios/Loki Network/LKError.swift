import Foundation

struct LKError : LocalizedError {
    let errorDescription: String
    
    static let generic = LKError(errorDescription: "An error occurred.")
}
