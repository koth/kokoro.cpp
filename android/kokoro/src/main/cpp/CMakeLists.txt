cmake_minimum_required(VERSION 3.22.1)
project(kokoro_android LANGUAGES C CXX)

if (NOT KOKORO_ORT_AAR OR NOT EXISTS "${KOKORO_ORT_AAR}")
    message(FATAL_ERROR "KOKORO_ORT_AAR must name the onnxruntime-android AAR (set by android/kokoro/build.gradle.kts)")
endif()

# Per-ABI build dir, so concurrent ABI configures never share an extraction.
set(ONNXRUNTIME_ROOT "${CMAKE_BINARY_DIR}/onnxruntime-android")
set(ort_stamp "${ONNXRUNTIME_ROOT}/extracted-from.txt")
set(ort_extracted_from "")
if (EXISTS "${ort_stamp}")
    file(READ "${ort_stamp}" ort_extracted_from)
endif()
if (NOT "${ort_extracted_from}" STREQUAL "${KOKORO_ORT_AAR}")
    file(REMOVE_RECURSE "${ONNXRUNTIME_ROOT}")
    file(ARCHIVE_EXTRACT INPUT "${KOKORO_ORT_AAR}" DESTINATION "${ONNXRUNTIME_ROOT}"
         PATTERNS "headers/*" "jni/${ANDROID_ABI}/*")
    file(WRITE "${ort_stamp}" "${KOKORO_ORT_AAR}")
endif()

get_filename_component(KOKORO_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
add_subdirectory("${KOKORO_REPO_ROOT}" kokoro)

add_library(kokoro_jni SHARED kokoro_jni.cpp)
target_link_libraries(kokoro_jni PRIVATE kokoro::kokoro)

# c++_static and kokoro_core are archives: keep their symbols out of the dynamic tables so libkokoro.so exports only
# the C API and cannot interpose on another libc++ in the app.
foreach(target kokoro kokoro_jni)
    target_link_options(${target} PRIVATE "-Wl,--exclude-libs,ALL")
endforeach()
