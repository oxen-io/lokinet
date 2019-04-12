import UIKit

class ViewController : UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        LokiNetwork.initialize(isDebuggingEnabled: true) { context in
            LokiNetwork.run(with: context)
        }
    }
}
