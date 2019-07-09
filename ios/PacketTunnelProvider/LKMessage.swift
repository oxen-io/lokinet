import Foundation

struct LKMessage {
    
    enum Kind : String {
        case requestSession, acceptSession, rejectSession
    }
    
    init(from data: Data) {
        
    }
}
