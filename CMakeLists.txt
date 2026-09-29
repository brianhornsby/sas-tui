cmake_minimum_required(VERSION 3.20)
project(sas-tui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(ftxui
  URL https://github.com/ArthurSonzogni/ftxui/archive/refs/tags/v5.0.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(ftxui json)

find_package(CURL REQUIRED)

add_executable(sas-tui
  src/main.cpp
  src/app.cpp
  src/player.cpp
  src/synology_client.cpp
)
target_include_directories(sas-tui PRIVATE include)
target_link_libraries(sas-tui PRIVATE
  CURL::libcurl
  nlohmann_json::nlohmann_json
  ftxui::screen
  ftxui::dom
  ftxui::component
)

if(APPLE)
  target_compile_definitions(sas-tui PRIVATE _DARWIN_C_SOURCE)
endif()
