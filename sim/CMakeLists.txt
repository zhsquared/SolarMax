cmake_minimum_required(VERSION 3.16)
project(solarmax_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v5.0.0)
FetchContent_MakeAvailable(ftxui)

add_executable(solarmax_sim
  main_tui.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/solar_math/solar_position.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/tracker_core/tracker_core.cpp)

# NATIVE_BUILD keeps solar_position.h from pulling in Arduino/RTClib.
target_compile_definitions(solarmax_sim PRIVATE NATIVE_BUILD)
target_include_directories(solarmax_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/solar_math
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/tracker_core)
target_link_libraries(solarmax_sim PRIVATE
  ftxui::screen ftxui::dom ftxui::component)
