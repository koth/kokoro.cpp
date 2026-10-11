# libkokoro_jni.so binds these by name.
-keepclasseswithmembernames,includedescriptorclasses class io.github.larroy.kokoro.KokoroNative { native <methods>; }
-keep class io.github.larroy.kokoro.KokoroException { <init>(int, byte[]); }
-keep class io.github.larroy.kokoro.KokoroAudio { <init>(float[], int); }
