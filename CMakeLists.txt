cmake_minimum_required(VERSION 3.20)
project(sas-tui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SAS_TUI_ENABLE_CLANG_TIDY "Run clang-tidy during normal builds" OFF)
option(SAS_TUI_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(SAS_TUI_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(SAS_TUI_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)

include(CTest)

include(FetchContent)
FetchContent_Declare(ftxui
  URL https://github.com/ArthurSonzogni/ftxui/archive/refs/tags/v5.0.0.tar.gz
  URL_HASH SHA256=a2991cb222c944aee14397965d9f6b050245da849d8c5da7c72d112de2786b5b
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
  URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(ftxui json)

find_package(CURL REQUIRED)

add_executable(sas-tui
  src/main.cpp
  src/app.cpp
  src/config.cpp
  src/player.cpp
  src/synology_client.cpp
)
target_include_directories(sas-tui PRIVATE include)
target_link_libraries(sas-tui PRIVATE
  CURL::libcurl
  nlohmann_json::nlohmann_json
  ftxui::component
)

if(BUILD_TESTING)
  add_executable(sas-config-tests
    tests/config_test.cpp
    src/config.cpp
  )
  target_include_directories(sas-config-tests PRIVATE include)
  target_link_libraries(sas-config-tests PRIVATE nlohmann_json::nlohmann_json)
  add_test(NAME sas-config-tests COMMAND sas-config-tests)
endif()

if(SAS_TUI_ENABLE_COVERAGE)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Coverage is supported here only with Clang or GCC")
  endif()
  if(TARGET sas-config-tests)
    target_compile_options(sas-config-tests PRIVATE --coverage)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(sas-config-tests PRIVATE -fprofile-abs-path)
    endif()
    target_link_options(sas-config-tests PRIVATE --coverage)
  endif()
  find_program(SAS_TUI_GCOVR gcovr)
  if(SAS_TUI_GCOVR)
    add_custom_target(coverage
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
      COMMAND ${SAS_TUI_GCOVR}
        --root ${CMAKE_SOURCE_DIR}
        --filter ${CMAKE_SOURCE_DIR}/src/config.cpp
        --exclude-unreachable-branches
        --print-summary
        --xml-pretty
        --output ${CMAKE_BINARY_DIR}/coverage.xml
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS sas-config-tests
      COMMENT "Running tests and generating coverage report"
      VERBATIM
    )
  else()
    message(FATAL_ERROR
      "SAS_TUI_ENABLE_COVERAGE is ON, but gcovr was not found")
  endif()
endif()

set(SAS_TUI_SANITIZERS)
if(SAS_TUI_ENABLE_ASAN)
  list(APPEND SAS_TUI_SANITIZERS address)
endif()
if(SAS_TUI_ENABLE_UBSAN)
  list(APPEND SAS_TUI_SANITIZERS undefined)
endif()
if(SAS_TUI_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "ASan/UBSan are supported here only with Clang or GCC")
  endif()
  string(JOIN "," SAS_TUI_SANITIZER_FLAGS ${SAS_TUI_SANITIZERS})
  target_compile_options(sas-tui PRIVATE -fsanitize=${SAS_TUI_SANITIZER_FLAGS} -fno-omit-frame-pointer)
  target_link_options(sas-tui PRIVATE -fsanitize=${SAS_TUI_SANITIZER_FLAGS})
endif()

set(SAS_TUI_FORMAT_FILES
  ${CMAKE_SOURCE_DIR}/src/main.cpp
  ${CMAKE_SOURCE_DIR}/src/app.cpp
  ${CMAKE_SOURCE_DIR}/src/config.cpp
  ${CMAKE_SOURCE_DIR}/src/player.cpp
  ${CMAKE_SOURCE_DIR}/src/synology_client.cpp
  ${CMAKE_SOURCE_DIR}/include/sas/app.hpp
  ${CMAKE_SOURCE_DIR}/include/sas/config.hpp
  ${CMAKE_SOURCE_DIR}/include/sas/model.hpp
  ${CMAKE_SOURCE_DIR}/include/sas/player.hpp
  ${CMAKE_SOURCE_DIR}/include/sas/synology_client.hpp
  ${CMAKE_SOURCE_DIR}/tests/config_test.cpp
)

find_program(SAS_TUI_CLANG_FORMAT clang-format)
if(SAS_TUI_CLANG_FORMAT)
  add_custom_target(format
    COMMAND ${SAS_TUI_CLANG_FORMAT} -i ${SAS_TUI_FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting sas-tui sources with clang-format"
    VERBATIM
  )
  add_custom_target(format-check
    COMMAND ${SAS_TUI_CLANG_FORMAT} --dry-run --Werror ${SAS_TUI_FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking sas-tui formatting"
    VERBATIM
  )
else()
  message(STATUS "clang-format not found; format targets are unavailable")
endif()

find_program(SAS_TUI_CLANG_TIDY clang-tidy)
set(SAS_TUI_CLANG_TIDY_ARGS)
if(APPLE AND SAS_TUI_CLANG_TIDY)
  # Homebrew LLVM does not automatically discover Apple's libc++ headers.
  # Pass the active SDK explicitly so clang-tidy can parse the same sources
  # that the Apple compiler builds successfully.
  execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE SAS_TUI_MACOS_SDK
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(SAS_TUI_MACOS_SDK)
    list(APPEND SAS_TUI_CLANG_TIDY_ARGS
      "--extra-arg=-isysroot"
      "--extra-arg=${SAS_TUI_MACOS_SDK}"
    )
  endif()
endif()
if(SAS_TUI_ENABLE_CLANG_TIDY)
  if(SAS_TUI_CLANG_TIDY)
    set(SAS_TUI_CLANG_TIDY_COMMAND
      "${SAS_TUI_CLANG_TIDY};${SAS_TUI_CLANG_TIDY_ARGS}")
    set_target_properties(sas-tui PROPERTIES
      CXX_CLANG_TIDY "${SAS_TUI_CLANG_TIDY_COMMAND}"
    )
  else()
    message(FATAL_ERROR "SAS_TUI_ENABLE_CLANG_TIDY is ON, but clang-tidy was not found")
  endif()
endif()

if(SAS_TUI_CLANG_TIDY)
  add_custom_target(tidy
    COMMAND ${SAS_TUI_CLANG_TIDY} -p=${CMAKE_BINARY_DIR}
      ${SAS_TUI_CLANG_TIDY_ARGS}
      ${CMAKE_SOURCE_DIR}/src/main.cpp
      ${CMAKE_SOURCE_DIR}/src/app.cpp
      ${CMAKE_SOURCE_DIR}/src/config.cpp
      ${CMAKE_SOURCE_DIR}/src/player.cpp
      ${CMAKE_SOURCE_DIR}/src/synology_client.cpp
    DEPENDS sas-tui
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy on sas-tui sources"
    VERBATIM
  )
else()
  message(STATUS "clang-tidy not found; tidy target is unavailable")
endif()

if(APPLE)
  target_compile_definitions(sas-tui PRIVATE _DARWIN_C_SOURCE)
endif()
