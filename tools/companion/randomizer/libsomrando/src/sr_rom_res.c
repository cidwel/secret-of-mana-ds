/* libsomrando: rebuilds the upstream embedded resources that are partly vanilla ROM data from the user's ROM (see sr_rom_recipes.c and
 * tools/gen_rom_recipes.py).  Original code of this repository, LGPL-2.1 or later. */
#include <string.h>

#include "sr_core.h"

static uint32_t crc32_of(const uint8_t *p, size_t n) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

const uint8_t *sr_rom_res(struct sr_ctx *c, const char *name, size_t *len) {
  for (int i = 0; i < c->n_rom_res; i++)
    if (!strcmp(c->rom_res[i].name, name)) { if (len) *len = c->rom_res[i].len; return c->rom_res[i].data; }
  for (int r = 0; r < sr_rom_recipe_count; r++) {
    const struct sr_rom_recipe *rc = &sr_rom_recipes[r];
    if (strcmp(rc->name, name)) continue;
    if (c->n_rom_res >= (int)(sizeof c->rom_res / sizeof *c->rom_res)) sr_fail(c, SOMRANDO_ERR_FAILED, "too many ROM-derived resources");
    uint8_t *d = (uint8_t *)sr_alloc(c, rc->len ? rc->len : 1);
    size_t at = 0, lit = 0;
    for (int k = 0; k < rc->nseg; k++) {
      const struct sr_rom_seg *sg = &rc->segs[k];
      if (at + sg->len > rc->len) sr_fail(c, SOMRANDO_ERR_FAILED, "bad recipe %s", name);
      if (sg->rom == 0xFFFFFFFFu) { memcpy(d + at, rc->lit + lit, sg->len); lit += sg->len; }
      else {
        if ((size_t)sg->rom + sg->len > SR_ROM_IN_SIZE) sr_fail(c, SOMRANDO_ERR_FAILED, "bad recipe %s", name);
        memcpy(d + at, c->orig + sg->rom, sg->len);
      }
      at += sg->len;
    }
    if (at != rc->len || crc32_of(d, at) != rc->crc)
      sr_fail(c, SOMRANDO_ERR_BAD_ROM, "The source ROM is not the unmodified US v1.0 image (resource %s does not match).", name);
    c->rom_res[c->n_rom_res].name = rc->name;
    c->rom_res[c->n_rom_res].data = d;
    c->rom_res[c->n_rom_res].len = rc->len;
    c->n_rom_res++;
    if (len) *len = rc->len;
    return d;
  }
  return NULL;
}
