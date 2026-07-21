cmake_minimum_required(VERSION 3.20)

project(SteamSalesTray
    VERSION 0.1.0
    DESCRIPTION "Steam Sales Tray - native Win32 sales monitor"
    LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "SteamSalesTray targets Windows only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static MSVC runtime (/MT, /MTd) so the exe has no VC++ redist dependency.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Unicode everywhere; trim windows.h.
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    _CRT_SECURE_NO_WARNINGS)

# ---------------------------------------------------------------------------
# Third-party (vendored, compiled from source). Built without our strict flags.
# ---------------------------------------------------------------------------
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_DQS=0
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
    SQLITE_ENABLE_STAT4)

add_library(yyjson STATIC third_party/yyjson/yyjson.c)
target_include_directories(yyjson PUBLIC third_party/yyjson)

# ---------------------------------------------------------------------------
# Core library: all application logic (everything except the WinMain entry).
# Both the executable and the test targets link against this.
# ---------------------------------------------------------------------------
add_library(sst_core STATIC
    src/app/App.cpp
    src/app/SingleInstance.cpp
    src/tray/TrayIcon.cpp
    src/tray/TrayMenu.cpp
    src/ui/SettingsDialog.cpp
    src/ui/SalesWindow.cpp
    src/network/WinHttpClient.cpp
    src/network/UrlEncoder.cpp
    src/steam/SteamFinancialClient.cpp
    src/steam/SteamJsonParser.cpp
    src/sync/SalesAggregator.cpp
    src/sync/SalesSyncService.cpp
    src/security/SecureString.cpp
    src/security/DpapiSecretStore.cpp
    src/storage/SettingsStore.cpp
    src/storage/Database.cpp
    src/storage/Migrations.cpp
    src/platform/Paths.cpp
    src/platform/StartupManager.cpp
    src/platform/StringUtil.cpp
    src/platform/TimeUtil.cpp
    src/platform/Logging.cpp
    src/platform/Win32Error.cpp)

target_include_directories(sst_core PUBLIC src src/resources)

target_link_libraries(sst_core PUBLIC
    sqlite3 yyjson
    winhttp crypt32 shell32 comctl32 shlwapi advapi32 user32 gdi32 ole32)

# ---------------------------------------------------------------------------
# Main executable (WIN32 => /SUBSYSTEM:WINDOWS, no console).
# ---------------------------------------------------------------------------
add_executable(SteamSalesTray WIN32
    src/app/Main.cpp
    src/resources/SteamSalesTray.rc)
target_link_libraries(SteamSalesTray PRIVATE sst_core)
set_target_properties(SteamSalesTray PROPERTIES OUTPUT_NAME "SteamSalesTray")

# ---------------------------------------------------------------------------
# Strict flags for our own code (not third-party).
# ---------------------------------------------------------------------------
if(MSVC)
    foreach(tgt sst_core SteamSalesTray)
        target_compile_options(${tgt} PRIVATE
            /W4 /permissive- /utf-8 /EHsc /MP
            $<$<CONFIG:Release>:/O2 /Gy /Gw /guard:cf /sdl /GS>)
    endforeach()
    target_link_options(SteamSalesTray PRIVATE
        /guard:cf
        /MANIFEST:NO  # we embed our own manifest via the .rc (RT_MANIFEST id 1)
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>)
    set_property(TARGET sst_core PROPERTY
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET SteamSalesTray PROPERTY
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# ---------------------------------------------------------------------------
# Tests (offline, hermetic). Run with: ctest -C Release
# ---------------------------------------------------------------------------
enable_testing()

set(SST_TESTS
    SteamJsonParserTests
    DatabaseTests
    DpapiSecretStoreTests
    LoggingTests)

foreach(test ${SST_TESTS})
    add_executable(${test} tests/${test}.cpp)
    target_link_libraries(${test} PRIVATE sst_core)
    if(MSVC)
        target_compile_options(${test} PRIVATE /W4 /utf-8 /EHsc)
    endif()
    add_test(NAME ${test} COMMAND ${test})
endforeach()
