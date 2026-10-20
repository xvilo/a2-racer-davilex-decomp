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
	src/platform/gl.c
	src/res/archive.c
	src/res/discimage.c
	src/res/pcx.c
	src/res/smacker.c
	src/movie/movies.c
	src/gfx/screen.c
	src/gfx/png.c
	src/gfx/wipes.c
	src/xvga/fileio.c
	src/xvga/inifile.c
	src/xvga/joystick.c
	src/xvga/font.c
	src/xvga/sound.c
	src/menu/menu_data.c
	src/menu/forms.c
	src/menu/help.c
	src/menu/infio.c
	src/menu/m_bedien.c
	src/menu/m_clas.c
	src/menu/m_main.c
	src/menu/m_nsplr.c
	src/menu/m_opties.c
	src/menu/m_score.c
	src/menu/m_stand.c
	src/menu/m_start.c
	src/menu/menu_util.c
	src/menu/requesters.c
	src/menu/score.c
	src/menu/mysound.c
	src/menu/optie.c
	src/menu/slot.c
	src/menu/spel.c
	src/menu/txttool.c
	src/race/camera.c
	src/race/driver.c
	src/race/bonus.c
	src/race/hud.c
	src/race/pion.c
	src/race/mapexec.c
	src/race/math3d.c
	src/race/object3d.c
	src/race/opponent.c
	src/race/player.c
	src/race/race.c
	src/race/race_files.c
	src/race/race_view.c
	src/race/render.c
	src/race/sndfx.c
	src/race/statobj.c
	src/race/texture.c
	src/race/weg.c
)
target_include_directories(a2racer PRIVATE src)
# SDL2main (WinMain → main) has to come before SDL2 for MinGW's linker.
if(TARGET SDL2::SDL2main)
	target_link_libraries(a2racer PRIVATE SDL2::SDL2main)
endif()
# Shared libraries where there are any, else the static ones (the macOS release build).
if(TARGET SDL2::SDL2)
	target_link_libraries(a2racer PRIVATE SDL2::SDL2)
else()
	target_link_libraries(a2racer PRIVATE SDL2::SDL2-static)
endif()
if(TARGET SDL2_ttf::SDL2_ttf)
	target_link_libraries(a2racer PRIVATE SDL2_ttf::SDL2_ttf)
else()
	target_link_libraries(a2racer PRIVATE SDL2_ttf::SDL2_ttf-static)
endif()
if(NOT WIN32)
	target_link_libraries(a2racer PRIVATE m)
endif()
if(MSVC)
	target_compile_options(a2racer PRIVATE /W3)
else()
	target_compile_options(a2racer PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
