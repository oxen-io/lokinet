import UIKit

class ViewController : UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        print(_llarp_ensure_config("liblokinet.ini", nil, false, false))
    }
}
