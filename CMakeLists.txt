cmake_minimum_required(VERSION 3.20)
project(RoseRecompiled C)
set(ROSE_ROM "" CACHE FILEPATH "Local compatible NES ROM")
set(ROSE_SEEDS "" CACHE FILEPATH "Per-ROM execution seeds")
set(ROSE_CAPTURES "" CACHE FILEPATH "Per-ROM RAM code captures")
if(NOT EXISTS "${ROSE_ROM}")
  message(FATAL_ERROR "Set -DROSE_ROM to your imported .nes file")
endif()
include("${CMAKE_SOURCE_DIR}/external/nesrecomp/runner/cyc/project.cmake")
nesrecomp_add_cycle_game(RoseRecompiledRuntime
  ROM "${ROSE_ROM}"
  SEED_FILE "${ROSE_SEEDS}" CAPTURE_FILE "${ROSE_CAPTURES}"
  GAME_CONFIG "${CMAKE_SOURCE_DIR}/game.toml"
  GAME_ID "rose-recompiled" PLAYERS 1
  HOST_EXTRAS src/rose_video.c)
set_target_properties(RoseRecompiledRuntime PROPERTIES OUTPUT_NAME RoseRecompiled)
option(ROSE_BUILD_ORACLE "Build the independent TriCNES reference for verification" OFF)
if(ROSE_BUILD_ORACLE)
  enable_language(CXX)
  include("${CMAKE_SOURCE_DIR}/external/nesrecomp/runner/cyc/cyc.cmake")
  nesrecomp_cyc_add_oracle(rose_oracle)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(rose_oracle PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing>)
  endif()
endif()
get_target_property(has_sdl RoseRecompiledRuntime NESRECOMP_CYC_SDL)
if(has_sdl)
  target_sources(RoseRecompiledRuntime PRIVATE src/mister_menu.c)
  target_include_directories(RoseRecompiledRuntime PRIVATE src)
  # Only the SDL host consumes our private UI adapter. Other framework sources
  # must not infer that the optional upstream recomp-ui library is linked.
  set_property(SOURCE "${CMAKE_SOURCE_DIR}/external/nesrecomp/runner/cyc/cyc_sdl.c"
    APPEND PROPERTY COMPILE_DEFINITIONS CYC_WITH_RECOMP_UI)
  include(CTest)
  if(BUILD_TESTING)
    set(cyc "${CMAKE_SOURCE_DIR}/external/nesrecomp/runner/cyc")
    add_executable(menu_test tests/menu_test.c src/mister_menu.c
      "${cyc}/cyc_overlay.c" "${cyc}/cyc_settings.c" "${cyc}/cyc_input.c" "${cyc}/cyc_png.c")
    target_include_directories(menu_test PRIVATE src "${cyc}" "${cyc}/../include")
    target_link_libraries(menu_test PRIVATE SDL2::SDL2)
    if(MSVC)
      target_compile_options(menu_test PRIVATE /UNDEBUG)
    else()
      target_compile_options(menu_test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME menu COMMAND menu_test "${CMAKE_BINARY_DIR}/menu-preview.png")
    add_test(NAME importer COMMAND "${Python3_EXECUTABLE}" -m unittest discover
      -s "${CMAKE_SOURCE_DIR}/tests" -p "test_*.py")
  endif()
endif()
