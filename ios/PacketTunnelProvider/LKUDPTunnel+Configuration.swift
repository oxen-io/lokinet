
extension LKUDPTunnel {
    
    struct Configuration {
        let address: String
        let port: UInt16
        
        init(address: String, port: String) {
            self.address = address
            self.port = port
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
