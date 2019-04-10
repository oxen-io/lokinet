import UIKit

class ViewController : UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        _enable_debug_mode()
        let filePath = Bundle.main.bundlePath + "/" + "liblokinet-configuration.ini"
        _llarp_ensure_config(filePath, nil, true, false)
        _llarp_main_init(filePath, false)
    }
}
