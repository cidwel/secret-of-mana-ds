/* Live render of ONE actor's sprites (an enemy) from the game's own OAM, VRAM and CGRAM onto a transparent image.
 *
 * Why: the target selector must show the enemy alone (no scene, no camera).  The SNES draws an enemy as up to 15 OAM
 * sprites; the game keeps the list of the pieces of every actor in WRAM, so the OAM entries that belong to an actor are
 * found by (tile, attribute) and the expected position, not by a screen crop.  Palette index 0 is transparent.
 *
 * Inputs (all read-only, none is retained):
 *   oam    544 bytes: 512 bytes of 128 x {x, y, tile, attr} plus the 32-byte high table (x bit 8, size bit)
 *   vram   65536 bytes
 *   cgram  512 bytes, 256 little-endian BGR555 words (sprite palettes are colours 128..255)
 *   obsel  the PPU register $2101: name base (bits 0-2), name gap (bits 3-4), size select (bits 5-7)
 *   wram   the 128 KiB WRAM image (the companion's `w`), for the actor record at 0xE000 + 0x200 * slot
 *
 * Output: straight-alpha RGBA bytes, tightly cropped with a 1 px transparent border, plus the actor's foot point
 * (its screen position) inside the image.  The caller supplies the pixel buffer (SOM_ES_BUF_BYTES).
 * Pure C99, no allocation, thread safe. */
#ifndef SOM_ENEMY_SPRITE_H
#define SOM_ENEMY_SPRITE_H
#include <stddef.h>
#include <stdint.h>

#define SOM_ES_OAM_BYTES 544
#define SOM_ES_VRAM_BYTES 65536
#define SOM_ES_CGRAM_BYTES 512
#define SOM_ES_MAX_SIDE 256                                     /* largest picture side that is accepted */
#define SOM_ES_BUF_BYTES ((SOM_ES_MAX_SIDE + 2) * (SOM_ES_MAX_SIDE + 2) * 4)

struct som_es_src {
  const uint8_t *oam;      /* 544 bytes */
  const uint8_t *vram;     /* 65536 bytes */
  const uint8_t *cgram;    /* 512 bytes */
  uint8_t obsel;           /* $2101 */
};

struct som_es_img {
  uint8_t *rgba;           /* caller buffer of SOM_ES_BUF_BYTES; on success w * h * 4 bytes are valid */
  int w, h;                /* picture size including the 1 px border */
  int foot_x, foot_y;      /* the actor's screen position inside the picture (may lie outside for tall sprites) */
  int n_sprites;           /* OAM entries drawn */
  int n_pieces;            /* pieces the actor lists in WRAM */
  int n_matched;           /* of those, found in the OAM */
};

/* Fills `src` from the WRAM image plus the VRAM the core exposes (RETRO_MEMORY_VIDEO_RAM works on the stock snes9x core and on the
 * wide core): OAM shadow $7E:0800 (544 bytes), CGRAM shadow $7E:0600 ($7E:BE00 while $7E:002A is non-zero), OBSEL shadow $7E:0091.
 * No core patch is needed.  The shadows are what the NMI uploads next, so they can run one frame ahead of VRAM; the hardware copies
 * of the wide core (retro_som_oam_get, retro_som_cgram_get, retro_som_regs_get()[0x2101]) are exact and may replace them. */
static inline void som_es_src_wram(struct som_es_src *src, const uint8_t *wram, const uint8_t *vram) {
  src->oam = wram + 0x0800;
  src->cgram = wram + (wram[0x2A] ? 0xBE00 : 0x0600);
  src->vram = vram;
  src->obsel = wram[0x91];
}

/* Renders actor `slot` (3..15).  Returns 1 when at least one piece was drawn, 0 when the actor is not drawn (off screen,
 * blinking frame, no pieces) or the picture is larger than SOM_ES_MAX_SIDE. */
int som_es_render_actor(const struct som_es_src *src, const uint8_t *wram, int slot, struct som_es_img *out);

/* Fallback without WRAM pieces: every OAM entry whose box intersects the screen rectangle [x0, x1) x [y0, y1) and is not
 * a ground shadow (tile 0x00, attribute 0x22/0x32 family).  Neighbouring actors inside the rectangle are drawn too. */
int som_es_render_box(const struct som_es_src *src, int x0, int y0, int x1, int y1, int foot_x, int foot_y, struct som_es_img *out);

/* Scales a picture into a side x side ARGB (0xAARRGGBB) square, centred, aspect preserved: integer nearest-neighbour
 * enlargement or premultiplied box reduction.  `argb` has side * side entries. */
void som_es_fit_argb(const struct som_es_img *img, int side, uint32_t *argb);


/* ---- atlas pack (format SESP v1, written by build_atlas.py): one clean idle picture per monster id ----
 * Little endian.  Header 16 bytes: "SESP", u16 version (1), u16 count, u32 total size, u32 reserved.  Then `count` directory entries of 24 bytes:
 * u8 id, u8 source (1 Godot ROM rip, 2 game rip, 3 live render), u8 confidence (3 verified, 2 probable, 1 partial), u8 reserved, u16 w, u16 h,
 * i16 foot_x, i16 foot_y, u32 offset of the RGBA bytes from the start of the file, u32 byte count (w * h * 4), u32 reserved.  Pixels are straight-alpha
 * RGBA, rows top to bottom, 1 px transparent border, foot = the actor's position inside the picture. */
struct som_es_pack_entry {
  int id, source, confidence, w, h, foot_x, foot_y;
  const uint8_t *rgba;     /* points into the blob */
};
/* Finds monster `id`; returns 1 and fills `e`, or 0 (id absent, blob malformed).  Validates every offset against `len`. */
int som_es_pack_find(const uint8_t *blob, size_t len, int id, struct som_es_pack_entry *e);


/* ---- one call for the frontend: the sprite of an enemy id, ready for som_ux_set_enemy_sprite() ----
 * Order: a pack picture of confidence >= 2; else a live render of an on-screen actor with that id whose pieces were all found; else any pack
 * picture; else nothing.  `scratch` is SOM_ES_BUF_BYTES of working memory (the live render).  `argb` receives up to SOM_ES_SPRITE_MAX x
 * SOM_ES_SPRITE_MAX straight-alpha 0xAARRGGBB pixels, rows w apart; a larger picture is reduced into a SOM_ES_SPRITE_MAX square.
 * `live` and `wram` may be NULL (atlas only); `pack` may be NULL (live only).  Returns SOM_ES_FROM_*; *w, *h, *conf are set on success. */
#define SOM_ES_SPRITE_MAX 96
enum { SOM_ES_FROM_NONE = 0, SOM_ES_FROM_PACK = 1, SOM_ES_FROM_LIVE = 2 };
int som_es_sprite_for_id(const uint8_t *pack, size_t pack_len, const struct som_es_src *live, const uint8_t *wram, int enemy_id,
                         uint8_t *scratch, uint32_t *argb, int *w, int *h, int *conf);

#endif
