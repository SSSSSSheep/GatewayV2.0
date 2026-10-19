cmake_minimum_required(VERSION 3.13)

project(gateway_v2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(
    GATEWAY_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

add_library(gateway_build_options INTERFACE)

target_compile_options(
    gateway_build_options
    INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wformat=2
)

if(GATEWAY_ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            gateway_build_options
            INTERFACE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
                -fno-sanitize-recover=all
        )

        target_link_options(
            gateway_build_options
            INTERFACE
                -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR
            "Sanitizers require GCC or Clang in this project"
        )
    endif()
endif()

enable_testing()

add_executable(
    test_smoke
    tests/unit/test_smoke.c
)

target_link_libraries(
    test_smoke
    PRIVATE
        gateway_build_options
)

add_test(
    NAME smoke
    COMMAND test_smoke
)

# ========= hex_decode 模块 =========
add_library(
    gateway_codec
    STATIC
        src/codec/hex_decode.c
)

target_include_directories(
    gateway_codec
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    gateway_codec
    PUBLIC
        gateway_build_options
)

add_executable(
    test_hex_decode
    tests/unit/test_hex_decode.c
)

target_link_libraries(
    test_hex_decode
    PRIVATE
        gateway_codec
)

add_test(
    NAME hex_decode
    COMMAND test_hex_decode
)

# --------------------------------------------------
# Third-party cJSON
# --------------------------------------------------

add_library(
    gateway_cjson
    STATIC
        third_party/cJSON/cJSON.c
)

target_include_directories(
    gateway_cjson
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON
)

if(GATEWAY_ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            gateway_cjson
            PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
                -fno-sanitize-recover=all
        )
    else()
        message(FATAL_ERROR
            "Sanitizers require GCC or Clang"
        )
    endif()
endif()

# --------------------------------------------------
# Add JSON implementation to the existing codec library
# --------------------------------------------------

target_sources(
    gateway_codec
    PRIVATE
        src/codec/message_json.c
)

target_link_libraries(
    gateway_codec
    PRIVATE
        gateway_cjson
)

# --------------------------------------------------
# JSON unit test
# --------------------------------------------------

add_executable(
    test_message_json
    tests/unit/test_message_json.c
)

target_link_libraries(
    test_message_json
    PRIVATE
        gateway_codec
)

add_test(
    NAME message_json
    COMMAND test_message_json
)

# --------------------------------------------------
# Protocol library
# --------------------------------------------------

add_library(
    gateway_protocol
    STATIC
        src/protocol/jdy_at.c
)

target_include_directories(
    gateway_protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    gateway_protocol
    PUBLIC
        gateway_build_options
)

# --------------------------------------------------
# JDY downlink encoding test
# --------------------------------------------------

add_executable(
    test_jdy_at
    tests/unit/test_jdy_at.c
)

target_link_libraries(
    test_jdy_at
    PRIVATE
        gateway_protocol
)

add_test(
    NAME jdy_at
    COMMAND test_jdy_at
)

target_sources(gateway_protocol
    PRIVATE
        src/protocol/jdy_parser.c
)

add_executable(test_jdy_parser
    tests/unit/test_jdy_parser.c
)

target_link_libraries(test_jdy_parser
    PRIVATE
        gateway_protocol
)

add_test(NAME jdy_parser COMMAND test_jdy_parser)

add_executable(main_demo
    src/main_demo.c
)

target_link_libraries(main_demo
    PRIVATE
        gateway_codec
        gateway_protocol
        gateway_build_options
)

add_library(gateway_device STATIC
    src/device/serial.c
)

target_include_directories(gateway_device
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gateway_device
    PUBLIC
        gateway_build_options
)

target_link_libraries(main_demo
    PRIVATE
        gateway_device
)