cmake_minimum_required(VERSION 3.22.1)
project(somcompanion C)

set(COMPANION ${CMAKE_CURRENT_LIST_DIR}/../../../../..)
set(CMAKE_C_STANDARD 99)

add_library(somjni SHARED
    som_jni.c
    ${COMPANION}/som_companion.c
    ${COMPANION}/som_status_draw.c
    ${COMPANION}/direct/som_direct.c
    ${COMPANION}/som_bind.c
    ${COMPANION}/som_states.c
    ${COMPANION}/som_gameplay_apply.c
    ${COMPANION}/pc/pc_gameplay.c
    som_start_pages.c
    ${COMPANION}/som_hotkey.c
    ${COMPANION}/ux/som_ui_draw.c
    ${COMPANION}/ux/som_ui_state.c
    ${COMPANION}/ux/som_ui_sel.c
    ${COMPANION}/ux/som_ui_zone.c
    ${COMPANION}/enemysprites/som_enemy_sprite.c
    som_launcher_jni.c
    ${COMPANION}/launcher/som_launcher.c
    ${COMPANION}/launcher/som_launcher_catalog.c
    ${COMPANION}/launcher/som_launcher_draw.c
    ${COMPANION}/launcher/somrando_menu.c)
target_include_directories(somjni PRIVATE ${COMPANION} ${CMAKE_CURRENT_LIST_DIR})

# The generator seam (launcher/somrando_api.h).  Default: the stub, which reports "no generator".  -DSOMRANDO_LIB=ON links
# libsomrando.so (C port of Moppu/SecretOfManaRandomizer, LGPL-2.1+) instead; it needs -DSOM_RANDO_SRC=<upstream SoMRandomizer folder>
# for the resources it generates at build time.  The port is partial: somrando_supports() tells which requests are byte-identical to the C#
# generator (docs/randomizer-c-port-progress-20261018.md); the menu enables GO for those only (launcher/somrando_menu.c).  Add
# -DSOMRANDO_ADVERTISE=OFF to hide the generator again (default ON: somrando_available() == 1, GO enabled for the supported presets only).
option(SOMRANDO_LIB "link libsomrando (partial C port) instead of the stub" OFF)
if(SOMRANDO_LIB)
    set(SOMRANDO_LIB_TYPE SHARED CACHE STRING "libsomrando is a separate library (LGPL section 6)")
    add_subdirectory(${COMPANION}/randomizer/libsomrando somrando_build)
    target_link_libraries(somjni PRIVATE somrando)
else()
    target_sources(somjni PRIVATE ${COMPANION}/launcher/somrando_stub.c)
endif()
target_compile_options(somjni PRIVATE -Wall -Wextra -O2)
target_link_libraries(somjni PRIVATE android log jnigraphics aaudio dl m)
