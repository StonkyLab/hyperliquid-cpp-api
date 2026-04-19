cmake_minimum_required(VERSION 4.0)
project(hyperliquid_api)

set(CMAKE_CXX_STANDARD 20)

if (MSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00 /bigobj /utf-8 -DVERBOSE_LOG)
else ()
    add_definitions(-fPIC)
endif ()

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

find_package(Boost 1.88 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(magic_enum REQUIRED)

include_directories(include vk_cpp_common/include SYSTEM ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

if (NOT TARGET vk_common)
    add_subdirectory(vk_cpp_common)
endif ()

set(HEADERS
        include/vk/hyperliquid/hyperliquid.h)

set(SOURCES
        src/hyperliquid.cpp
        include/vk/hyperliquid/hyperliquid_models.h)

if (MODULE_MANAGER)
    add_library(hyperliquid_api SHARED ${SOURCES} ${HEADERS})
else ()
    add_library(hyperliquid_api STATIC ${SOURCES} ${HEADERS})

    add_executable(test test/main.cpp)
    target_link_libraries(test PRIVATE spdlog::spdlog_header_only hyperliquid_api OpenSSL::Crypto)

endif ()

target_link_libraries(hyperliquid_api PRIVATE spdlog::spdlog_header_only OpenSSL::Crypto OpenSSL::SSL vk_common nlohmann_json::nlohmann_json)

if (NOT MSVC)
    target_link_libraries(hyperliquid_api PRIVATE atomic)
endif ()