find_package(Threads REQUIRED)

add_library(kokoro_doctest INTERFACE)
target_include_directories(kokoro_doctest SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/third_party/doctest")

set(KOKORO_TEST_MODEL "${PROJECT_SOURCE_DIR}/models/kokoro-v1.1-zh.onnx" CACHE FILEPATH "Model used by the synthesis test")
set(KOKORO_TEST_VOICES "${PROJECT_SOURCE_DIR}/models/voices-v1.1-zh.bin" CACHE FILEPATH "Voices file used by the synthesis test")
set(KOKORO_TEST_DICT_DIR "${PROJECT_SOURCE_DIR}/dict" CACHE PATH "Dictionary directory used by the tests")
set(KOKORO_TEST_MODEL_ES "${PROJECT_SOURCE_DIR}/models/kokoro-v1.0.onnx" CACHE FILEPATH
    "Model used by the Spanish synthesis test")
set(KOKORO_TEST_VOICES_ES "${PROJECT_SOURCE_DIR}/models/voices-v1.0-es.bin" CACHE FILEPATH
    "Voices file used by the Spanish synthesis test")
set(KOKORO_TEST_MODEL_DE "${PROJECT_SOURCE_DIR}/models/kokoro-de.onnx" CACHE FILEPATH
    "Model used by the German synthesis test")
set(KOKORO_TEST_VOICES_DE "${PROJECT_SOURCE_DIR}/models/voices-de.bin" CACHE FILEPATH
    "Voices file used by the German synthesis test")

add_executable(test_c_api test_c_api.cpp c_header_check.c)
target_link_libraries(test_c_api PRIVATE kokoro kokoro_doctest Threads::Threads)
target_compile_definitions(test_c_api PRIVATE KOKORO_EXPECTED_VERSION="${PROJECT_VERSION}")
add_test(NAME c_api COMMAND test_c_api)

add_executable(test_synthesis test_synthesis.cpp)
target_link_libraries(test_synthesis PRIVATE kokoro kokoro_doctest)
add_test(NAME synthesis COMMAND test_synthesis "${KOKORO_TEST_MODEL}" "${KOKORO_TEST_VOICES}" "${KOKORO_TEST_DICT_DIR}")
set_tests_properties(synthesis PROPERTIES SKIP_RETURN_CODE 77)

# kokoro-server: unit tests and the end-to-end HTTP test (skipped without the model) run under cargo.
if (KOKORO_BUILD_SERVER)
    set(server_test_env ${KOKORO_CARGO_ENV}
        "KOKORO_TEST_MODEL=${KOKORO_TEST_MODEL}"
        "KOKORO_TEST_VOICES=${KOKORO_TEST_VOICES}"
        "KOKORO_TEST_DICT_DIR=${KOKORO_TEST_DICT_DIR}")
    if (WIN32)
        # Windows has no rpath: put kokoro.dll and the ONNX Runtime DLLs next to cargo's test executables.
        add_custom_target(server_test_dlls ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${KOKORO_CARGO_TARGET_DIR}/$<IF:$<CONFIG:Debug>,debug,release>/deps"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:kokoro> ${ONNXRUNTIME_RUNTIME_DLLS}
                    "${KOKORO_CARGO_TARGET_DIR}/$<IF:$<CONFIG:Debug>,debug,release>/deps"
            VERBATIM)
        add_dependencies(server_test_dlls kokoro)
    endif()
    add_test(NAME server
        COMMAND ${CMAKE_COMMAND} -E env ${server_test_env}
                "${CARGO}" test --locked --profile ${KOKORO_CARGO_PROFILE}
                --manifest-path "${PROJECT_SOURCE_DIR}/src/server/Cargo.toml")
endif()

add_executable(test_synthesis_es test_synthesis_es.cpp)
target_link_libraries(test_synthesis_es PRIVATE kokoro kokoro_doctest)
add_test(NAME synthesis_es COMMAND test_synthesis_es "${KOKORO_TEST_MODEL_ES}" "${KOKORO_TEST_VOICES_ES}"
         "${KOKORO_TEST_DICT_DIR}")
set_tests_properties(synthesis_es PROPERTIES SKIP_RETURN_CODE 77)

add_executable(test_synthesis_de test_synthesis_de.cpp)
target_link_libraries(test_synthesis_de PRIVATE kokoro kokoro_doctest)
add_test(NAME synthesis_de COMMAND test_synthesis_de "${KOKORO_TEST_MODEL_DE}" "${KOKORO_TEST_VOICES_DE}"
         "${KOKORO_TEST_DICT_DIR}")
set_tests_properties(synthesis_de PROPERTIES SKIP_RETURN_CODE 77)

add_executable(test_g2p test_g2p.cpp)
target_link_libraries(test_g2p PRIVATE kokoro_core kokoro_doctest)
add_test(NAME g2p COMMAND test_g2p "${KOKORO_TEST_DICT_DIR}")

add_executable(test_chunker test_chunker.cpp)
target_link_libraries(test_chunker PRIVATE kokoro_core kokoro_doctest)
add_test(NAME chunker COMMAND test_chunker)

# Windows has no rpath: put kokoro.dll and the ONNX Runtime DLLs next to the test executables.
if (WIN32)
    foreach(test_target test_c_api test_synthesis test_synthesis_es test_synthesis_de)
        add_custom_command(TARGET ${test_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:kokoro> ${ONNXRUNTIME_RUNTIME_DLLS} $<TARGET_FILE_DIR:${test_target}>)
    endforeach()
endif()
