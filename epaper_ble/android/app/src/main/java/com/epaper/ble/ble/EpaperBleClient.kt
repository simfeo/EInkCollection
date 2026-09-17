package com.epaper.ble.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.UUID
import java.util.concurrent.ConcurrentLinkedQueue

/** See PROTOCOL.md — one service, a control characteristic and a data characteristic. */
object Protocol {
    val SERVICE: UUID = UUID.fromString("a1b2c000-5e1f-4a7d-9c3b-2f6e8d0a4b71")
    val CONTROL: UUID = UUID.fromString("a1b2c001-5e1f-4a7d-9c3b-2f6e8d0a4b71")
    val DATA: UUID = UUID.fromString("a1b2c002-5e1f-4a7d-9c3b-2f6e8d0a4b71")
    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    const val CMD_BEGIN: Byte = 0x01
    const val CMD_COMMIT: Byte = 0x02
    const val CMD_ABORT: Byte = 0x03
    const val CMD_STATUS: Byte = 0x04

    const val EVT_STATE: Byte = 0x10
    const val EVT_PROGRESS: Byte = 0x11
    const val EVT_OK: Byte = 0x12
    const val EVT_ERROR: Byte = 0x13

    fun errorText(reason: Int): String = when (reason) {
        1 -> "No transfer open"
        2 -> "Too much data"
        3 -> "Wrong frame length"
        4 -> "Checksum mismatch"
        5 -> "Storage error on the device"
        6 -> "Display is busy"
        else -> "Unknown error $reason"
    }
}

data class DiscoveredDevice(val address: String, val name: String, val rssi: Int)

sealed interface TransferState {
    data object Idle : TransferState
    data object Scanning : TransferState
    data object Connecting : TransferState
    data object Ready : TransferState
    data class Sending(val sent: Int, val total: Int) : TransferState
    data object Refreshing : TransferState
    data object Done : TransferState
    data class Failed(val message: String) : TransferState
}

/**
 * Minimal GATT client for the frame transfer. Every GATT operation is issued
 * from the main thread and the next one waits for the previous callback, which
 * is what the Android stack expects.
 */
@SuppressLint("MissingPermission")
class EpaperBleClient(private val context: Context) {

    private val handler = Handler(Looper.getMainLooper())

    private val _devices = MutableStateFlow<List<DiscoveredDevice>>(emptyList())
    val devices: StateFlow<List<DiscoveredDevice>> = _devices.asStateFlow()

    private val _state = MutableStateFlow<TransferState>(TransferState.Idle)
    val state: StateFlow<TransferState> = _state.asStateFlow()

    private var gatt: BluetoothGatt? = null
    private var control: BluetoothGattCharacteristic? = null
    private var data: BluetoothGattCharacteristic? = null

    private val pendingChunks = ConcurrentLinkedQueue<ByteArray>()
    private var frameSize = 0
    private var sentBytes = 0
    private var chunkSize = 20

    private val adapter: BluetoothAdapter?
        get() = (context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter

    val isBluetoothOn: Boolean get() = adapter?.isEnabled == true

    // -------------------------------------------------------------------------
    // Scanning
    // -------------------------------------------------------------------------

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = result.device.name ?: result.scanRecord?.deviceName ?: return
            val found = DiscoveredDevice(result.device.address, name, result.rssi)
            _devices.value = (_devices.value.filter { it.address != found.address } + found)
                .sortedByDescending { it.rssi }
        }

        override fun onScanFailed(errorCode: Int) {
            _state.value = TransferState.Failed("Scan failed (code $errorCode)")
        }
    }

    fun startScan() {
        val scanner = adapter?.bluetoothLeScanner ?: run {
            _state.value = TransferState.Failed("Bluetooth is off")
            return
        }
        _devices.value = emptyList()
        _state.value = TransferState.Scanning

        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(Protocol.SERVICE)).build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        scanner.startScan(listOf(filter), settings, scanCallback)

        handler.postDelayed({ stopScan() }, 15_000)
    }

    fun stopScan() {
        adapter?.bluetoothLeScanner?.stopScan(scanCallback)
        if (_state.value is TransferState.Scanning) _state.value = TransferState.Idle
    }

    // -------------------------------------------------------------------------
    // Connection
    // -------------------------------------------------------------------------

    fun connect(address: String) {
        stopScan()
        val device: BluetoothDevice = adapter?.getRemoteDevice(address) ?: run {
            _state.value = TransferState.Failed("Unknown device")
            return
        }
        _state.value = TransferState.Connecting
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        control = null
        data = null
        pendingChunks.clear()
        _state.value = TransferState.Idle
    }

    // -------------------------------------------------------------------------
    // Transfer
    // -------------------------------------------------------------------------

    fun sendFrame(frame: ByteArray, crc: Int) {
        val controlCharacteristic = control
        if (controlCharacteristic == null || gatt == null) {
            _state.value = TransferState.Failed("Not connected")
            return
        }

        pendingChunks.clear()
        var offset = 0
        while (offset < frame.size) {
            val end = minOf(offset + chunkSize, frame.size)
            pendingChunks.add(frame.copyOfRange(offset, end))
            offset = end
        }

        frameSize = frame.size
        sentBytes = 0
        _state.value = TransferState.Sending(0, frameSize)

        val begin = ByteArray(9)
        begin[0] = Protocol.CMD_BEGIN
        writeIntLe(begin, 1, frame.size)
        writeIntLe(begin, 5, crc)
        writeCharacteristic(controlCharacteristic, begin, withResponse = true)
    }

    private fun sendNextChunk() {
        val chunk = pendingChunks.poll()
        val dataCharacteristic = data
        if (chunk == null || dataCharacteristic == null) {
            val controlCharacteristic = control ?: return
            writeCharacteristic(
                controlCharacteristic,
                byteArrayOf(Protocol.CMD_COMMIT),
                withResponse = true
            )
            return
        }
        writeCharacteristic(dataCharacteristic, chunk, withResponse = true)
    }

    private fun writeCharacteristic(
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray,
        withResponse: Boolean
    ) {
        val currentGatt = gatt ?: return
        val writeType = if (withResponse) {
            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        } else {
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        }

        handler.post {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                currentGatt.writeCharacteristic(characteristic, value, writeType)
            } else {
                @Suppress("DEPRECATION")
                characteristic.writeType = writeType
                @Suppress("DEPRECATION")
                characteristic.value = value
                @Suppress("DEPRECATION")
                currentGatt.writeCharacteristic(characteristic)
            }
        }
    }

    private fun writeIntLe(target: ByteArray, offset: Int, value: Int) {
        target[offset] = (value and 0xFF).toByte()
        target[offset + 1] = (value shr 8 and 0xFF).toByte()
        target[offset + 2] = (value shr 16 and 0xFF).toByte()
        target[offset + 3] = (value shr 24 and 0xFF).toByte()
    }

    // -------------------------------------------------------------------------
    // GATT callbacks
    // -------------------------------------------------------------------------

    private val gattCallback = object : BluetoothGattCallback() {

        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                g.requestMtu(247)
            } else {
                val wasTransferring = _state.value is TransferState.Sending
                handler.post {
                    if (wasTransferring) {
                        _state.value = TransferState.Failed("Disconnected during transfer")
                    } else if (_state.value !is TransferState.Done) {
                        _state.value = TransferState.Idle
                    }
                }
                g.close()
                gatt = null
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            // Three bytes of ATT header come off the top of every write.
            chunkSize = (mtu - 3).coerceIn(20, 512)
            g.discoverServices()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val service = g.getService(Protocol.SERVICE)
            if (service == null) {
                handler.post { _state.value = TransferState.Failed("Service not found") }
                return
            }
            control = service.getCharacteristic(Protocol.CONTROL)
            data = service.getCharacteristic(Protocol.DATA)

            val controlCharacteristic = control
            if (controlCharacteristic == null || data == null) {
                handler.post { _state.value = TransferState.Failed("Characteristics not found") }
                return
            }

            g.setCharacteristicNotification(controlCharacteristic, true)
            val cccd = controlCharacteristic.getDescriptor(Protocol.CCCD)
            if (cccd != null) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                } else {
                    @Suppress("DEPRECATION")
                    cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    @Suppress("DEPRECATION")
                    g.writeDescriptor(cccd)
                }
            } else {
                handler.post { _state.value = TransferState.Ready }
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            handler.post { _state.value = TransferState.Ready }
        }

        override fun onCharacteristicWrite(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                handler.post { _state.value = TransferState.Failed("Write failed (status $status)") }
                return
            }

            when (characteristic.uuid) {
                Protocol.DATA -> {
                    sentBytes = minOf(sentBytes + chunkSize, frameSize)
                    handler.post { _state.value = TransferState.Sending(sentBytes, frameSize) }
                    sendNextChunk()
                }

                Protocol.CONTROL -> {
                    // BEGIN was acknowledged; COMMIT is answered by a notification.
                    if (pendingChunks.isNotEmpty()) sendNextChunk()
                }
            }
        }

        @Deprecated("Kept for API < 33")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            handleNotification(characteristic.value ?: return)
        }

        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleNotification(value)
        }
    }

    private fun handleNotification(value: ByteArray) {
        if (value.isEmpty()) return
        when (value[0]) {
            Protocol.EVT_OK -> handler.post { _state.value = TransferState.Done }

            Protocol.EVT_ERROR -> {
                val reason = if (value.size > 1) value[1].toInt() else 0
                handler.post { _state.value = TransferState.Failed(Protocol.errorText(reason)) }
            }

            Protocol.EVT_STATE -> {
                if (value.size > 1 && value[1].toInt() == 3) {
                    handler.post { _state.value = TransferState.Refreshing }
                }
            }
        }
    }
}
