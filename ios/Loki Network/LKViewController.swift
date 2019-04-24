import UIKit

final class LKViewController : UIViewController {
    
    override func viewDidLoad() {
        super.viewDidLoad()
        let directoryPath = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!.path
        let configurationFileName = "lokinet-configuration.ini"
        let bootstrapFileURL = URL(string: "https://i2p.rocks/i2procks.signed")!
        let bootstrapFileName = "bootstrap.signed"
        let daemonConfiguration = LKDaemon.Configuration(isDebuggingEnabled: false, directoryPath: directoryPath, configurationFileName: configurationFileName, bootstrapFileURL: bootstrapFileURL, bootstrapFileName: bootstrapFileName)
        LKDaemon.shared.configure(with: daemonConfiguration) { result in
            switch result {
            case .success(let configurationFilePath, let context):
                do {
                    let tunnelConfiguration = try LKTunnel.Configuration(fromFileAt: configurationFilePath)
                    LKTunnel.shared.configure(with: tunnelConfiguration) { result in
                        switch result {
                        case .success: LKDaemon.shared.run(with: context)
                        case .failure(let error): print(error)
                        }
                    }
                } catch let error {
                    print(error)
                }
            case .failure(let error): print(error)
            }
        }
    }
}
