cmake_minimum_required(VERSION 3.16)
project(a2racer C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(SDL2 REQUIRED CONFIG)
find_package(SDL2_ttf REQUIRED CONFIG)

add_executable(a2racer
	src/main.c
	src/platform/sys_sdl.c
	src/res/archive.c
	src/res/pcx.c
	src/gfx/screen.c
	src/gfx/png.c
	src/gfx/wipes.c
	src/xvga/fileio.c
	src/xvga/font.c
	src/xvga/sound.c
	src/menu/menu_data.c
	src/menu/forms.c
	src/menu/help.c
	src/menu/m_bedien.c
	src/menu/m_main.c
	src/menu/m_opties.c
	src/menu/m_score.c
	src/menu/m_start.c
	src/menu/menu_util.c
	src/menu/requesters.c
	src/menu/score.c
	src/menu/mysound.c
	src/menu/optie.c
	src/menu/slot.c
	src/menu/txttool.c
)
target_include_directories(a2racer PRIVATE src)
target_link_libraries(a2racer PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
if(NOT WIN32)
	target_link_libraries(a2racer PRIVATE m)
endif()
if(TARGET SDL2::SDL2main)
	target_link_libraries(a2racer PRIVATE SDL2::SDL2main)
endif()
if(MSVC)
	target_compile_options(a2racer PRIVATE /W3)
else()
	target_compile_options(a2racer PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
