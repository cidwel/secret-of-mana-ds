# SoM companion for the Nintendo 3DS (devkitARM + libctru).  No ROM is built in; the user's ROM is read at run time.
#
#   export DEVKITPRO=<dir containing devkitARM, libctru, tools>   (see docs/companion-platforms-20261017.md)
#   make                                  demo build: status screen with a synthetic state, no emulator core
#   make CORE_A=/path/snes9x2005_libretro_ctr.a     links the libretro core statically and runs the user's ROM
#   make CORE_A=... RANDO_A=/path/libsomrando.a     also links the on-device generator (emu/build_libsomrando.sh); without it "Create Randomized" needs a ROM made on the PC
#   build/gen_data.h is generated from data/credits.json and tools/gameplay/gameplay_menu.json (python3 on the build machine); see docs/companion-3ds-20261020.md
#
# Output: som_companion.3dsx (+ .smdh).  Run it from the Homebrew Launcher or with Citra/Azahar.

ifeq ($(strip $(DEVKITPRO)),)
$(error "Set DEVKITPRO to the directory that contains devkitARM and libctru")
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
TOOLS := $(DEVKITPRO)/tools/bin

COMP := $(abspath ../)
TARGET := som_companion
BUILD := build
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -std=gnu99 -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ \
          -I$(COMP) -Isource -I$(BUILD) -I$(DEVKITPRO)/libctru/include
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -L$(DEVKITPRO)/libctru/lib
LIBS := -lctru -lm

# RANDO_A=<libsomrando.a>: the on-device Ancient Cave / randomizer generator (emu/build_libsomrando.sh builds it).  Without it a stub reports "no generator".
ifneq ($(strip $(RANDO_A)),)
LIBS := -Wl,--whole-archive $(RANDO_A) -Wl,--no-whole-archive $(LIBS)   # --whole-archive is not needed any more (no weak stubs); it is harmless
else
SRC_RANDO := $(COMP)/launcher/somrando_stub.c
endif

ifneq ($(strip $(CORE_A)),)
CFLAGS += -DSOM_WITH_CORE
LIBS := $(CORE_A) $(LIBS)
endif

SRC := source/main.c $(COMP)/som_companion.c $(COMP)/som_status_draw.c $(COMP)/som_bind.c $(COMP)/som_hotkey.c \
       $(COMP)/direct/som_direct.c $(COMP)/ux/som_ui_draw.c $(COMP)/ux/som_ui_state.c $(COMP)/ux/som_ui_sel.c $(COMP)/ux/som_ui_zone.c \
       source/menu.c source/screens.c source/sui.c source/savefile.c source/settings.c $(COMP)/gameplay_c/som_gameplay.c $(COMP)/launcher/somrando_menu.c $(SRC_RANDO) $(COMP)/launcher/som_launcher.c $(COMP)/launcher/som_launcher_catalog.c $(COMP)/launcher/som_launcher_draw.c
OBJ := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(SRC)))
vpath %.c source $(COMP) $(COMP)/direct $(COMP)/ux $(COMP)/launcher $(COMP)/gameplay_c

all: $(TARGET).3dsx

$(BUILD):
	@mkdir -p $@

# The credits (data/credits.json) and the gameplay option list (tools/gameplay/gameplay_menu.json) are embedded at build time.
$(BUILD)/gen_data.h: gen_data.py ../../../data/credits.json ../../gameplay/gameplay_menu.json | $(BUILD)
	python3 gen_data.py $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/screens.o $(BUILD)/settings.o: $(BUILD)/gen_data.h
$(BUILD)/main.o $(BUILD)/menu.o: source/settings.h source/screens.h source/sui.h source/savefile.h $(COMP)/gameplay_c/som_gameplay.h

$(TARGET).elf: $(OBJ) Makefile
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

$(TARGET).smdh:
	$(TOOLS)/smdhtool --create "SoM Companion" "Dual-screen companion for the user's own ROM" "som-decomp" $(DEVKITPRO)/libctru/default_icon.png $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	$(TOOLS)/3dsxtool $< $@ --smdh=$(TARGET).smdh

.PHONY: all

# som_launcher.c formats a clamped 8 digit seed into a 9 byte buffer; GCC cannot see the clamp for a 32-bit long.
$(BUILD)/som_launcher.o: CFLAGS += -Wno-format-truncation
