import UIKit

class ViewController : UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        LokiNetwork.initialize(isDebuggingEnabled: true)
    }
}
