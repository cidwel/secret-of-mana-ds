# Enemy sprites: API proposal for `ux_glue` and what the UX and Android sides change

Date: 2026-10-17. Producer: `tools/companion/enemysprites/som_enemy_sprite.[ch]` (method and coverage: `docs/companion-enemysprites-20261017.md`).
Consumer: the sprite API the UX layer already has (`som_ux_set_enemy_sprite`, `som_ux_enemy_sprites_wanted`, `som_ux_clear_enemy_sprites`; glue wrappers
`ux_glue_set_enemy_sprite`, `ux_glue_sprites_wanted`, `ux_glue_clear_enemy_sprites` in `tools/companion/ux/android_patch/som_ux_glue.h`).

The consumer side needs no new function: the frontend hands the UX one straight-alpha ARGB sprite per enemy id. The only new piece is the producer, and the frontend
calls one function of it per wanted id.

## Contract

```c
#include "som_enemy_sprite.h"

/* once: the pack bytes (assets/som_enemy_sprites.bin, 0.5 MB, must outlive the calls); NULL = live render only */
static const uint8_t *es_pack; static size_t es_pack_len;

/* the picture of an enemy id, ready for ux_glue_set_enemy_sprite() */
int som_es_sprite_for_id(const uint8_t *pack, size_t pack_len, const struct som_es_src *live, const uint8_t *wram, int enemy_id,
                         uint8_t *scratch /* SOM_ES_BUF_BYTES */, uint32_t *argb /* 96 * 96 */, int *w, int *h, int *conf);
/* returns SOM_ES_FROM_PACK (1), SOM_ES_FROM_LIVE (2) or SOM_ES_FROM_NONE (0); *conf 3 verified, 2 probable, 1 partial */
```

Order inside the call: a pack picture of confidence >= 2; else a live render of an on-screen actor with that id whose pieces were all found; else any pack picture; else
nothing (the selector keeps its silhouette). The picture is tightly cropped, w and h <= 96 (`SOM_UX_SPRITE_MAX`); a larger one is reduced into a 96 square.

`live` is built from what the core gives, with no core patch:

```c
struct som_es_src live;
som_es_src_wram(&live, wram, vram);   /* OAM $7E:0800, CGRAM $7E:0600 ($BE00 while $2A != 0), OBSEL $7E:0091; vram = RETRO_MEMORY_VIDEO_RAM */
```

`vram` is `retro_get_memory_data(RETRO_MEMORY_VIDEO_RAM)` (id 3, 65536 bytes), exposed by the stock snes9x core and by the wide core. If the wide core is loaded, `live.oam`,
`live.cgram` and `live.obsel` may instead point at `retro_som_oam_get()`, `retro_som_cgram_get()` and `retro_som_regs_get()[0x2101]` (hardware copies, one frame ahead of the shadows at most).

## Glue function to add (UX agent, in `som_ux_glue.h`)

The glue owns the per-id cache already (`spr_ok` / `spr_id`), so the helper only fills what is missing:

```c
/* Fills the sprites of the enemies the UX still lacks.  Call it when the target selector is open (ux_g->pend_kind != 0) and every ~8 frames otherwise.
 * vram may be NULL (atlas only).  Returns the number of sprites set. */
static inline int ux_glue_fill_enemy_sprites(const uint8_t *wram, const uint8_t *vram, const uint8_t *pack, size_t pack_len) {
  static uint8_t scratch[SOM_ES_BUF_BYTES];
  static uint32_t argb[SOM_ES_SPRITE_MAX * SOM_ES_SPRITE_MAX];
  uint8_t ids[16];
  struct som_es_src live;
  int n = ux_glue_sprites_wanted(ids, 16), set = 0;
  if (vram) som_es_src_wram(&live, wram, vram);
  for (int i = 0; i < n; i++) {
    int w, h, conf;
    if (som_es_sprite_for_id(pack, pack_len, vram ? &live : NULL, wram, ids[i], scratch, argb, &w, &h, &conf) != SOM_ES_FROM_NONE)
      if (ux_glue_set_enemy_sprite(ids[i], argb, w, h) == 0) set++;
  }
  return set;
}
```

Cost: a pack lookup is a copy of at most 96 x 96 pixels; a live render is about 3 us. Both are safe on the loop thread.

## What each side changes

### UX agent

1. Add `ux_glue_fill_enemy_sprites` above to `som_ux_glue.h` (or accept the call sequence in the Android code). `som_ux_enemy_sprites_wanted()` lists every enemy in `enemy_idx`, on screen or not: that is right for the atlas
   (it does not need the enemy to be visible) and harmless for the live route (an actor off screen gives `NONE` and the id is asked again at the next call; keep the call rate at 8 frames).
2. `tools/companion/ux/build_assets.py`: build the atlas pack next to `som_ui_pack.bin`. The pack is a separate file (`som_enemy_sprites.bin`, format `SESP`, 115 pictures, 528,584 bytes), because the 512 x 512 `SUXP`
   atlas has room for about a third of it. Either copy `<state>/enemysprites/som_enemy_sprites.bin` (produced by `python3 tools/companion/enemysprites/build_atlas.py`) into the assets directory and record its sha256 in `tools/companion/assets/README.md`,
   or call `build_atlas.py` from `build_assets.py`. The pack contains Square Enix art: private builds only, never committed or published.
3. Retire `som_ux_enemy_crop_rect`, `som_ux_set_enemy_crop`, `som_ux_clear_enemy_crops`, `crop[16]` / `crop_id` / `crop_ok` and `ux_glue_crop_enemies` once the Android side no longer calls them (`docs/companion-ux-20261017.md` section 3k item 4 still describes the crop).
4. Do not clear the sprites on a map change: the pictures are keyed by enemy id, not by slot. Clear them only when the pack is reloaded. Keep `SOM_UX_SPRITES` (16) as is. No pack picture is larger than 85 px on a side; a live render of a large boss (up to 114 px) is reduced into 96 by `som_es_sprite_for_id`.
5. Add a test (`test_glue_path.c` style) that feeds a synthetic pack through `som_es_sprite_for_id` and `ux_glue_set_enemy_sprite` and checks that `ux_glue_sprites_wanted` becomes empty.

### Android agent

1. CMake (`tools/companion/android/app/src/main/cpp`): add `tools/companion/enemysprites/som_enemy_sprite.c` to the JNI library and its directory to the include path. C99, no dependencies, no allocation.
2. Load `som_enemy_sprites.bin` from the assets once at start (next to `som_ui_pack.bin`) and keep the bytes; a missing file means atlas-less operation (live render only).
3. In `som_jni.c`, delete the block `if ((total_frames & 7) == 0 && ux_g && wram && frame_w >= 256 && frame_h >= 224) { ... ux_glue_crop_enemies(crop_rgba, 256, 224); ... }` (it copies the frame and crops it: that is the camera the user does not want) and replace it with:
   ```c
   if ((total_frames & 7) == 0 && ux_g && wram) {
     const uint8_t *vram = (const uint8_t *)r_mem(3);   /* RETRO_MEMORY_VIDEO_RAM, 65536 bytes; NULL is tolerated */
     if (pthread_mutex_trylock(&ux_mu) == 0) { ux_glue_fill_enemy_sprites(wram, vram, es_pack, es_pack_len); pthread_mutex_unlock(&ux_mu); }
   }
   ```
   `st_crop` / `st_crop_n` and the `enemy crops` log can stay as the timing of this call.
4. The live fallback needs the PPU data only for ids the pack lacks or marks weak (the 11 bosses without a picture and bosses at confidence 1). If the wide core is the active core, pointing `live.oam` / `live.cgram` / `live.obsel` at its `retro_som_*` exports is optional.
5. The 3DS and desktop frontends call the same function with their own WRAM and VRAM pointers.

## Test on the PC

```
gcc -std=c99 -Wall -Wextra -fsanitize=address,undefined -o /tmp/test_es tools/companion/enemysprites/test_som_enemy_sprite.c tools/companion/enemysprites/som_enemy_sprite.c && /tmp/test_es   # 39 checks
python3 tools/companion/enemysprites/live_test.py MAP[,MAP...]          # live render vs the Godot atlas
python3 tools/companion/enemysprites/test_provider.py MAP[,MAP...]      # the provider on live scenes
```
