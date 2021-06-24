//
//  ExamplePluginSwift.swift
//  VisionCamera
//
//  Created by Marc Rousavy on 30.04.21.
//  Copyright © 2021 mrousavy. All rights reserved.
//

import AVKit
import Vision

@objc(ExamplePluginSwift)
public class ExamplePluginSwift: NSObject, FrameProcessorPluginBase {
    @objc
    public static func callback(_ frame: Frame!, withArgs args: [Any]!) -> Any! {
        guard let imageBuffer = CMSampleBufferGetImageBuffer(frame.buffer) else {
            return nil
        }
        NSLog("ExamplePlugin: \(CVPixelBufferGetWidth(imageBuffer)) x \(CVPixelBufferGetHeight(imageBuffer)) Image. Logging \(args.count) parameters:")

        args.forEach { arg in
            NSLog("ExamplePlugin:   -> \(arg) (\(type(of: arg)))")
        }

        return [
            "example_str": "Test",
            "example_bool": true,
            "example_double": 5.3,
            "example_array": [
                "Hello",
                true,
                17.38,
            ],
        ]
    }
}
