/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from SoMRandomizer/processing/common/structure/VanillaStructure.cs, MapObject.cs and Door.cs, and from
 * SoMRandomizer/processing/common/VanillaMapUtil.cs (getObjectOffset); Author: Moppleton.  Translated to C 2026-10-18.
 *
 * VanillaStructure.set() ORs the shifted value without masking it to the field, exactly as the C# does; keep it that way.
 */
#include "sr_core.h"

static int shift_of(int bits) {
  for (int i = 0; i < 32; i++)
    if (bits & (1 << i)) return i;
  return 32;
}

static int vs_get(const uint8_t *d, int byte_index, int bits) { return (uint8_t)((d[byte_index] & bits) >> shift_of(bits)); }

static void vs_set(uint8_t *d, int byte_index, int bits, int value) {
  uint8_t other = (uint8_t)(d[byte_index] & (~bits));
  d[byte_index] = (uint8_t)(value << shift_of(bits));
  d[byte_index] |= other;
}

int sr_vanilla_object_offset(const uint8_t *rom, int map_num) {
  /* 87000 - 16-bit pointers to object lists for each composite map; the first "object" is an 8-byte header */
  return 0x80000 + rom[0x87000 + map_num * 2] + (rom[0x87000 + map_num * 2 + 1] << 8);
}

int sr_door_get_ypos(const struct sr_door *d) { return vs_get(d->d, 2, 0xFE); }
void sr_door_set_ypos(struct sr_door *d, int y) { vs_set(d->d, 2, 0xFE, y); }
int sr_obj_get_ypos(const struct sr_map_object *o) { return vs_get(o->d, 3, 0x7F); }
void sr_obj_set_ypos(struct sr_map_object *o, int y) { vs_set(o->d, 3, 0x7F, y); }
int sr_obj_get_vis_flag(const struct sr_map_object *o) { return vs_get(o->d, 0, 0xFF); }
void sr_obj_set_vis_flag(struct sr_map_object *o, int v) { vs_set(o->d, 0, 0xFF, v); }
void sr_obj_set_spawn_priority(struct sr_map_object *o, int v) { vs_set(o->d, 2, 0x80, v ? 1 : 0); }
