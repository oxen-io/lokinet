import UIKit
import MMWormhole

@UIApplicationMain
final class LKAppDelegate : UIResponder, UIApplicationDelegate {
    var window: UIWindow?
    
    static let wormhole = MMWormhole(applicationGroupIdentifier: "group.com.niels-andriesse.loki-project", optionalDirectory: "logs")
    
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey:Any]? = nil) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        self.window = window
        window.rootViewController = LKMainViewController.load()
        window.makeKeyAndVisible()
        LKAppDelegate.wormhole.listenForMessage(withIdentifier: "log") { log in print(log ?? "nil") }
        return true
    }
}
