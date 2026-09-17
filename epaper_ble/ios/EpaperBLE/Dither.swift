import UIKit

/// Port of the browser pipeline from the Wi-Fi uploader, matching the Android
/// app stage for stage so a frame prepared here is byte-identical.
enum Dither {

    static let width = 296
    static let height = 128
    static let rowBytes = (296 + 7) / 8
    static let frameBytes = rowBytes * 128

    enum Fit: String, CaseIterable {
        case cover, contain
    }

    enum Algorithm: String, CaseIterable, Identifiable {
        case atkinson, floydSteinberg, sierra2, jarvis, bayer2, bayer4, bayer8, threshold

        var id: String { rawValue }

        var label: String {
            switch self {
            case .atkinson: return "Atkinson — contrasty"
            case .floydSteinberg: return "Floyd–Steinberg — accurate tone"
            case .sierra2: return "Sierra-2"
            case .jarvis: return "Jarvis — soft"
            case .bayer2: return "Bayer 2×2 — coarse grid"
            case .bayer4: return "Bayer 4×4"
            case .bayer8: return "Bayer 8×8 — fine grid"
            case .threshold: return "No dithering — threshold"
            }
        }
    }

    struct Settings: Equatable {
        var algorithm: Algorithm = .atkinson
        var fit: Fit = .cover
        var rotate = false
        var invert = false
        var autoLevels = true
        var brightness: Float = 0
        var contrast: Float = 1
        var sharpen: Float = 0.6
        var threshold: Float = 128
    }

    // MARK: - Kernels

    /// Flat triples of dx, dy, weight, with a shared divisor.
    private struct Kernel {
        let divisor: Float
        let taps: [Int]
    }

    private static let kernels: [Algorithm: Kernel] = [
        .floydSteinberg: Kernel(divisor: 16, taps: [
            1, 0, 7, -1, 1, 3, 0, 1, 5, 1, 1, 1
        ]),
        .atkinson: Kernel(divisor: 8, taps: [
            1, 0, 1, 2, 0, 1, -1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 2, 1
        ]),
        .sierra2: Kernel(divisor: 16, taps: [
            1, 0, 4, 2, 0, 3, -2, 1, 1, -1, 1, 2, 0, 1, 3, 1, 1, 2, 2, 1, 1
        ]),
        .jarvis: Kernel(divisor: 48, taps: [
            1, 0, 7, 2, 0, 5,
            -2, 1, 3, -1, 1, 5, 0, 1, 7, 1, 1, 5, 2, 1, 3,
            -2, 2, 1, -1, 2, 3, 0, 2, 5, 1, 2, 3, 2, 2, 1
        ])
    ]

    private static let bayerSize: [Algorithm: Int] = [
        .bayer2: 2, .bayer4: 4, .bayer8: 8
    ]

    /// Ordered Bayer matrix, least significant bit first. Reversing the bit
    /// order clusters the low thresholds into one quadrant, which dithers in
    /// visible blocks instead of an even grid.
    static func makeBayer(_ size: Int) -> [Float] {
        let bits = Int(log2(Double(size)))
        var matrix = [Float](repeating: 0, count: size * size)
        for y in 0..<size {
            for x in 0..<size {
                var value = 0
                for bit in 0..<bits {
                    let xc = (x >> bit) & 1
                    let yc = (y >> bit) & 1
                    value = (value << 2) | ((xc ^ yc) << 1) | yc
                }
                matrix[y * size + x] = (Float(value) + 0.5) / Float(size * size) * 255
            }
        }
        return matrix
    }

    private static let bayer: [Algorithm: [Float]] = bayerSize.mapValues { makeBayer($0) }

    // MARK: - Rasterising

    /// Source photo to 296x128 grayscale, honouring fit and rotation.
    private static func rasterize(_ image: UIImage, _ settings: Settings) -> [Float] {
        let w = CGFloat(width)
        let h = CGFloat(height)

        let format = UIGraphicsImageRendererFormat.default()
        format.scale = 1
        format.opaque = true

        let rendered = UIGraphicsImageRenderer(size: CGSize(width: w, height: h), format: format)
            .image { context in
                UIColor.white.setFill()
                context.fill(CGRect(x: 0, y: 0, width: w, height: h))

                let sourceWidth = image.size.width
                let sourceHeight = image.size.height
                let orientedWidth = settings.rotate ? sourceHeight : sourceWidth
                let orientedHeight = settings.rotate ? sourceWidth : sourceHeight

                let scale: CGFloat
                switch settings.fit {
                case .cover: scale = max(w / orientedWidth, h / orientedHeight)
                case .contain: scale = min(w / orientedWidth, h / orientedHeight)
                }

                let drawWidth = sourceWidth * scale
                let drawHeight = sourceHeight * scale

                let cg = context.cgContext
                cg.interpolationQuality = .high
                cg.translateBy(x: w / 2, y: h / 2)
                if settings.rotate { cg.rotate(by: .pi / 2) }
                image.draw(in: CGRect(x: -drawWidth / 2, y: -drawHeight / 2,
                                      width: drawWidth, height: drawHeight))
            }

        // Re-draw into an 8-bit gray buffer so the luminance is already done.
        var gray = [UInt8](repeating: 255, count: width * height)
        gray.withUnsafeMutableBytes { raw in
            guard let context = CGContext(
                data: raw.baseAddress,
                width: width,
                height: height,
                bitsPerComponent: 8,
                bytesPerRow: width,
                space: CGColorSpaceCreateDeviceGray(),
                bitmapInfo: CGImageAlphaInfo.none.rawValue
            ), let cgImage = rendered.cgImage else { return }
            context.draw(cgImage, in: CGRect(x: 0, y: 0, width: w, height: h))
        }

        var values = [Float](repeating: 0, count: width * height)
        for i in 0..<values.count {
            let v = Float(gray[i])
            values[i] = settings.invert ? 255 - v : v
        }
        return values
    }

    // MARK: - Tone

    private static func stretchLevels(_ values: inout [Float]) {
        var histogram = [Int](repeating: 0, count: 256)
        for v in values {
            histogram[max(0, min(255, Int(v.rounded())))] += 1
        }

        // Ignore the extreme 0.5% so a few specks cannot set the range.
        let cut = Float(values.count) * 0.005
        var low = 0
        var high = 255
        var acc: Float = 0
        for i in 0...255 {
            acc += Float(histogram[i])
            if acc > cut { low = i; break }
        }
        acc = 0
        for i in stride(from: 255, through: 0, by: -1) {
            acc += Float(histogram[i])
            if acc > cut { high = i; break }
        }
        guard high - low >= 32 else { return }

        let scale = 255 / Float(high - low)
        for i in values.indices {
            values[i] = (values[i] - Float(low)) * scale
        }
    }

    private static func unsharpMask(_ values: inout [Float], amount: Float) {
        guard amount > 0 else { return }

        var rowBlur = [Float](repeating: 0, count: values.count)
        for y in 0..<height {
            for x in 0..<width {
                let i = y * width + x
                let left = x > 0 ? values[i - 1] : values[i]
                let right = x < width - 1 ? values[i + 1] : values[i]
                rowBlur[i] = (left + values[i] + right) / 3
            }
        }

        var blurred = [Float](repeating: 0, count: values.count)
        for y in 0..<height {
            for x in 0..<width {
                let i = y * width + x
                let up = y > 0 ? rowBlur[i - width] : rowBlur[i]
                let down = y < height - 1 ? rowBlur[i + width] : rowBlur[i]
                blurred[i] = (up + rowBlur[i] + down) / 3
            }
        }

        for i in values.indices {
            values[i] += amount * (values[i] - blurred[i])
        }
    }

    // MARK: - Dithering

    struct Result {
        let frame: Data
        let preview: UIImage
    }

    static func process(_ image: UIImage, _ settings: Settings) -> Result {
        var values = rasterize(image, settings)

        if settings.autoLevels { stretchLevels(&values) }
        for i in values.indices {
            values[i] = 128 + (values[i] - 128) * settings.contrast + settings.brightness
        }
        unsharpMask(&values, amount: settings.sharpen)

        var frame = [UInt8](repeating: 0, count: frameBytes)
        var pixels = [UInt8](repeating: 0, count: width * height * 4)

        let kernel = kernels[settings.algorithm]
        let ordered = bayer[settings.algorithm]
        let orderedSize = bayerSize[settings.algorithm] ?? 1
        let flatThreshold: Float = settings.algorithm == .threshold ? settings.threshold : 128

        for y in 0..<height {
            // Serpentine scan: alternating direction hides the diagonal worming
            // a plain left-to-right pass leaves in flat areas.
            let reversed = kernel != nil && (y & 1) == 1
            for step in 0..<width {
                let x = reversed ? width - 1 - step : step
                let index = y * width + x

                let limit: Float
                if let ordered {
                    limit = ordered[(y % orderedSize) * orderedSize + (x % orderedSize)]
                } else {
                    limit = flatThreshold
                }

                let oldValue = values[index]
                let isBlack = oldValue < limit
                let newValue: Float = isBlack ? 0 : 255

                if isBlack {
                    frame[y * rowBytes + (x >> 3)] |= UInt8(0x80 >> (x & 7))
                }

                let shade = UInt8(newValue)
                let p = index * 4
                pixels[p] = shade
                pixels[p + 1] = shade
                pixels[p + 2] = shade
                pixels[p + 3] = 255

                if let kernel {
                    // Error stays unclamped; clamping lets highlights and
                    // shadows swallow it and collapse into flat blobs.
                    let error = oldValue - newValue
                    var t = 0
                    while t < kernel.taps.count {
                        let dx = kernel.taps[t]
                        let dy = kernel.taps[t + 1]
                        let weight = Float(kernel.taps[t + 2])
                        let tx = x + (reversed ? -dx : dx)
                        let ty = y + dy
                        if tx >= 0 && tx < width && ty < height {
                            values[ty * width + tx] += error * weight / kernel.divisor
                        }
                        t += 3
                    }
                }
            }
        }

        return Result(frame: Data(frame), preview: makeImage(pixels))
    }

    private static func makeImage(_ rgba: [UInt8]) -> UIImage {
        var bytes = rgba
        let image = bytes.withUnsafeMutableBytes { raw -> UIImage? in
            guard let context = CGContext(
                data: raw.baseAddress,
                width: width,
                height: height,
                bitsPerComponent: 8,
                bytesPerRow: width * 4,
                space: CGColorSpaceCreateDeviceRGB(),
                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
            ), let cgImage = context.makeImage() else { return nil }
            return UIImage(cgImage: cgImage)
        }
        return image ?? UIImage()
    }

    /// CRC-32, matching zlib and the firmware's own implementation.
    static func crc32(_ data: Data) -> UInt32 {
        var table = [UInt32](repeating: 0, count: 256)
        for i in 0..<256 {
            var c = UInt32(i)
            for _ in 0..<8 {
                c = (c & 1) != 0 ? (0xEDB8_8320 ^ (c >> 1)) : (c >> 1)
            }
            table[i] = c
        }
        var crc: UInt32 = 0xFFFF_FFFF
        for byte in data {
            crc = table[Int((crc ^ UInt32(byte)) & 0xFF)] ^ (crc >> 8)
        }
        return crc ^ 0xFFFF_FFFF
    }
}
