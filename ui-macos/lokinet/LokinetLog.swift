//
//  LokinetLog.swift
//  lokinet
//
//  Copyright © 2019 Loki. All rights reserved.
//

import AppKit

class LokinetLogController : NSViewController {
    override func viewDidLoad() {
        super.viewDidLoad()
    }

    var log: LokinetLog {
        get {
            // this is walking down the UI stack.
            // TODO: work out a better way of doing this
            let scroll = self.view.subviews[0] as! NSScrollView
            let clip = scroll.subviews[0] as! NSClipView
            let log = clip.subviews[0] as! LokinetLog
            return log
        }
    }

}

protocol Appendable {
    func append(string: String)
}

final class LokinetLog : NSTextView, Appendable {
    func append(string: String) {
        self.textStorage?.append(NSAttributedString(string: string + "\n"))
        self.scrollToEndOfDocument(nil)
    }
}
