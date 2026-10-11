import java.security.MessageDigest
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    `maven-publish`
}

val repoRoot: File = rootDir.parentFile

fun readVersion(file: File, pattern: String, what: String): String =
    Regex(pattern, RegexOption.MULTILINE).find(file.readText())?.groupValues?.get(1)
        ?: throw GradleException("Could not read '$what' from $file")

val kokoroVersion = readVersion(
    repoRoot.resolve("CMakeLists.txt"), """project\(KokoroCPP VERSION (\d+\.\d+\.\d+)""",
    "project(KokoroCPP VERSION x.y.z)")
val ortVersion = readVersion(
    repoRoot.resolve("bootstrap.py"), """^ORT_VERSION = "(\d+\.\d+\.\d+)"""", "ORT_VERSION = \"x.y.z\"")

// Resolved at configuration time: AGP runs CMake configure during IDE sync, so the AAR must already exist.
val ortAar: File = configurations.detachedConfiguration(
    dependencies.create("com.microsoft.onnxruntime:onnxruntime-android:$ortVersion@aar"),
).apply { isTransitive = false }.singleFile

android {
    namespace = "io.github.larroy.kokoro"
    compileSdk = 35
    // NDK r28 links with 16 KB pages by default.
    ndkVersion = "28.2.13676358"

    defaultConfig {
        minSdk = 24
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        testInstrumentationRunnerArguments["kokoroVersion"] = kokoroVersion
        consumerProguardFiles("consumer-rules.pro")
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                // Only C types cross between libkokoro_jni, libkokoro and libonnxruntime, so a static libc++ per
                // library is safe and avoids shipping a libc++_shared.so that clashes with the app's.
                arguments += listOf("-DANDROID_STL=c++_static", "-DKOKORO_ORT_AAR=${ortAar.invariantSeparatorsPath}")
                targets += listOf("kokoro", "kokoro_jni")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    // Test models are stored uncompressed in the test APK.
    androidResources {
        noCompress += listOf("onnx", "bin")
    }

    publishing {
        singleVariant("release") {
            withSourcesJar()
        }
    }
}

kotlin {
    explicitApi()
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}

dependencies {
    implementation("com.microsoft.onnxruntime:onnxruntime-android:$ortVersion")
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("junit:junit:4.13.2")
}

/** Stages dict/ as assets under kokoro-dict/, plus kokoro-dict.stamp (a content hash KokoroDict.install compares). */
abstract class KokoroDictAssets : DefaultTask() {
    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val dictDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun stage() {
        val source = dictDir.get().asFile
        val output = outputDir.get().asFile
        output.listFiles()?.forEach { it.deleteRecursively() }
        val digest = MessageDigest.getInstance("SHA-256")
        source.walkTopDown().filter { it.isFile }.sortedBy { it.relativeTo(source).invariantSeparatorsPath }
            .forEach { file ->
                val relative = file.relativeTo(source).invariantSeparatorsPath
                val bytes = file.readBytes()
                output.resolve("kokoro-dict/$relative").apply { parentFile.mkdirs() }.writeBytes(bytes)
                digest.update(relative.toByteArray())
                digest.update(0)
                digest.update(bytes)
                digest.update(0)
            }
        // Outside kokoro-dict/ and without a leading dot: AAPT drops dotfiles from assets.
        output.resolve("kokoro-dict.stamp").writeText(digest.digest().joinToString("") { "%02x".format(it) } + "\n")
    }
}

/** Stages the synthesis test's model and voices as androidTest assets under models/; missing files are skipped. */
abstract class KokoroTestModels : DefaultTask() {
    @get:InputFiles
    @get:PathSensitive(PathSensitivity.NAME_ONLY)
    abstract val models: ConfigurableFileCollection

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun stage() {
        val output = outputDir.get().asFile
        output.listFiles()?.forEach { it.deleteRecursively() }
        val target = output.resolve("models").apply { mkdirs() }
        models.filter { it.isFile }.forEach { it.copyTo(target.resolve(it.name)) }
    }
}

androidComponents {
    onVariants { variant ->
        val cap = variant.name.replaceFirstChar { it.uppercase() }
        val dict = tasks.register<KokoroDictAssets>("stage${cap}KokoroDict") {
            dictDir.set(repoRoot.resolve("dict"))
        }
        variant.sources.assets?.addGeneratedSourceDirectory(dict, KokoroDictAssets::outputDir)
        val models = tasks.register<KokoroTestModels>("stage${cap}KokoroTestModels") {
            this.models.from(
                repoRoot.resolve("models/kokoro-v1.1-zh.onnx"), repoRoot.resolve("models/voices-v1.1-zh.bin"))
        }
        variant.androidTest?.sources?.assets?.addGeneratedSourceDirectory(models, KokoroTestModels::outputDir)
    }
}

publishing {
    publications {
        register<MavenPublication>("release") {
            groupId = "io.github.larroy"
            artifactId = "kokoro-android"
            version = kokoroVersion
            afterEvaluate {
                from(components["release"])
            }
            pom {
                name.set("kokoro-android")
                description.set(
                    "Kokoro TTS (kokoro.cpp) for Android: Kotlin API over the kokoro C API. " +
                        "ONNX Runtime comes from onnxruntime-android.")
                url.set("https://github.com/larroy/kokoro.cpp")
                licenses {
                    license {
                        name.set("MIT")
                        url.set("https://opensource.org/licenses/MIT")
                    }
                }
            }
        }
    }
    repositories {
        maven {
            name = "local"
            url = uri(layout.buildDirectory.dir("repo"))
        }
    }
}
