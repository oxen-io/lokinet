import MMWormhole

private let wormhole = MMWormhole(applicationGroupIdentifier: "group.com.niels-andriesse.loki-network", optionalDirectory: nil)

func LKLog(_ string: String) {
    #if DEBUG
    wormhole.passMessageObject(string as NSString, identifier: "log")
    #endif
}

func LKUpdateConnectionProgress(_ progress: Double) {
    wormhole.passMessageObject(progress as NSNumber, identifier: "connection_progress")
}
