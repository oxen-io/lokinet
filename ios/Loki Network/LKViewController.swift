import UIKit

final class LKViewController : UIViewController {
    
    override func viewDidLoad() {
        super.viewDidLoad()
        LKTunnel.shared.configure() { error in
            if let error = error {
                print(error)
            } else {
                LKDaemon.shared.configure(isDebuggingEnabled: false) { error in
                    if let error = error {
                        print(error)
                    } else {
                        // TODO: Implement
                    }
                }
            }
        }
    }
}
