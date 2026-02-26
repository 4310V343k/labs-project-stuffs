cmake_minimum_required(VERSION 3.14)
project(base_converter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v6.1.9
)
FetchContent_MakeAvailable(ftxui)

add_executable(base_converter
    src/main.cpp
    src/ui.cpp
    src/converter.cpp
)
target_include_directories(base_converter PRIVATE src)
target_link_libraries(base_converter
    PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)
target_compile_options(base_converter PRIVATE -Wall -Wextra)
