import CoreBluetooth
import Foundation

/// See PROTOCOL.md — one service, a control characteristic and a data one.
enum Protocolo {
    static let service = CBUUID(string: "a1b2c000-5e1f-4a7d-9c3b-2f6e8d0a4b71")
    static let control = CBUUID(string: "a1b2c001-5e1f-4a7d-9c3b-2f6e8d0a4b71")
    static let data = CBUUID(string: "a1b2c002-5e1f-4a7d-9c3b-2f6e8d0a4b71")

    static let cmdBegin: UInt8 = 0x01
    static let cmdCommit: UInt8 = 0x02
    static let cmdAbort: UInt8 = 0x03

    static let evtState: UInt8 = 0x10
    static let evtProgress: UInt8 = 0x11
    static let evtOk: UInt8 = 0x12
    static let evtError: UInt8 = 0x13

    static func errorText(_ reason: Int) -> String {
        switch reason {
        case 1: return "No transfer open"
        case 2: return "Too much data"
        case 3: return "Wrong frame length"
        case 4: return "Checksum mismatch"
        case 5: return "Storage error on the device"
        case 6: return "Display is busy"
        default: return "Unknown error \(reason)"
        }
    }
}

struct DiscoveredDevice: Identifiable, Equatable {
    let id: UUID
    let name: String
    let rssi: Int
}

enum TransferState: Equatable {
    case idle
    case scanning
    case connecting
    case ready
    case sending(sent: Int, total: Int)
    case refreshing
    case done
    case failed(String)

    var text: String {
        switch self {
        case .idle: return "Not connected"
        case .scanning: return "Scanning…"
        case .connecting: return "Connecting…"
        case .ready: return "Connected"
        case .sending(let sent, let total): return "Sending \(sent) / \(total) bytes"
        case .refreshing: return "Refreshing the panel…"
        case .done: return "Shown on the display"
        case .failed(let message): return "Error: \(message)"
        }
    }

    var isConnected: Bool {
        switch self {
        case .ready, .sending, .refreshing, .done: return true
        default: return false
        }
    }
}

@MainActor
final class EpaperBleClient: NSObject, ObservableObject {

    @Published private(set) var devices: [DiscoveredDevice] = []
    @Published private(set) var state: TransferState = .idle

    private var manager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var control: CBCharacteristic?
    private var data: CBCharacteristic?
    private var discovered: [UUID: CBPeripheral] = [:]

    private var chunks: [Data] = []
    private var chunkIndex = 0
    private var sentBytes = 0
    private var frameSize = 0

    override init() {
        super.init()
        manager = CBCentralManager(delegate: self, queue: .main)
    }

    func startScan() {
        guard manager.state == .poweredOn else {
            state = .failed("Bluetooth is off")
            return
        }
        devices = []
        discovered = [:]
        state = .scanning
        manager.scanForPeripherals(withServices: [Protocolo.service])

        // Scanning forever is a battery drain; the user can scan again.
        DispatchQueue.main.asyncAfter(deadline: .now() + 15) { [weak self] in
            self?.stopScan()
        }
    }

    func stopScan() {
        manager.stopScan()
        if state == .scanning { state = .idle }
    }

    func connect(_ id: UUID) {
        guard let target = discovered[id] else {
            state = .failed("Unknown device")
            return
        }
        stopScan()
        state = .connecting
        peripheral = target
        target.delegate = self
        manager.connect(target)
    }

    func disconnect() {
        if let peripheral { manager.cancelPeripheralConnection(peripheral) }
        peripheral = nil
        control = nil
        data = nil
        chunks = []
        state = .idle
    }

    func send(frame: Data) {
        guard let peripheral, let control else {
            state = .failed("Not connected")
            return
        }

        // Three bytes of ATT header come off the top of every write.
        let chunkSize = min(peripheral.maximumWriteValueLength(for: .withResponse), 244)
        chunks = stride(from: 0, to: frame.count, by: chunkSize).map {
            frame.subdata(in: $0..<min($0 + chunkSize, frame.count))
        }
        chunkIndex = 0
        sentBytes = 0
        frameSize = frame.count
        state = .sending(sent: 0, total: frameSize)

        let crc = Dither.crc32(frame)
        var payload = Data([Protocolo.cmdBegin])
        payload.append(contentsOf: withUnsafeBytes(of: UInt32(frame.count).littleEndian) { Array($0) })
        payload.append(contentsOf: withUnsafeBytes(of: crc.littleEndian) { Array($0) })
        peripheral.writeValue(payload, for: control, type: .withResponse)
    }

    private func sendNextChunk() {
        guard let peripheral, let data, let control else { return }
        if chunkIndex < chunks.count {
            peripheral.writeValue(chunks[chunkIndex], for: data, type: .withResponse)
        } else {
            peripheral.writeValue(Data([Protocolo.cmdCommit]), for: control, type: .withResponse)
        }
    }
}

extension EpaperBleClient: CBCentralManagerDelegate {

    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            if central.state != .poweredOn, self.state == .scanning {
                self.state = .failed("Bluetooth is off")
            }
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let name = peripheral.name
            ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String
            ?? "Unknown"
        let found = DiscoveredDevice(id: peripheral.identifier, name: name, rssi: RSSI.intValue)
        Task { @MainActor in
            self.discovered[peripheral.identifier] = peripheral
            self.devices = (self.devices.filter { $0.id != found.id } + [found])
                .sorted { $0.rssi > $1.rssi }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([Protocolo.service])
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor in self.state = .failed("Could not connect") }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor in
            if case .sending = self.state {
                self.state = .failed("Disconnected during transfer")
            } else if self.state != .done {
                self.state = .idle
            }
        }
    }
}

extension EpaperBleClient: CBPeripheralDelegate {

    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == Protocolo.service }) else {
            Task { @MainActor in self.state = .failed("Service not found") }
            return
        }
        peripheral.discoverCharacteristics([Protocolo.control, Protocolo.data], for: service)
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        let found = service.characteristics ?? []
        let controlCharacteristic = found.first { $0.uuid == Protocolo.control }
        let dataCharacteristic = found.first { $0.uuid == Protocolo.data }

        guard let controlCharacteristic, let dataCharacteristic else {
            Task { @MainActor in self.state = .failed("Characteristics not found") }
            return
        }

        Task { @MainActor in
            self.control = controlCharacteristic
            self.data = dataCharacteristic
        }
        // The firmware relies on the CCCD NimBLE creates automatically.
        peripheral.setNotifyValue(true, for: controlCharacteristic)
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateNotificationStateFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        Task { @MainActor in
            if let error {
                self.state = .failed("Could not enable notifications: \(error.localizedDescription)")
            } else {
                self.state = .ready
            }
        }
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        if let error {
            Task { @MainActor in self.state = .failed("Write failed: \(error.localizedDescription)") }
            return
        }
        let isData = characteristic.uuid == Protocolo.data
        Task { @MainActor in
            if isData {
                self.sentBytes = min(self.sentBytes + (self.chunks.first?.count ?? 0), self.frameSize)
                self.chunkIndex += 1
                self.state = .sending(sent: self.sentBytes, total: self.frameSize)
                self.sendNextChunk()
            } else if self.chunkIndex < self.chunks.count {
                // BEGIN was acknowledged; COMMIT is answered by a notification.
                self.sendNextChunk()
            }
        }
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        guard let value = characteristic.value, let opcode = value.first else { return }
        let bytes = [UInt8](value)
        Task { @MainActor in
            switch opcode {
            case Protocolo.evtOk:
                self.state = .done
            case Protocolo.evtError:
                self.state = .failed(Protocolo.errorText(bytes.count > 1 ? Int(bytes[1]) : 0))
            case Protocolo.evtState:
                if bytes.count > 1, bytes[1] == 3 { self.state = .refreshing }
            default:
                break
            }
        }
    }
}
