# libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (LGPL-2.1 or later, see COPYING.LIB and NOTICE).
# Original build file of this repository.  Usable stand-alone (host tests) and through add_subdirectory (Android, see
# tools/companion/android/app/src/main/cpp/CMakeLists.txt, option SOMRANDO_LIB).
#
#   SOM_RANDO_SRC          upstream SoMRandomizer/ project folder, for the resources generated at build time (Open World pokedex text and nesoberi art;
#                          not stored in the repository).  Nothing derived from the ROM is built in: see src/sr_rom_recipes.c
#   SOMRANDO_LIB_TYPE      SHARED (default; libsomrando.so is replaceable, LGPL section 6) or STATIC
#   SOMRANDO_QUOTES        ON (default): data/sr_quotes_*.c, the Ancient Cave dialogue quotes (third-party text), are linked; OFF: placeholders
#   SOMRANDO_ADVERTISE     ON (default): somrando_available() returns 1; the menu then enables GO only for the presets that somrando_supports()
#                          accepts, i.e. those verified byte-identical (3 of 17 in phase 2).  OFF: the app behaves as with the stub.
cmake_minimum_required(VERSION 3.22.1)
if(NOT CMAKE_PROJECT_NAME)
  project(somrando C)
endif()

set(SOM_RANDO_SRC "" CACHE PATH "upstream SoMRandomizer folder (build-time resources)")
set(SOMRANDO_LIB_TYPE "SHARED" CACHE STRING "SHARED or STATIC")
option(SOMRANDO_QUOTES "link the third-party Ancient Cave dialogue quotes (data/README.quotes.md); OFF uses neutral placeholders and loses byte identity of the dialogue" ON)
option(SOMRANDO_ADVERTISE "somrando_available() returns 1 (generation offered for the supported presets)" ON)

set(_sr_here ${CMAKE_CURRENT_LIST_DIR})
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_sr_gen_ev1 ${CMAKE_CURRENT_BINARY_DIR}/sr_resources_p4_ev1_gen.c)
add_custom_command(OUTPUT ${_sr_gen_ev1}
  COMMAND ${Python3_EXECUTABLE} ${_sr_here}/tools/gen_p4_ev1_resources.py ${SOM_RANDO_SRC} ${_sr_gen_ev1}
  DEPENDS ${_sr_here}/tools/gen_p4_ev1_resources.py ${SOM_RANDO_SRC}/Resources/pokedata3.xml
  COMMENT "libsomrando: generating the Open World pokedex resource")
set(_sr_gen_p4 ${CMAKE_CURRENT_BINARY_DIR}/sr_p4_resources_gen.c)
add_custom_command(OUTPUT ${_sr_gen_p4}
  COMMAND ${Python3_EXECUTABLE} ${_sr_here}/tools/gen_p4_hmisc_resources.py ${SOM_RANDO_SRC} ${_sr_gen_p4}
  DEPENDS ${_sr_here}/tools/gen_p4_hmisc_resources.py ${SOM_RANDO_SRC}/processing/hacks/openworld/Nesoberi.cs
  COMMENT "libsomrando: generating the nesoberi resources")

file(GLOB _sr_srcs CONFIGURE_DEPENDS ${_sr_here}/src/*.c)
if(SOMRANDO_QUOTES)
  set(_sr_quotes ${_sr_here}/data/sr_quotes_mitch.c ${_sr_here}/data/sr_quotes_demetri.c)
else()
  set(_sr_quotes ${_sr_here}/data/sr_quotes_placeholder.c)
endif()
add_library(somrando ${SOMRANDO_LIB_TYPE} ${_sr_srcs} ${_sr_quotes} ${_sr_gen_ev1} ${_sr_gen_p4})
target_include_directories(somrando PUBLIC ${_sr_here}/../../launcher PRIVATE ${_sr_here}/src)
# no fused multiply-add: double arithmetic must match the .NET results bit for bit
target_compile_options(somrando PRIVATE -Wall -Wextra -ffp-contract=off -O2)
target_compile_definitions(somrando PRIVATE _POSIX_C_SOURCE=200809L)
if(SOMRANDO_ADVERTISE)
  target_compile_definitions(somrando PRIVATE SOMRANDO_ADVERTISE=1)
endif()
set_target_properties(somrando PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
target_link_libraries(somrando PRIVATE m)
