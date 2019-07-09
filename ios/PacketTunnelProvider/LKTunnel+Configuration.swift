
extension LKTunnel {
    
    struct Configuration {
        let address: String
        let port: UInt16
        
        init(address: String, port: UInt16) {
            self.address = address
            self.port = port
        }
    }
}
