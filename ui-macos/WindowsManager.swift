//
//  WindowsManager.swift
//  lokinet
//
//  Copyright © 2019 Loki. All rights reserved.
//

import AppKit
import Foundation

struct WindowsManager {
    static func getVC<T: NSViewController>(withIdentifier identifier: String,
                                           ofType: T.Type?,
                                           storyboard: String = "Main",
                                           bundle: Bundle? = nil) -> T? {
        let storyboard = NSStoryboard(name: storyboard, bundle: bundle)

        guard let vc: T = storyboard.instantiateController(withIdentifier: identifier) as? T else {
            let alert = NSAlert()
            alert.alertStyle = .critical
            alert.messageText = "Error initiating the viewcontroller"
            alert.runModal()

            return nil
        }

        return vc
    }
}
