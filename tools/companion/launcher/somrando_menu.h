/* Glue between the start menu model (som_launcher.h) and the generator seam (somrando_api.h): asks the generator whether the menu's current
 * configuration, or a given preset, would give the same ROM as the C# generator.  Original work of this repository (LGPL-2.1+, like libsomrando). */
#ifndef SOMRANDO_MENU_H
#define SOMRANDO_MENU_H

#include <stddef.h>

struct sl_launcher;

/* 1 when the generator in this build can produce the menu's current configuration byte for byte; 0 otherwise and `why` names what is
 * missing ("unsupported option: ..."). */
int somrando_menu_supports(const struct sl_launcher *L, char *why, size_t why_len);
/* The same for preset index `preset` of sl_presets (seed ignored). -1 for a bad index. */
int somrando_menu_preset_supported(int preset, char *why, size_t why_len);

#endif
