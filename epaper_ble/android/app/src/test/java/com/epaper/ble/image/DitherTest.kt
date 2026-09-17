package com.epaper.ble.image

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Test
import kotlin.math.roundToInt

class DitherTest {

    /** Canonical recursive construction, used as the reference. */
    private fun recursive(n: Int): IntArray {
        var matrix = arrayOf(intArrayOf(0))
        var size = 1
        while (size < n) {
            val top = matrix.map { row ->
                row.map { it * 4 }.toIntArray() + row.map { it * 4 + 2 }.toIntArray()
            }
            val bottom = matrix.map { row ->
                row.map { it * 4 + 3 }.toIntArray() + row.map { it * 4 + 1 }.toIntArray()
            }
            matrix = (top + bottom).toTypedArray()
            size *= 2
        }
        return matrix.flatMap { it.toList() }.toIntArray()
    }

    private fun ranks(size: Int): IntArray =
        Dither.makeBayer(size).map { (it / 255f * size * size - 0.5f).roundToInt() }.toIntArray()

    /**
     * Regression guard. Iterating the bits most-significant-first also yields a
     * plausible-looking matrix, but it clusters the low thresholds into one
     * quadrant and the result dithers in visible blocks.
     */
    @Test
    fun `bayer matrices match the canonical construction`() {
        for (size in intArrayOf(2, 4, 8)) {
            assertArrayEquals("size $size", recursive(size), ranks(size))
        }
    }

    @Test
    fun `bayer 4x4 has the documented first row`() {
        assertArrayEquals(intArrayOf(0, 8, 2, 10), ranks(4).copyOfRange(0, 4))
    }

    @Test
    fun `bayer thresholds are evenly spread, never clustered`() {
        // Each quadrant of the tile must carry an equal share of the low
        // thresholds; clustering is exactly what the broken bit order produced.
        val size = 8
        val values = ranks(size)
        val lowPerQuadrant = IntArray(4)
        for (y in 0 until size) {
            for (x in 0 until size) {
                if (values[y * size + x] < size * size / 4) {
                    val quadrant = (if (y >= size / 2) 2 else 0) + (if (x >= size / 2) 1 else 0)
                    lowPerQuadrant[quadrant]++
                }
            }
        }
        lowPerQuadrant.forEach { assertEquals(4, it) }
    }

    @Test
    fun `frame size matches the panel`() {
        assertEquals(4736, Dither.FRAME_BYTES)
        assertEquals(37, Dither.ROW_BYTES)
    }
}
