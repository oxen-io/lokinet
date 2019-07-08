
extension LKTCPTunnel {
    
    struct Configuration {
        let address: String
        let port: UInt16
        let readBufferSize: UInt
        
        init(address: String, port: String, readBufferSize: UInt = 128 * 1024) {
            self.address = address
            self.port = port
            self.readBufferSize = readBufferSize
        }
        
        init(fromFileAt path: String) throws {
            let contents = try INIParser(path).sections
            if let dns = contents["dns"]?["bind"] {
                self.dns = dns
            } else {
                throw "No configuration file entry found for: \"dns\"."
            }
            if let ifaddr = contents["network"]?["ifaddr"] {
                self.ifaddr = ifaddr
            } else {
                throw "No configuration file entry found for: \"ifaddr\"."
            }
        }
    }
}
