package io.github.larroy.kokoro

import android.content.Context
import android.content.res.AssetManager
import java.io.File
import java.io.IOException

/** The G2P dictionaries bundled in this library's assets. */
public object KokoroDict {
    private const val ASSET_DIR = "kokoro-dict"
    private const val STAMP = "kokoro-dict.stamp"

    /**
     * Extracts the bundled dictionaries to `noBackupFilesDir/kokoro-dict` unless that copy is already current, and
     * returns the directory; pass its `path` as `dictDir` to [KokoroContext]. The first call copies about 39 MB, so
     * call it off the main thread.
     *
     * @throws IOException the dictionaries could not be extracted.
     */
    @JvmStatic
    @Synchronized
    public fun install(context: Context): File {
        val assets = context.assets
        val expected = assets.open(STAMP).use { it.readBytes().decodeToString() }
        val root = context.noBackupFilesDir
        val target = File(root, ASSET_DIR)
        if (File(target, STAMP).takeIf { it.isFile }?.readText() == expected) return target

        val staging = File(root, "$ASSET_DIR.tmp")
        staging.deleteRecursively()
        copyAssetTree(assets, ASSET_DIR, staging)
        File(staging, STAMP).writeText(expected)
        target.deleteRecursively()
        if (!staging.renameTo(target)) throw IOException("Could not move $staging to $target")
        return target
    }

    // AssetManager.list returns no children for files; dict/ has no empty directories.
    private fun copyAssetTree(assets: AssetManager, path: String, dest: File) {
        val children = assets.list(path).orEmpty()
        if (children.isEmpty()) {
            dest.parentFile?.mkdirs()
            assets.open(path).use { input -> dest.outputStream().use { input.copyTo(it) } }
            return
        }
        dest.mkdirs()
        children.forEach { copyAssetTree(assets, "$path/$it", File(dest, it)) }
    }
}
