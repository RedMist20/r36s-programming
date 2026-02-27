cmake_minimum_required(VERSION 3.13)

project(tools-menu-sdl2 VERSION 1.0.0 LANGUAGES C)

add_executable(tools-menu-sdl2
  tools-menu-sdl2.c
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

target_include_directories(tools-menu-sdl2 PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_directories(tools-menu-sdl2 PRIVATE
  ${SDL2_LIBRARY_DIRS}
  ${SDL2_TTF_LIBRARY_DIRS}
)

target_link_libraries(tools-menu-sdl2 PRIVATE
  ${SDL2_LIBRARIES}
  ${SDL2_TTF_LIBRARIES}
)

target_compile_options(tools-menu-sdl2 PRIVATE
  ${SDL2_CFLAGS_OTHER}
  ${SDL2_TTF_CFLAGS_OTHER}
)
