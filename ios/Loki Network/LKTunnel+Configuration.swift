
extension LKTunnel {
    
    struct Configuration {
        let port: String
        let subnet: String
        let dns: String
        let server: String
        
        init(port: String, subnet: String, dns: String, server: String) {
            self.port = port
            self.subnet = subnet
            self.dns = dns
            self.server = server
        }
        
        init(fromFileAt path: String) {
            fatalError("not implemented")
        }
    }
}
