package com.epaper.ble.image

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Rect
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Port of the browser pipeline from the Wi-Fi uploader. Same stages in the same
 * order, so a frame prepared here is identical to one prepared by the web page.
 */
object Dither {

    const val WIDTH = 296
    const val HEIGHT = 128
    const val ROW_BYTES = (WIDTH + 7) / 8
    const val FRAME_BYTES = ROW_BYTES * HEIGHT

    enum class Fit { CONTAIN, COVER }

    enum class Algorithm(val label: String) {
        ATKINSON("Atkinson — contrasty"),
        FLOYD_STEINBERG("Floyd–Steinberg — accurate tone"),
        SIERRA2("Sierra-2"),
        JARVIS("Jarvis — soft"),
        BAYER2("Bayer 2×2 — coarse grid"),
        BAYER4("Bayer 4×4"),
        BAYER8("Bayer 8×8 — fine grid"),
        THRESHOLD("No dithering — threshold")
    }

    data class Settings(
        val algorithm: Algorithm = Algorithm.ATKINSON,
        val fit: Fit = Fit.COVER,
        val rotate: Boolean = false,
        val invert: Boolean = false,
        val autoLevels: Boolean = true,
        val brightness: Int = 0,
        val contrast: Float = 1.0f,
        val sharpen: Float = 0.6f,
        val threshold: Int = 128
    )

    /** dx, dy, weight — the divisor is carried alongside. */
    private class Kernel(val divisor: Int, val taps: IntArray)

    private val KERNELS = mapOf(
        Algorithm.FLOYD_STEINBERG to Kernel(16, intArrayOf(
            1, 0, 7, -1, 1, 3, 0, 1, 5, 1, 1, 1
        )),
        Algorithm.ATKINSON to Kernel(8, intArrayOf(
            1, 0, 1, 2, 0, 1, -1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 2, 1
        )),
        Algorithm.SIERRA2 to Kernel(16, intArrayOf(
            1, 0, 4, 2, 0, 3, -2, 1, 1, -1, 1, 2, 0, 1, 3, 1, 1, 2, 2, 1, 1
        )),
        Algorithm.JARVIS to Kernel(48, intArrayOf(
            1, 0, 7, 2, 0, 5,
            -2, 1, 3, -1, 1, 5, 0, 1, 7, 1, 1, 5, 2, 1, 3,
            -2, 2, 1, -1, 2, 3, 0, 2, 5, 1, 2, 3, 2, 2, 1
        ))
    )

    private val BAYER_SIZE = mapOf(
        Algorithm.BAYER2 to 2,
        Algorithm.BAYER4 to 4,
        Algorithm.BAYER8 to 8
    )

    /**
     * Ordered Bayer matrix, least significant bit first. Reversing the bit order
     * clusters the low thresholds into one quadrant and dithers in blocks.
     */
    internal fun makeBayer(size: Int): FloatArray {
        val bits = Integer.numberOfTrailingZeros(size)
        val matrix = FloatArray(size * size)
        for (y in 0 until size) {
            for (x in 0 until size) {
                var value = 0
                for (bit in 0 until bits) {
                    val xc = (x shr bit) and 1
                    val yc = (y shr bit) and 1
                    value = (value shl 2) or ((xc xor yc) shl 1) or yc
                }
                matrix[y * size + x] = (value + 0.5f) / (size * size) * 255f
            }
        }
        return matrix
    }

    private val BAYER = BAYER_SIZE.mapValues { makeBayer(it.value) }

    // -------------------------------------------------------------------------
    // Scaling
    // -------------------------------------------------------------------------

    /**
     * Halve repeatedly before the final draw. One huge downscale step drops
     * detail that the dither then cannot recover.
     */
    private fun shrink(source: Bitmap, targetWidth: Int): Bitmap {
        var current = source
        var width = source.width
        var height = source.height

        while (width / 2 > targetWidth && width > 2 && height > 2) {
            val nextWidth = max(1, width / 2)
            val nextHeight = max(1, height / 2)
            val step = Bitmap.createScaledBitmap(current, nextWidth, nextHeight, true)
            if (current !== source) current.recycle()
            current = step
            width = nextWidth
            height = nextHeight
        }
        return current
    }

    /** Source photo -> 296x128 grayscale values, honouring fit and rotation. */
    fun rasterize(source: Bitmap, settings: Settings): FloatArray {
        val canvasBitmap = Bitmap.createBitmap(WIDTH, HEIGHT, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(canvasBitmap)
        canvas.drawColor(android.graphics.Color.WHITE)

        val orientedWidth = if (settings.rotate) source.height else source.width
        val orientedHeight = if (settings.rotate) source.width else source.height

        val scale = when (settings.fit) {
            Fit.COVER -> max(WIDTH.toFloat() / orientedWidth, HEIGHT.toFloat() / orientedHeight)
            Fit.CONTAIN -> min(WIDTH.toFloat() / orientedWidth, HEIGHT.toFloat() / orientedHeight)
        }

        val shrunk = shrink(source, (source.width * scale).roundToInt().coerceAtLeast(1))
        val drawWidth = source.width * scale
        val drawHeight = source.height * scale

        val matrix = Matrix()
        matrix.postScale(drawWidth / shrunk.width, drawHeight / shrunk.height)
        if (settings.rotate) {
            matrix.postRotate(90f)
            matrix.postTranslate(drawHeight, 0f)
        }
        val placedWidth = if (settings.rotate) drawHeight else drawWidth
        val placedHeight = if (settings.rotate) drawWidth else drawHeight
        matrix.postTranslate((WIDTH - placedWidth) / 2f, (HEIGHT - placedHeight) / 2f)

        val paint = Paint(Paint.FILTER_BITMAP_FLAG or Paint.ANTI_ALIAS_FLAG)
        canvas.drawBitmap(shrunk, matrix, paint)
        if (shrunk !== source) shrunk.recycle()

        val pixels = IntArray(WIDTH * HEIGHT)
        canvasBitmap.getPixels(pixels, 0, WIDTH, 0, 0, WIDTH, HEIGHT)
        canvasBitmap.recycle()

        val values = FloatArray(WIDTH * HEIGHT)
        for (i in values.indices) {
            val p = pixels[i]
            var gray = ((p shr 16 and 0xFF) * 0.299f +
                (p shr 8 and 0xFF) * 0.587f +
                (p and 0xFF) * 0.114f)
            if (settings.invert) gray = 255f - gray
            values[i] = gray
        }
        return values
    }

    // -------------------------------------------------------------------------
    // Tone
    // -------------------------------------------------------------------------

    private fun stretchLevels(values: FloatArray) {
        val histogram = IntArray(256)
        for (v in values) histogram[v.roundToInt().coerceIn(0, 255)]++

        // Ignore the extreme 0.5% so a few specks cannot set the range.
        val cut = values.size * 0.005f
        var low = 0
        var high = 255
        var acc = 0f
        for (i in 0..255) {
            acc += histogram[i]
            if (acc > cut) { low = i; break }
        }
        acc = 0f
        for (i in 255 downTo 0) {
            acc += histogram[i]
            if (acc > cut) { high = i; break }
        }
        if (high - low < 32) return

        val scale = 255f / (high - low)
        for (i in values.indices) values[i] = (values[i] - low) * scale
    }

    private fun unsharpMask(values: FloatArray, amount: Float) {
        if (amount <= 0f) return

        val rowBlur = FloatArray(values.size)
        for (y in 0 until HEIGHT) {
            for (x in 0 until WIDTH) {
                val i = y * WIDTH + x
                val left = if (x > 0) values[i - 1] else values[i]
                val right = if (x < WIDTH - 1) values[i + 1] else values[i]
                rowBlur[i] = (left + values[i] + right) / 3f
            }
        }

        val blurred = FloatArray(values.size)
        for (y in 0 until HEIGHT) {
            for (x in 0 until WIDTH) {
                val i = y * WIDTH + x
                val up = if (y > 0) rowBlur[i - WIDTH] else rowBlur[i]
                val down = if (y < HEIGHT - 1) rowBlur[i + WIDTH] else rowBlur[i]
                blurred[i] = (up + rowBlur[i] + down) / 3f
            }
        }

        for (i in values.indices) values[i] += amount * (values[i] - blurred[i])
    }

    // -------------------------------------------------------------------------
    // Dithering
    // -------------------------------------------------------------------------

    /** Packed frame plus a preview bitmap, both from one pass. */
    class Result(val frame: ByteArray, val preview: Bitmap)

    fun process(source: Bitmap, settings: Settings): Result {
        val values = rasterize(source, settings)

        if (settings.autoLevels) stretchLevels(values)
        for (i in values.indices) {
            values[i] = 128f + (values[i] - 128f) * settings.contrast + settings.brightness
        }
        unsharpMask(values, settings.sharpen)

        val frame = ByteArray(FRAME_BYTES)
        val pixels = IntArray(WIDTH * HEIGHT)

        val kernel = KERNELS[settings.algorithm]
        val ordered = BAYER[settings.algorithm]
        val orderedSize = BAYER_SIZE[settings.algorithm] ?: 1
        val flatThreshold =
            if (settings.algorithm == Algorithm.THRESHOLD) settings.threshold.toFloat() else 128f

        for (y in 0 until HEIGHT) {
            // Serpentine scan: alternating direction hides the diagonal worming
            // that a plain left-to-right pass leaves in flat areas.
            val reversed = kernel != null && (y and 1) == 1
            for (step in 0 until WIDTH) {
                val x = if (reversed) WIDTH - 1 - step else step
                val index = y * WIDTH + x

                val limit = if (ordered != null) {
                    ordered[(y % orderedSize) * orderedSize + (x % orderedSize)]
                } else {
                    flatThreshold
                }

                val oldValue = values[index]
                val black = oldValue < limit
                val newValue = if (black) 0f else 255f

                if (black) {
                    frame[y * ROW_BYTES + (x shr 3)] =
                        (frame[y * ROW_BYTES + (x shr 3)].toInt() or (0x80 ushr (x and 7))).toByte()
                }
                val shade = newValue.toInt()
                pixels[index] = (0xFF shl 24) or (shade shl 16) or (shade shl 8) or shade

                if (kernel != null) {
                    // Error stays unclamped; clamping lets highlights and shadows
                    // swallow it and collapse into flat blobs.
                    val error = oldValue - newValue
                    var t = 0
                    while (t < kernel.taps.size) {
                        val dx = kernel.taps[t]
                        val dy = kernel.taps[t + 1]
                        val weight = kernel.taps[t + 2]
                        val tx = x + if (reversed) -dx else dx
                        val ty = y + dy
                        if (tx in 0 until WIDTH && ty < HEIGHT) {
                            values[ty * WIDTH + tx] += error * weight / kernel.divisor
                        }
                        t += 3
                    }
                }
            }
        }

        val preview = Bitmap.createBitmap(WIDTH, HEIGHT, Bitmap.Config.ARGB_8888)
        preview.setPixels(pixels, 0, WIDTH, 0, 0, WIDTH, HEIGHT)
        return Result(frame, preview)
    }

    fun crc32(data: ByteArray): Int {
        val crc = java.util.zip.CRC32()
        crc.update(data)
        return crc.value.toInt()
    }
}
