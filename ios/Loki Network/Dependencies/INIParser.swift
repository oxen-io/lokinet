//
//  INIParser.swift
//  Perfect-INIParser
//
//  Created by Rockford Wei on 2017-04-25.
//  Copyright © 2017 PerfectlySoft. All rights reserved.
//
//===----------------------------------------------------------------------===//
//
// This source file is part of the Perfect.org open source project
//
// Copyright (c) 2017 - 2018 PerfectlySoft Inc. and the Perfect project authors
// Licensed under Apache License v2.0
//
// See http://perfect.org/licensing.html for license information
//
//===----------------------------------------------------------------------===//
//

import Foundation

public class Stack<T> {
    internal var array: [T] = []
    public func push(_ element: T) {
        array.append(element)
    }
    public func pop () -> T? {
        if array.isEmpty { return nil }
        let element = array.removeLast()
        return element
    }
    public func top () -> T? {
        return array.last
    }
    public var isEmpty: Bool { return array.isEmpty }
}

/// INI Configuration File Reader
public class INIParser {
    
    internal var _sections: [String: [String: String]] = [:]
    internal var _anonymousSection: [String: String] = [:]
    
    public var sections: [String:[String:String]] { return _sections }
    public var anonymousSection: [String: String] { return _anonymousSection }
    
    public enum Exception: Error {
        case InvalidSyntax, InvalidFile
    }
    
    enum State {
        case Title, Variable, Value, SingleQuotation, DoubleQuotation
    }
    
    enum ContentType {
        case Section(String)
        case Assignment(String, String)
    }
    
    internal func parse(line: String) throws -> ContentType? {
        var cache = ""
        var state = State.Variable
        let stack = Stack<State>()
        
        var variable: String? = nil
        for c in line {
            switch c {
            case " ", "\t":
                if state == .SingleQuotation || state == .DoubleQuotation {
                    cache.append(c)
                }
                break
            case "[":
                if state == .Variable {
                    cache = ""
                    stack.push(state)
                    state = .Title
                }
                break
            case "]":
                if state == .Title {
                    guard let last = stack.pop() else { throw Exception.InvalidSyntax }
                    state = last
                    return ContentType.Section(cache)
                }
                break
            case "=":
                if state == .Variable {
                    variable = cache
                    cache = ""
                    state = .Value
                } else {
                    cache.append(c)
                }
                break
            case "#", ";":
                if state == .Value {
                    if let v = variable {
                        return ContentType.Assignment(v, cache)
                    } else {
                        throw Exception.InvalidSyntax
                    }
                } else {
                    return nil
                }
            case "\"":
                if state == .DoubleQuotation {
                    guard let last = stack.pop() else {
                        throw Exception.InvalidSyntax
                    }
                    state = last
                } else {
                    stack.push(state)
                    state = .DoubleQuotation
                }
                cache.append(c)
                break
            case "\'":
                if state == .SingleQuotation {
                    guard let last = stack.pop() else {
                        throw Exception.InvalidSyntax
                    }
                    state = last
                } else {
                    stack.push(state)
                    state = .SingleQuotation
                }
                cache.append(c)
                break
            default:
                cache.append(c)
            }
        }
        guard state == .Value, let v = variable else {
            throw Exception.InvalidSyntax
        }
        return ContentType.Assignment(v, cache)
    }
    /// Constructor
    /// - parameters:
    ///   - path: path of INI file to load
    /// - throws:
    ///   Exception
    public init(_ path: String) throws {
        let data = try Data(contentsOf: URL(fileURLWithPath: path))
        guard let text = String(bytes: data, encoding: .utf8) else {
            throw Exception.InvalidFile
        }
        let lines: [String] = text.split(separator: "\n").map { String($0) }
        var title: String? = nil
        for line in lines {
            if let content = try parse(line: line) {
                switch content {
                case .Section(let newTitle):
                    title = newTitle
                    break
                case .Assignment(let variable, let value):
                    if let currentTitle = title {
                        if var sec = _sections[currentTitle] {
                            sec[variable] = value
                            _sections[currentTitle] = sec
                        } else {
                            var sec: [String: String] = [:]
                            sec[variable] = value
                            _sections[currentTitle] = sec
                        }
                    } else {
                        _anonymousSection[variable] = value
                    }
                    break
                }
            }
        }
    }
}
