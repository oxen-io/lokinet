import UIKit

final class LKMainViewController : UIViewController {
    @IBOutlet private var button: UIButton!
    
    private var lokiNet: LKLokiNetManager { return LKLokiNetManager.shared }
    
    // MARK: Lifecycle
    static func load() -> LKMainViewController {
        return LKMainViewController(nibName: String(describing: self), bundle: Bundle.main)
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        lokiNet.onStatusChanged = { [weak self] in self?.updateButton() }
    }
    
    // MARK: Updating
    private func updateButton() {
        let title: String = {
            switch lokiNet.status {
            case .connecting: return "connecting..."
            case .connected: return "disconnect"
            case .disconnecting: return "disconnecting..."
            case .disconnected: return "connect"
            }
        }()
        button.setTitle(title, for: UIControl.State.normal)
    }
    
    // MARK: Interaction
    @IBAction private func toggleLokiNet() {
        switch lokiNet.status {
        case .disconnected: lokiNet.start()
        case .connected: lokiNet.stop()
        default: break
        }
    }
}
