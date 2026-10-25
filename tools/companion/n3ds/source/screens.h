/* Pad-driven screens of the 3DS frontend built on sui.c: settings, credits and the in-game pause menu. */
#ifndef SOM_N3DS_SCREENS_H
#define SOM_N3DS_SCREENS_H
#include <3ds.h>
#include <stdint.h>
#include "settings.h"
#include "sui.h"
#include "savefile.h"

#define SETTINGS_PATH "sdmc:/som/settings.cfg"

/* Presents one top and one bottom buffer (both fully written) and waits for the next frame. */
void screens_present(const uint32_t *top400, const uint32_t *bottom320);

/* Credits (data/credits.json, embedded by gen_data.py): scrolls with the D-pad, B leaves. */
void screens_credits(void);

/* Settings from the start menu: video filter, gameplay options (applied to the ROM when a game starts), credits.  Saves SETTINGS_PATH on exit. */
void screens_settings(struct n3_settings *s);

/* Pause menu of a running game.  Returns an action for the caller. */
enum pause_action { PAUSE_RESUME = 0, PAUSE_SAVE, PAUSE_LOAD, PAUSE_EXIT };
struct pause_ctx {
  struct n3_settings *settings;
  const char *state_dir, *key;    /* where the slot files live, "original" or the generated key */
  uint32_t rom_crc;               /* CRC-32 of the ROM image the core runs */
  char notice[96];                /* set by the caller after a failed save or load, shown on entry */
};
int screens_pause(struct pause_ctx *p);    /* slot in p->settings->slot */

#endif
