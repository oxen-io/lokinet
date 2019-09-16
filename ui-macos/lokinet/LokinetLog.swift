//
//  LokinetLog.swift
//  lokinet
//
//  Copyright © 2019 Loki. All rights reserved.
//

import AppKit

final class LokinetLog : NSTextView {

    var runner: LokinetRunner?

    override init(frame: NSRect, textContainer: NSTextContainer?) {
        super.init(frame: frame, textContainer: textContainer)
        self.runner = LokinetRunner(window: self, interface: "Wi-Fi")

        self.runner?.start()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        self.runner = LokinetRunner(window: self, interface: "Wi-Fi")

        self.runner?.start()
    }

    func append(string: String) {
        self.textStorage?.append(NSAttributedString(string: string + "\n"))
        self.scrollToEndOfDocument(nil)
    }
}
