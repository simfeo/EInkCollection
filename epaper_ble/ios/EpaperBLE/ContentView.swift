import PhotosUI
import SwiftUI

struct ContentView: View {

    @StateObject private var client = EpaperBleClient()
    @State private var settings = Dither.Settings()
    @State private var pickerItem: PhotosPickerItem?
    @State private var source: UIImage?
    @State private var preview: UIImage?
    @State private var frame: Data?
    @State private var renderTask: Task<Void, Never>?

    var body: some View {
        NavigationStack {
            Form {
                previewSection
                pickerSection
                if source != nil { settingsSection }
                connectionSection
            }
            .navigationTitle("E-Paper BLE")
        }
        .onChange(of: pickerItem) { _, item in load(item) }
        .onChange(of: settings) { _, _ in render() }
    }

    // MARK: - Sections

    private var previewSection: some View {
        Section {
            ZStack {
                Color.white
                if let preview {
                    // Nearest neighbour, so the dot pattern stays honest when
                    // scaled up rather than being smoothed into fake greys.
                    Image(uiImage: preview)
                        .interpolation(.none)
                        .resizable()
                        .scaledToFit()
                } else {
                    Text(source == nil ? "No image selected" : "Rendering…")
                        .foregroundStyle(.secondary)
                }
            }
            .aspectRatio(CGFloat(Dither.width) / CGFloat(Dither.height), contentMode: .fit)
            .overlay(Rectangle().stroke(.gray.opacity(0.5)))
        } header: {
            HStack {
                Text("Preview")
                Spacer()
                Text("296 × 128 · 1 bit").font(.caption)
            }
        }
    }

    private var pickerSection: some View {
        Section {
            PhotosPicker(selection: $pickerItem, matching: .images) {
                Text(source == nil ? "Choose an image" : "Choose another image")
            }
        }
    }

    private var settingsSection: some View {
        Section("Processing") {
            Picker("Dithering", selection: $settings.algorithm) {
                ForEach(Dither.Algorithm.allCases) { algorithm in
                    Text(algorithm.label).tag(algorithm)
                }
            }

            Picker("Scaling", selection: $settings.fit) {
                Text("Fill").tag(Dither.Fit.cover)
                Text("Fit whole").tag(Dither.Fit.contain)
            }
            .pickerStyle(.segmented)

            Toggle("Rotate 90°", isOn: $settings.rotate)
            Toggle("Auto levels", isOn: $settings.autoLevels)
            Toggle("Invert", isOn: $settings.invert)

            slider("Brightness", value: $settings.brightness, range: -100...100,
                   text: String(Int(settings.brightness)))
            slider("Contrast", value: $settings.contrast, range: 0.5...2,
                   text: String(format: "%.2f", settings.contrast))
            slider("Sharpen", value: $settings.sharpen, range: 0...2,
                   text: String(format: "%.1f", settings.sharpen))

            if settings.algorithm == .threshold {
                slider("Black threshold", value: $settings.threshold, range: 20...235,
                       text: String(Int(settings.threshold)))
            }
        }
    }

    private var connectionSection: some View {
        Section("Display") {
            Text(client.state.text).font(.callout)

            if case .sending(let sent, let total) = client.state {
                ProgressView(value: Double(sent), total: Double(total))
            } else if client.state == .refreshing {
                ProgressView()
            }

            HStack {
                Button("Scan") { client.startScan() }
                    .disabled(client.state.isConnected)
                if client.state.isConnected {
                    Spacer()
                    Button("Disconnect", role: .destructive) { client.disconnect() }
                }
            }

            if !client.state.isConnected {
                ForEach(client.devices) { device in
                    Button {
                        client.connect(device.id)
                    } label: {
                        HStack {
                            Text(device.name)
                            Spacer()
                            Text("\(device.rssi) dBm").font(.caption).foregroundStyle(.secondary)
                        }
                    }
                }
            }

            Button("Show on e-paper") {
                if let frame { client.send(frame: frame) }
            }
            .disabled(frame == nil || !canUpload)
        }
    }

    private var canUpload: Bool {
        switch client.state {
        case .ready, .done, .failed: return true
        default: return false
        }
    }

    private func slider(
        _ label: String,
        value: Binding<Float>,
        range: ClosedRange<Float>,
        text: String
    ) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack {
                Text(label).font(.callout)
                Spacer()
                Text(text).font(.callout).foregroundStyle(.secondary)
            }
            Slider(value: value, in: range)
        }
    }

    // MARK: - Work

    private func load(_ item: PhotosPickerItem?) {
        guard let item else { return }
        Task {
            guard let data = try? await item.loadTransferable(type: Data.self),
                  let image = UIImage(data: data) else { return }
            source = downscale(image)
            render()
        }
    }

    /// Photos are far larger than the panel; shrink once up front rather than
    /// re-sampling a 12 MP bitmap on every slider move.
    private func downscale(_ image: UIImage) -> UIImage {
        let maxEdge: CGFloat = 1200
        let longest = max(image.size.width, image.size.height)
        guard longest > maxEdge else { return image }

        let scale = maxEdge / longest
        let size = CGSize(width: image.size.width * scale, height: image.size.height * scale)
        let format = UIGraphicsImageRendererFormat.default()
        format.scale = 1
        return UIGraphicsImageRenderer(size: size, format: format).image { _ in
            image.draw(in: CGRect(origin: .zero, size: size))
        }
    }

    private func render() {
        guard let source else { return }
        renderTask?.cancel()
        let current = settings
        renderTask = Task {
            let result = await Task.detached(priority: .userInitiated) {
                Dither.process(source, current)
            }.value
            guard !Task.isCancelled else { return }
            preview = result.preview
            frame = result.frame
        }
    }
}

#Preview {
    ContentView()
}
