package com.epaper.ble

import android.app.Application
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.epaper.ble.ble.EpaperBleClient
import com.epaper.ble.ble.TransferState
import com.epaper.ble.image.Dither
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainViewModel(application: Application) : AndroidViewModel(application) {

    val client = EpaperBleClient(application)

    private val _settings = MutableStateFlow(Dither.Settings())
    val settings: StateFlow<Dither.Settings> = _settings.asStateFlow()

    private val _preview = MutableStateFlow<Bitmap?>(null)
    val preview: StateFlow<Bitmap?> = _preview.asStateFlow()

    private val _hasImage = MutableStateFlow(false)
    val hasImage: StateFlow<Boolean> = _hasImage.asStateFlow()

    private var source: Bitmap? = null
    private var frame: ByteArray? = null
    private var renderJob: Job? = null

    fun loadImage(uri: Uri) {
        viewModelScope.launch {
            val loaded = withContext(Dispatchers.IO) { decode(uri) }
            if (loaded == null) return@launch
            source?.recycle()
            source = loaded
            _hasImage.value = true
            render()
        }
    }

    /**
     * Photos are far larger than the panel, so sub-sample while decoding rather
     * than holding a 12 MP bitmap just to shrink it to 296x128.
     */
    private fun decode(uri: Uri): Bitmap? {
        val resolver = getApplication<Application>().contentResolver

        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        resolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it, null, bounds) }
        if (bounds.outWidth <= 0) return null

        var sample = 1
        while (bounds.outWidth / (sample * 2) >= Dither.WIDTH * 2 &&
            bounds.outHeight / (sample * 2) >= Dither.HEIGHT * 2
        ) {
            sample *= 2
        }

        val options = BitmapFactory.Options().apply {
            inSampleSize = sample
            inPreferredConfig = Bitmap.Config.ARGB_8888
        }
        return resolver.openInputStream(uri)?.use {
            BitmapFactory.decodeStream(it, null, options)
        }
    }

    fun update(transform: (Dither.Settings) -> Dither.Settings) {
        _settings.value = transform(_settings.value)
        render()
    }

    private fun render() {
        val current = source ?: return
        renderJob?.cancel()
        renderJob = viewModelScope.launch {
            val result = withContext(Dispatchers.Default) {
                Dither.process(current, _settings.value)
            }
            frame = result.frame
            _preview.value?.recycle()
            _preview.value = result.preview
        }
    }

    fun upload() {
        val payload = frame ?: return
        client.sendFrame(payload, Dither.crc32(payload))
    }

    val transferState: StateFlow<TransferState> get() = client.state

    override fun onCleared() {
        client.disconnect()
        source?.recycle()
        super.onCleared()
    }
}
