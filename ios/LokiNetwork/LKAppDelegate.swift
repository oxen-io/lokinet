import UIKit
import MMWormhole

@UIApplicationMain
final class LKAppDelegate : UIResponder, UIApplicationDelegate {
    private let wormhole = MMWormhole(applicationGroupIdentifier: "group.com.niels-andriesse.loki-project", optionalDirectory: "logs")
    var window: UIWindow?
    
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey:Any]? = nil) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        self.window = window
        window.rootViewController = LKMainViewController.load()
        window.makeKeyAndVisible()
        wormhole.listenForMessage(withIdentifier: "loki") { message in print(message ?? "nil") }
        return true
    }
}
