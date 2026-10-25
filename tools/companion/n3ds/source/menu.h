/* Start menu of the 3DS frontend: SoM Original / Create Randomized (shared model tools/companion/launcher). */
#ifndef SOM_N3DS_MENU_H
#define SOM_N3DS_MENU_H
#include <stddef.h>

#define MENU_ROOT "sdmc:/som"
#define MENU_GEN_DIR MENU_ROOT "/roms/gen"
#define MENU_TRASH_DIR MENU_ROOT "/roms/trash"

/* Runs the menu on both screens until the player picks a ROM.  Returns 1 with the ROM path in rom_out (original: sdmc:/som/rom.sfc,
 * randomized: sdmc:/som/roms/gen/<key>.sfc) and *original set to 1 for the original, or 0 when the player quit. */
struct n3_settings;
int menu_run(char *rom_out, size_t n, int *original, struct n3_settings *settings);
#endif
