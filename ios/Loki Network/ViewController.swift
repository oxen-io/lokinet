import UIKit

class ViewController : UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        let daemon = Daemon.shared
        daemon.configure(isDebuggingEnabled: true) { [weak daemon] context in
            daemon?.run(with: context)
        }
    }
}
