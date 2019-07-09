
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
            if let bind = contents["api"]?["bind"] {
                let parts = bind.split(separator: ":").map { String($0) }
                address = parts[0]
                port = UInt16(parts[1])!
            } else {
                throw "No configuration file entry found for: \"bind\"."
            }
        }
    }
}
