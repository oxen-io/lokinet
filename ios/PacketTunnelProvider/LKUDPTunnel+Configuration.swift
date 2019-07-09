
extension LKUDPTunnel {
    
    struct Configuration {
        let address: String
        let port: UInt16
        
        init(address: String, port: UInt16) {
            self.address = address
            self.port = port
        }
        
        init(fromFileAt path: String) throws {
            let contents = try INIParser(path).sections
            if let _ = contents["dns"]?["bind"] {
                // TODO: Use
            } else {
                throw "No configuration file entry found for: \"dns\"."
            }
            if let _ = contents["network"]?["ifaddr"] {
                // TODO: Use
            } else {
                throw "No configuration file entry found for: \"ifaddr\"."
            }
            self.address = ""
            self.port = 0
        }
    }
}
