cmake_minimum_required(VERSION 3.15)
project(KokoroCPP VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libkokoro only exports the C API; everything else stays internal.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if (MSVC)
    add_compile_options("/utf-8" "/EHsc" "/Zc:inline" "/permissive-" "/Ob2")
    add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Threads REQUIRED)

# ONNX Runtime Configuration
# Priority 1: -DONNXRUNTIME_ROOT (packaging/build-natives.ps1 passes one per RID)
# Priority 2: third_party/onnxruntime, downloaded by `uv run bootstrap.py configure`
# Priority 3: common system paths
set(LOCAL_ONNX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
if (DEFINED ONNXRUNTIME_ROOT)
    message(STATUS "Using ONNX Runtime from ONNXRUNTIME_ROOT: ${ONNXRUNTIME_ROOT}")
elseif (EXISTS "${LOCAL_ONNX_PATH}")
    message(STATUS "Using local ONNX Runtime from: ${LOCAL_ONNX_PATH}")
    set(ONNXRUNTIME_ROOT "${LOCAL_ONNX_PATH}")
elseif (APPLE)
    set(ONNXRUNTIME_ROOT "/opt/homebrew/opt/onnxruntime")
else()
    set(ONNXRUNTIME_ROOT "/usr/local")
endif()

message(STATUS "Looking for ONNX Runtime in: ${ONNXRUNTIME_ROOT}")

# Forget cached results that no longer exist (e.g. after the bundled runtime was replaced).
foreach(var ONNXRUNTIME_LIB ONNXRUNTIME_DLL)
    if (${var} AND NOT EXISTS "${${var}}")
        unset(${var} CACHE)
    endif()
endforeach()
# A source-built ORT puts its headers in include/onnxruntime/; the stale include/ dir still exists after the switch.
if (ONNXRUNTIME_INCLUDE_DIR AND NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    unset(ONNXRUNTIME_INCLUDE_DIR CACHE)
endif()

find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    HINTS ${ONNXRUNTIME_ROOT}/include
    PATH_SUFFIXES onnxruntime core/session
)

find_library(ONNXRUNTIME_LIB onnxruntime
    HINTS ${ONNXRUNTIME_ROOT}/lib
)

if (NOT (ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB))
    message(FATAL_ERROR "ONNX Runtime not found. Run `uv run bootstrap.py configure`, or set ONNXRUNTIME_ROOT, e.g. cmake -DONNXRUNTIME_ROOT=/path/to/onnxruntime ..")
endif()
message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIB}")

file(GLOB ONNXRUNTIME_CUDA_PROVIDER
    "${ONNXRUNTIME_ROOT}/lib/*onnxruntime_providers_cuda*"
    "${ONNXRUNTIME_ROOT}/bin/*onnxruntime_providers_cuda*")
if (ONNXRUNTIME_CUDA_PROVIDER)
    message(STATUS "ONNX Runtime has the CUDA execution provider")
else()
    message(STATUS "ONNX Runtime is CPU-only")
endif()

# Windows has no rpath: onnxruntime.dll has to sit next to every executable that loads kokoro.dll.
if (WIN32)
    get_filename_component(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_LIB}" DIRECTORY)
    find_file(ONNXRUNTIME_DLL onnxruntime.dll
        HINTS ${ONNXRUNTIME_LIB_DIR} ${ONNXRUNTIME_ROOT}/bin
        NO_DEFAULT_PATH
    )
    if (NOT ONNXRUNTIME_DLL)
        message(FATAL_ERROR "onnxruntime.dll not found next to ${ONNXRUNTIME_LIB}")
    endif()
    get_filename_component(ONNXRUNTIME_DLL_DIR "${ONNXRUNTIME_DLL}" DIRECTORY)
    # ORT loads providers from onnxruntime.dll's directory, so they must be copied together.
    # onnxruntime_providers_tensorrt.dll is deliberately left out: TensorRT is not used.
    # The prebuilt package puts the provider DLLs in bin/, a source build in lib/.
    set(ONNXRUNTIME_RUNTIME_DLLS "${ONNXRUNTIME_DLL}")
    foreach(provider shared cuda)
        foreach(dir "${ONNXRUNTIME_DLL_DIR}" "${ONNXRUNTIME_ROOT}/lib")
            if (EXISTS "${dir}/onnxruntime_providers_${provider}.dll")
                list(APPEND ONNXRUNTIME_RUNTIME_DLLS "${dir}/onnxruntime_providers_${provider}.dll")
                break()
            endif()
        endforeach()
    endforeach()
endif()

# Internal implementation, linked into libkokoro and test_g2p.
add_library(kokoro_core STATIC
    src/Kokoro.cpp
    src/PhonemeChunker.cpp
    src/Phonemizer.cpp
    src/PhonemeEncoder.cpp
    src/chinese_english/ChineseEnglishPhonemizer.cpp
    src/english/EnglishPhonemizer.cpp
    src/spanish/SpanishPhonemizer.cpp
    src/chinese_english/ZHG2P.cpp
    src/NumberNormalizer.cpp
    src/chinese_english/PinyinFinder.cpp
    src/chinese_english/ToneSandhi.cpp
    src/chinese_english/ZHFrontend.cpp
    src/NeuralG2P.cpp
    src/PhonemeText.cpp
    src/Utf8.cpp
    src/german/GermanG2P.cpp
    src/german/GermanLexicon.cpp
    src/german/GermanPhonemizer.cpp
    src/german/GermanPronunciation.cpp
    src/spanish/SpanishG2P.cpp
    src/spanish/SpanishLoanwords.cpp
)
target_include_directories(kokoro_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppjieba/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppjieba/deps/limonp/include
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# Public shared library: C API from include/kokoro/kokoro.h.
add_library(kokoro SHARED src/kokoro_c.cpp)
add_library(kokoro::kokoro ALIAS kokoro)
target_include_directories(kokoro PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(kokoro PRIVATE KOKORO_BUILD KOKORO_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(kokoro PRIVATE kokoro_core ${ONNXRUNTIME_LIB})
set_target_properties(kokoro PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if (WIN32)
    add_custom_command(TARGET kokoro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ONNXRUNTIME_RUNTIME_DLLS} $<TARGET_FILE_DIR:kokoro>)
    install(FILES ${ONNXRUNTIME_RUNTIME_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})
elseif (APPLE)
    target_link_libraries(kokoro PRIVATE "-framework Foundation" "-framework CoreFoundation")
    set_target_properties(kokoro PROPERTIES INSTALL_RPATH "@loader_path")
elseif (UNIX)
    set_target_properties(kokoro PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Command-line tool. Target name differs from the library; the executable is still named `kokoro`.
add_executable(kokoro_cli
    src/cli/main.cpp
    src/cli/console.cpp
    src/cli/interactive.cpp
    src/cli/audio_player.cpp
    third_party/miniaudio/miniaudio.c
)
target_include_directories(kokoro_cli SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio")
# Playback only: drop miniaudio's decoders, encoders, generators and high-level engine.
target_compile_definitions(kokoro_cli PRIVATE
    MA_NO_DECODING MA_NO_ENCODING MA_NO_GENERATION MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENGINE)
target_link_libraries(kokoro_cli PRIVATE kokoro Threads::Threads ${CMAKE_DL_LIBS})
if (UNIX AND NOT APPLE)
    target_link_libraries(kokoro_cli PRIVATE m)
endif()
set_target_properties(kokoro_cli PROPERTIES OUTPUT_NAME kokoro)
if (WIN32)
    target_link_libraries(kokoro_cli PRIVATE Shell32)   # CommandLineToArgvW
endif()
if (MSVC)
    # kokoro.exe and kokoro.dll share an output directory: keep their .pdb/.ilk apart.
    set_target_properties(kokoro_cli PROPERTIES PDB_NAME kokoro_cli)
    target_link_options(kokoro_cli PRIVATE "/INCREMENTAL:NO")
elseif (APPLE)
    set_target_properties(kokoro_cli PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
elseif (UNIX)
    set_target_properties(kokoro_cli PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(KOKORO_IS_TOP_LEVEL ON)
else()
    set(KOKORO_IS_TOP_LEVEL OFF)
endif()
# HTTP server: a Rust crate in src/server, built by cargo against libkokoro and copied next to it.
option(KOKORO_BUILD_SERVER "Build kokoro-server (needs a Rust toolchain)" ${KOKORO_IS_TOP_LEVEL})
if (KOKORO_BUILD_SERVER)
    find_program(CARGO cargo HINTS "$ENV{CARGO_HOME}/bin" "$ENV{HOME}/.cargo/bin" "$ENV{USERPROFILE}/.cargo/bin")
    if (NOT CARGO)
        message(FATAL_ERROR
            "cargo not found: install Rust (https://rustup.rs) or configure with -DKOKORO_BUILD_SERVER=OFF")
    endif()
    set(KOKORO_CARGO_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo")
    set(KOKORO_CARGO_PROFILE "$<IF:$<CONFIG:Debug>,dev,release>")
    set(KOKORO_CARGO_ENV
        "CARGO_TARGET_DIR=${KOKORO_CARGO_TARGET_DIR}"
        "KOKORO_LIB_DIR=$<TARGET_LINKER_FILE_DIR:kokoro>"
        "KOKORO_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}")
    set(KOKORO_SERVER_EXE "kokoro-server${CMAKE_EXECUTABLE_SUFFIX}")
    add_custom_target(kokoro_server ALL
        COMMAND ${CMAKE_COMMAND} -E env ${KOKORO_CARGO_ENV}
                "${CARGO}" build --locked --profile ${KOKORO_CARGO_PROFILE}
                --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/src/server/Cargo.toml"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${KOKORO_CARGO_TARGET_DIR}/$<IF:$<CONFIG:Debug>,debug,release>/${KOKORO_SERVER_EXE}"
                "$<TARGET_FILE_DIR:kokoro>/${KOKORO_SERVER_EXE}"
        COMMENT "Building kokoro-server with cargo"
        VERBATIM
    )
    add_dependencies(kokoro_server kokoro)
    install(PROGRAMS "$<TARGET_FILE_DIR:kokoro>/${KOKORO_SERVER_EXE}" DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Install: headers, libkokoro and a CMake package (find_package(kokoro) -> kokoro::kokoro).
install(TARGETS kokoro EXPORT kokoroTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(TARGETS kokoro_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/kokoro DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT kokoroTargets NAMESPACE kokoro:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kokoro)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/kokoroConfig.cmake
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/kokoroTargets.cmake\")\n")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/kokoroConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kokoroConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/kokoroConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kokoro
)

option(KOKORO_BUILD_TESTS "Build the kokoro tests" ${KOKORO_IS_TOP_LEVEL})
if (KOKORO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
option(KOKORO_BUILD_EVAL "Build the G2P evaluation tools in eval_bench/" ${KOKORO_IS_TOP_LEVEL})
if (KOKORO_BUILD_EVAL)
    add_subdirectory(eval_bench)
endif()

