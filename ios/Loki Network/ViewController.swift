import UIKit

class ViewController : UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        let filePath = Bundle.main.bundlePath + "/" + "liblokinet-configuration.ini"
        let isConfigured = _llarp_ensure_config(filePath, nil, true, false)
        print(isConfigured)
    }
}
