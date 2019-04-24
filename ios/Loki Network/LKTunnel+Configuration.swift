
extension LKTunnel {
    
    struct Configuration {
        let dns: String
        let ifaddr: String
        
        init(dns: String, ifaddr: String) {
            self.dns = dns
            self.ifaddr = ifaddr
        }
        
        init(fromFileAt path: String) throws {
            let contents = try INIParser(path).sections
            if let dns = contents["dns"]?["bind"] {
                self.dns = dns
            } else {
                throw LKError.incompleteConfigurationFile(missingEntry: "dns")
            }
            if let ifaddr = contents["network"]?["ifaddr"] {
                self.ifaddr = ifaddr
            } else {
                throw LKError.incompleteConfigurationFile(missingEntry: "ifaddr")
            }
        }
    }
}
