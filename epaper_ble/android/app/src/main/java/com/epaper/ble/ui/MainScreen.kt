package com.epaper.ble.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import com.epaper.ble.MainViewModel
import com.epaper.ble.ble.TransferState
import com.epaper.ble.image.Dither
import androidx.compose.foundation.Image

@Composable
fun MainScreen(
    viewModel: MainViewModel,
    onRequestPermissions: () -> Unit,
    modifier: Modifier = Modifier
) {
    val settings by viewModel.settings.collectAsState()
    val preview by viewModel.preview.collectAsState()
    val hasImage by viewModel.hasImage.collectAsState()
    val devices by viewModel.client.devices.collectAsState()
    val transfer by viewModel.transferState.collectAsState()

    val picker = rememberLauncherForActivityResult(
        ActivityResultContracts.PickVisualMedia()
    ) { uri -> uri?.let(viewModel::loadImage) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text("E-Paper BLE", style = MaterialTheme.typography.headlineSmall)

        PreviewCard(preview, hasImage)

        Button(
            onClick = {
                picker.launch(
                    PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly)
                )
            },
            modifier = Modifier.fillMaxWidth()
        ) { Text(if (hasImage) "Choose another image" else "Choose an image") }

        if (hasImage) {
            SettingsCard(settings, viewModel)
        }

        ConnectionCard(
            devices = devices,
            transfer = transfer,
            hasImage = hasImage,
            onScan = {
                onRequestPermissions()
                viewModel.client.startScan()
            },
            onConnect = viewModel.client::connect,
            onDisconnect = viewModel.client::disconnect,
            onUpload = viewModel::upload
        )
    }
}

@Composable
private fun PreviewCard(preview: android.graphics.Bitmap?, hasImage: Boolean) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text("Preview", style = MaterialTheme.typography.titleMedium)
                Text("296 × 128 · 1 bit", style = MaterialTheme.typography.bodySmall)
            }

            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 8.dp)
                    .aspectRatio(Dither.WIDTH.toFloat() / Dither.HEIGHT)
                    .background(Color.White)
                    .border(1.dp, Color(0xFF999999)),
                contentAlignment = Alignment.Center
            ) {
                if (preview != null) {
                    // Nearest-neighbour, so the dot pattern stays honest when
                    // scaled up instead of being smoothed into fake greys.
                    Image(
                        bitmap = preview.asImageBitmap(),
                        contentDescription = "Dithered preview",
                        modifier = Modifier.fillMaxSize(),
                        contentScale = ContentScale.Fit,
                        filterQuality = FilterQuality.None
                    )
                } else {
                    Text(
                        if (hasImage) "Rendering…" else "No image selected",
                        color = Color.Gray
                    )
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SettingsCard(settings: Dither.Settings, viewModel: MainViewModel) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text("Processing", style = MaterialTheme.typography.titleMedium)

            var expanded by remember { mutableStateOf(false) }
            ExposedDropdownMenuBox(
                expanded = expanded,
                onExpandedChange = { expanded = it }
            ) {
                OutlinedTextField(
                    value = settings.algorithm.label,
                    onValueChange = {},
                    readOnly = true,
                    label = { Text("Dithering") },
                    trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
                    modifier = Modifier
                        .fillMaxWidth()
                        .menuAnchor(androidx.compose.material3.MenuAnchorType.PrimaryNotEditable)
                )
                ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                    Dither.Algorithm.entries.forEach { algorithm ->
                        DropdownMenuItem(
                            text = { Text(algorithm.label) },
                            onClick = {
                                viewModel.update { it.copy(algorithm = algorithm) }
                                expanded = false
                            }
                        )
                    }
                }
            }

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = settings.fit == Dither.Fit.COVER,
                    onClick = { viewModel.update { it.copy(fit = Dither.Fit.COVER) } },
                    label = { Text("Fill") }
                )
                FilterChip(
                    selected = settings.fit == Dither.Fit.CONTAIN,
                    onClick = { viewModel.update { it.copy(fit = Dither.Fit.CONTAIN) } },
                    label = { Text("Fit whole") }
                )
            }

            ToggleRow("Rotate 90°", settings.rotate) { v ->
                viewModel.update { it.copy(rotate = v) }
            }
            ToggleRow("Auto levels", settings.autoLevels) { v ->
                viewModel.update { it.copy(autoLevels = v) }
            }
            ToggleRow("Invert", settings.invert) { v ->
                viewModel.update { it.copy(invert = v) }
            }

            HorizontalDivider()

            LabelledSlider(
                label = "Brightness",
                value = settings.brightness.toFloat(),
                range = -100f..100f,
                display = settings.brightness.toString()
            ) { v -> viewModel.update { it.copy(brightness = v.toInt()) } }

            LabelledSlider(
                label = "Contrast",
                value = settings.contrast,
                range = 0.5f..2.0f,
                display = String.format("%.2f", settings.contrast)
            ) { v -> viewModel.update { it.copy(contrast = v) } }

            LabelledSlider(
                label = "Sharpen",
                value = settings.sharpen,
                range = 0f..2f,
                display = String.format("%.1f", settings.sharpen)
            ) { v -> viewModel.update { it.copy(sharpen = v) } }

            if (settings.algorithm == Dither.Algorithm.THRESHOLD) {
                LabelledSlider(
                    label = "Black threshold",
                    value = settings.threshold.toFloat(),
                    range = 20f..235f,
                    display = settings.threshold.toString()
                ) { v -> viewModel.update { it.copy(threshold = v.toInt()) } }
            }
        }
    }
}

@Composable
private fun ToggleRow(label: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(
        Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(label)
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

@Composable
private fun LabelledSlider(
    label: String,
    value: Float,
    range: ClosedFloatingPointRange<Float>,
    display: String,
    onChange: (Float) -> Unit
) {
    Column {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(label, style = MaterialTheme.typography.bodyMedium)
            Text(display, style = MaterialTheme.typography.bodyMedium)
        }
        Slider(value = value, onValueChange = onChange, valueRange = range)
    }
}

@Composable
private fun ConnectionCard(
    devices: List<com.epaper.ble.ble.DiscoveredDevice>,
    transfer: TransferState,
    hasImage: Boolean,
    onScan: () -> Unit,
    onConnect: (String) -> Unit,
    onDisconnect: () -> Unit,
    onUpload: () -> Unit
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text("Display", style = MaterialTheme.typography.titleMedium)
            Text(statusText(transfer), style = MaterialTheme.typography.bodyMedium)

            when (transfer) {
                is TransferState.Sending -> LinearProgressIndicator(
                    progress = { transfer.sent.toFloat() / transfer.total },
                    modifier = Modifier.fillMaxWidth()
                )

                is TransferState.Refreshing -> LinearProgressIndicator(
                    modifier = Modifier.fillMaxWidth()
                )

                else -> {}
            }

            val connected = transfer is TransferState.Ready ||
                transfer is TransferState.Sending ||
                transfer is TransferState.Refreshing ||
                transfer is TransferState.Done

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = onScan, enabled = !connected) { Text("Scan") }
                if (connected) {
                    OutlinedButton(onClick = onDisconnect) { Text("Disconnect") }
                }
            }

            if (!connected) {
                devices.forEach { device ->
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .clickable { onConnect(device.address) }
                            .padding(vertical = 8.dp),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(device.name)
                        Text("${device.rssi} dBm", style = MaterialTheme.typography.bodySmall)
                    }
                    HorizontalDivider()
                }
            }

            Button(
                onClick = onUpload,
                enabled = hasImage && (transfer is TransferState.Ready ||
                    transfer is TransferState.Done ||
                    transfer is TransferState.Failed),
                modifier = Modifier.fillMaxWidth()
            ) { Text("Show on e-paper") }
        }
    }
}

private fun statusText(state: TransferState): String = when (state) {
    TransferState.Idle -> "Not connected"
    TransferState.Scanning -> "Scanning…"
    TransferState.Connecting -> "Connecting…"
    TransferState.Ready -> "Connected"
    is TransferState.Sending -> "Sending ${state.sent} / ${state.total} bytes"
    TransferState.Refreshing -> "Refreshing the panel…"
    TransferState.Done -> "Shown on the display"
    is TransferState.Failed -> "Error: ${state.message}"
}
