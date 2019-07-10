import UIKit

final class LKMainViewController : UIViewController {
    @IBOutlet private var button: GBKUIButtonProgressView!
    
    private var lokinet: LKLokiNetManager { return LKLokiNetManager.shared }
    
    // MARK: Lifecycle
    static func load() -> LKMainViewController {
        return LKMainViewController(nibName: String(describing: self), bundle: Bundle.main)
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        button.tintColor = UIColor(0x5BCA5B)
        button.initialTitle = "Connect"
        button.completeTitle = "Disconnect"
        button.addTarget(self, action: #selector(toggleLokinet), for: UIControl.Event.touchUpInside)
        lokinet.onStatusChanged = { [weak self] in self?.updateButtonStatus() }
        LKAppDelegate.wormhole.listenForMessage(withIdentifier: "connection_progress") { [weak self] progress in self?.setButtonProgress(to: progress as! Double) }
    }
    
    // MARK: Updating
    private func updateButtonStatus() {
        switch lokinet.status {
        case .connecting: button.startProgressing()
        case .connected: button.completeProgressing()
        case .disconnecting, .disconnected: button.reset()
        }
    }
    
    private func setButtonProgress(to progress: Double) {
        guard button.isProgressing else { return }
        button.setProgress(CGFloat(progress), animated: true)
    }
    
    // MARK: Interaction
    @objc private func toggleLokinet() {
        switch lokinet.status {
        case .disconnected: lokinet.start()
        case .connected: lokinet.stop()
        default: break
        }
    }
}
