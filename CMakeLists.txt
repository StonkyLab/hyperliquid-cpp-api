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

include_directories(include stonky-cpp-common/include SYSTEM ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

if (NOT TARGET stonky_common)
    add_subdirectory(stonky-cpp-common)
endif ()

set(HEADERS
        include/stonky/hyperliquid/hyperliquid.h
        include/stonky/hyperliquid/hyperliquid_enums.h
        include/stonky/hyperliquid/hyperliquid_models.h
        include/stonky/hyperliquid/hyperliquid_http_session.h
        include/stonky/hyperliquid/hyperliquid_rest_client.h)

set(SOURCES
        src/hyperliquid.cpp
        src/hyperliquid_models.cpp
        src/hyperliquid_http_session.cpp
        src/hyperliquid_rest_client.cpp)

if (MODULE_MANAGER)
    add_library(hyperliquid_api SHARED ${SOURCES} ${HEADERS})
    # On Windows a SHARED lib exports nothing without explicit dllexport, so no
    # import .lib is produced and consumers fail with LNK1104. Auto-export all
    # symbols (no-op on non-MSVC, where shared libs already export everything).
    set_target_properties(hyperliquid_api PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else ()
    add_library(hyperliquid_api STATIC ${SOURCES} ${HEADERS})

    add_executable(hyperliquid_test test/main.cpp)
    target_link_libraries(hyperliquid_test PRIVATE spdlog::spdlog_header_only hyperliquid_api OpenSSL::Crypto)

endif ()

target_link_libraries(hyperliquid_api PRIVATE spdlog::spdlog_header_only OpenSSL::Crypto OpenSSL::SSL stonky_common nlohmann_json::nlohmann_json)

if (NOT MSVC)
    target_link_libraries(hyperliquid_api PRIVATE atomic)
endif ()
