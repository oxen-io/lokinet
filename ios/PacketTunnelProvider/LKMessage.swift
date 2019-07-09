import Foundation

struct LKMessage {
    private let headerSizeInBytes = 2
    
    enum Kind : String {
        case requestSession, acceptSession, rejectSession
    }
    
    init(from data: Data) {
        let payloadAsData = data[headerSizeInBytes...data.endIndex]
    }
}
