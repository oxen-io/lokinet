import UIKit

class ViewController : UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        let lokiNetwork = LokiNetwork.shared
        lokiNetwork.configure(isDebuggingEnabled: true) { [weak lokiNetwork] context in
            lokiNetwork?.run(with: context)
        }
    }
}
