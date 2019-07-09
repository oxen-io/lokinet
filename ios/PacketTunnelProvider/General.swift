import MMWormhole

private let wormhole = MMWormhole(applicationGroupIdentifier: "group.com.niels-andriesse.loki-project", optionalDirectory: "logs")

func LKLog(_ string: String) {
    wormhole.passMessageObject(string as NSString, identifier: "loki")
}
