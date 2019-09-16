//
//  StreamReader.swift
//  lokinet
//
//  Copyright © 2019 Loki. All rights reserved.
//

import Foundation

final class StreamReader  {
    let encoding : String.Encoding
    let chunkSize : Int

    var fileHandle : FileHandle!
    var buffer : Data
    let delimData : Data
    var atEof : Bool = false

    init?(fh: FileHandle, delimiter: String = "\n", encoding : String.Encoding = .utf8, chunkSize : Int = 4096) {
        self.chunkSize = chunkSize
        self.encoding = encoding
        self.fileHandle = fh

        guard let delimData = delimiter.data(using: encoding) else {
            return nil
        }
        self.delimData = delimData
        self.buffer = Data(capacity: chunkSize)
    }

    /// Return next line, or nil on EOF.
    func nextLine() -> String? {
        precondition(fileHandle != nil, "Attempt to read from closed file")

        if atEof {
            return nil
        }

        // Read data chunks from file until a line delimiter is found:
        while !atEof {
            if let range = buffer.range(of: delimData) {
                // Convert complete line (excluding the delimiter) to a string:
                let line = String(data: buffer.subdata(in: 0..<range.lowerBound), encoding: encoding)
                // Remove line (and the delimiter) from the buffer:
                buffer.removeSubrange(0..<range.upperBound)
                return line
            }
            let tmpData = fileHandle.readData(ofLength: chunkSize)
            if tmpData.count > 0 {
                buffer.append(tmpData)
            } else {
                // EOF or read error.
                atEof = true
                if buffer.count > 0 {
                    // Buffer contains last line in file (not terminated by delimiter).
                    let line = String(data: buffer as Data, encoding: encoding)
                    buffer.count = 0
                    return line
                }
            }
        }

        return nil
    }
}

extension StreamReader : Sequence {
    func makeIterator() -> AnyIterator<String> {
        return AnyIterator {
            return self.nextLine()
        }
    }
}
