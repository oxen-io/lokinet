import Foundation
import AppKit

// Apple deprecated their command line tools to set images on things and replaced them with a
// barely-documented swift function.  Yay!

// Usage: ./seticon /path/to/my.icns /path/to/some.dmg

let args = CommandLine.arguments

if args.count != 3 {
    print("Error: usage: ./seticon /path/to/my.icns /path/to/some.dmg")
    exit(1)
}

var icns = args[1]
var dmg = args[2]

var img = NSImage(byReferencingFile: icns)!

if NSWorkspace.shared.setIcon(img, forFile: dmg) {
    print("Set \(dmg) icon to \(icns) [\(img.size)]")
} else {
    print("Setting icon failed, don't know why")
    exit(2)
}
