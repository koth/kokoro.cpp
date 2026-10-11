package io.github.larroy.kokoro

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import org.junit.AfterClass
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Assume
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith

// Expectations mirror tests/test_synthesis.cpp, run through the JNI layer and the Kotlin API.
@RunWith(AndroidJUnit4::class)
class KokoroContextTest {
    companion object {
        private const val MODEL = "kokoro-v1.1-zh.onnx"
        private const val VOICES = "voices-v1.1-zh.bin"
        private const val VOICE = "zf_002"

        private lateinit var appContext: Context
        private lateinit var dictDir: File
        private lateinit var modelDir: File
        private var shared: KokoroContext? = null
        private val tts: KokoroContext get() = shared!!

        @BeforeClass
        @JvmStatic
        fun setUp() {
            appContext = InstrumentationRegistry.getInstrumentation().targetContext
            dictDir = KokoroDict.install(appContext)
            Assume.assumeTrue(
                "models missing: run uv run bootstrap.py configure",
                appContext.assets.list("models").orEmpty().toSet().containsAll(setOf(MODEL, VOICES)))
            modelDir = File(appContext.cacheDir, "models").apply { mkdirs() }
            listOf(MODEL, VOICES).forEach(::copyModel)
            shared = KokoroContext(File(modelDir, MODEL).path, File(modelDir, VOICES).path, dictDir.path)
        }

        @AfterClass
        @JvmStatic
        fun tearDown() {
            shared?.close()
        }

        private fun copyModel(name: String) {
            val dest = File(modelDir, name)
            val length = appContext.assets.openFd("models/$name").use { it.length }
            if (dest.length() == length) return
            appContext.assets.open("models/$name").use { input -> dest.outputStream().use { input.copyTo(it) } }
        }
    }

    @Test
    fun versionMatchesProject() {
        assertEquals(InstrumentationRegistry.getArguments().getString("kokoroVersion"), KokoroContext.version())
    }

    @Test
    fun missingModelMessageRoundTripsUtf8() {
        for (path in listOf("missing-model.onnx", "缺失的模型-你好.onnx", "modèle-😀.onnx")) {
            val e = assertThrows(KokoroException::class.java) {
                KokoroContext(path, "missing-voices.bin", dictDir.path)
            }
            assertEquals(KokoroStatus.LOAD, e.status)
            assertEquals("Model file not found: $path", e.message)
        }
    }

    @Test
    fun voicesAreSorted() {
        assertEquals(tts.voices.sorted(), tts.voices)
        assertTrue(tts.voices.containsAll(listOf("af_maple", VOICE)))
    }

    @Test
    fun phonemizeMatchesG2p() {
        assertEquals("ʈʂʊ→ŋkwo↗", tts.phonemize("中国"))
    }

    @Test
    fun synthesizeProducesSpeech() {
        val audio = tts.synthesize("你好，世界。", VOICE)
        assertEquals(24000, audio.sampleRateHz)
        assertTrue("${audio.samples.size} samples", audio.samples.size in 12000..240000)
        assertTrue(audio.samples.all { it.isFinite() })
    }

    @Test
    fun phonemeInputMatchesTextInput() {
        // The vocoder adds noise, but the duration predictor is deterministic: equal phonemes, equal length.
        val fromPhonemes = tts.synthesize(tts.phonemize("你好"), VOICE, inputIsPhonemes = true)
        val fromText = tts.synthesize("你好", VOICE)
        assertEquals(fromText.samples.size, fromPhonemes.samples.size)
    }

    @Test
    fun languageAndNumberLanguageAreApplied() {
        assertTrue(tts.phonemize("3").contains("sa→n"))
        try {
            tts.setLanguage(KokoroLanguage.ENGLISH)
            assertEquals("θɹˈi", tts.phonemize("3"))
        } finally {
            tts.setLanguage(KokoroLanguage.AUTO)
        }
        try {
            tts.setNumberLanguage(KokoroNumberLanguage.CHINESE)
            val chinese = tts.phonemize("I have 3 apples")
            assertTrue(chinese, chinese.contains("sa→n"))
            assertFalse(chinese, chinese.contains("θɹˈi"))
        } finally {
            tts.setNumberLanguage(KokoroNumberLanguage.AUTO)
        }
    }

    @Test
    fun unknownVoiceIsReported() {
        val e = assertThrows(KokoroException::class.java) { tts.synthesize("你好", "no_such_voice") }
        assertEquals(KokoroStatus.VOICE_NOT_FOUND, e.status)
    }

    @Test
    fun invalidSpeedIsRejected() {
        val e = assertThrows(KokoroException::class.java) { tts.synthesize("你好", VOICE, 0f) }
        assertEquals(KokoroStatus.INVALID_ARGUMENT, e.status)
    }

    @Test
    fun closedContextThrows() {
        val other = KokoroContext(File(modelDir, MODEL).path, File(modelDir, VOICES).path, dictDir.path)
        other.close()
        other.close()
        assertThrows(IllegalStateException::class.java) { other.phonemize("x") }
    }

    @Test
    fun dictInstallRepairsStaleStamp() {
        File(dictDir, "kokoro-dict.stamp").writeText("stale")
        val installed = KokoroDict.install(appContext)
        val expected = appContext.assets.open("kokoro-dict.stamp").use { it.readBytes().decodeToString() }
        assertEquals(expected, File(installed, "kokoro-dict.stamp").readText())
        assertTrue(File(installed, "vocab.txt").isFile)
    }
}
