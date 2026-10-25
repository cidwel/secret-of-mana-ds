/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * CustomMusic, MuteMusic, MusicExpander and RandomMusicHack (processing/hacks/common/music/) and FlammieDrum (common/procgen), modes `rando`, `bossrush` and `chaos`.  Author of every
 * upstream hack: Moppleton.  Translated to C 2026-10-18.
 *
 * The C# looks for ./music/{samples,music,vanillamusictypes}.xml relative to the working directory.  This port has no music pack
 * support: it reproduces the behaviour when that folder is absent (default song categories, nothing imported).
 */
#include <string.h>

#include "sr_core.h"
#include "sr_p4.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

#define SR_W4_NTYPES 7
enum { T_OVERWORLD, T_DUNGEON, T_BATTLE, T_TOWN, T_FLIGHT, T_PALACE, T_VICTORY };

struct sr_w4_song_type {
  uint8_t id;
  uint8_t type;
};

/* originalSongTypes in insertion order */
static const struct sr_w4_song_type sr_w4_song_types[] = {
  {4, T_BATTLE}, {57, T_BATTLE}, {12, T_BATTLE},
  {1, T_FLIGHT}, {2, T_FLIGHT}, {3, T_FLIGHT},
  {0, T_OVERWORLD}, {5, T_OVERWORLD}, {6, T_OVERWORLD}, {10, T_OVERWORLD}, {36, T_OVERWORLD}, {39, T_OVERWORLD},
  {14, T_DUNGEON}, {21, T_DUNGEON}, {27, T_DUNGEON}, {38, T_DUNGEON}, {42, T_DUNGEON}, {49, T_DUNGEON},
  {9, T_TOWN}, {13, T_TOWN}, {15, T_TOWN}, {17, T_TOWN}, {22, T_TOWN}, {31, T_TOWN}, {37, T_TOWN},
  {29, T_PALACE},
  {20, T_VICTORY},
};
#define SR_W4_NSONGS ((int)(sizeof sr_w4_song_types / sizeof sr_w4_song_types[0]))

/* MuteMusic.process */
static void sr_w4_mute_music(struct sr_ctx *c) {
  static const int skip[] = {24, 25, 26, 30, 40, 45};
  uint8_t *rom = c->out;
  for (int id = 0; id < 59; id++) {
    int skipped = 0;
    for (int k = 0; k < 6; k++) skipped |= (skip[k] == id);
    if (skipped) continue;
    int o = 0x33D39 + id * 3;
    int offset = rom[o] | (rom[o + 1] << 8) | (rom[o + 2] << 16);
    offset -= 0xC00000;
    if (offset < 0 || offset + 40 >= SR_ROM_OUT_SIZE) sr_fail(c, SOMRANDO_ERR_FAILED, "music offset out of range");
    int size = sr_ushort_from_bytes(rom, offset);
    offset += 4 + 16;
    for (int i = offset; i < offset + 10; i++)
      if (rom[i] == 0xF8) {
        rom[i + 1] = 0;
        break;
      }
    if (offset + size - 18 + 2 >= SR_ROM_OUT_SIZE) sr_fail(c, SOMRANDO_ERR_FAILED, "music size out of range");
    for (int i = offset; i < offset + size - 18; i++) {
      if (rom[i] == 0xFD && rom[i + 1] == 0x25) {
        rom[i] = 0xD2;
        rom[i + 1] = 0;
      }
      if (rom[i] == 0xD2 && rom[i + 1] == 0xFF) rom[i + 1] = 0;
    }
  }
}

static void sr_w4_poke24(struct sr_ctx *c, int off, int addr) {
  P(off, addr);
  P(off + 1, addr >> 8);
  P(off + 2, (addr >> 16) + 0xC0);
}

/* MusicExpander.expandMusic with no imported songs or samples */
static void sr_w4_expand_music(struct sr_ctx *c) {
  uint8_t *rom = c->out;
  int offset = c->working_offset;
  int music_offsets = offset;
  sr_w4_poke24(c, 0x30231, music_offsets);
  sr_w4_poke24(c, 0x30237, music_offsets + 1);
  sr_w4_poke24(c, 0x3023D, music_offsets + 2);
  for (int i = 0; i < 64 * 3; i++) E(rom[0x33D39 + i]);

  int sample_table = c->working_offset;
  sr_w4_poke24(c, 0x302B7, sample_table);
  for (int i = 0; i < 64 * 32; i++) E(rom[0x33F22 + i]);

  int sample_loops = c->working_offset;
  sr_w4_poke24(c, 0x3050B, sample_loops);
  for (int i = 0; i < 33 * 2; i++) E(rom[0x33E5C + i]);

  int sample_freqs = c->working_offset;
  sr_w4_poke24(c, 0x304F7, sample_freqs);
  for (int i = 0; i < 33 * 2; i++) E(rom[0x33E9E + i]);

  int sample_adsr = c->working_offset;
  sr_w4_poke24(c, 0x30515, sample_adsr);
  for (int i = 0; i < 33 * 2; i++) E(rom[0x33EE0 + i]);

  int sample_offsets = c->working_offset;
  sr_w4_poke24(c, 0x30305, sample_offsets);
  sr_w4_poke24(c, 0x3030B, sample_offsets + 1);
  sr_w4_poke24(c, 0x30311, sample_offsets + 2);
  sr_w4_poke24(c, 0x3044F, sample_offsets);
  sr_w4_poke24(c, 0x30455, sample_offsets + 1);
  sr_w4_poke24(c, 0x3045B, sample_offsets + 2);
  for (int i = 0; i < 33 * 3; i++) E(rom[0x33DF9 + i]);
}

/* RandomMusicHack.process; songs[type] lists {id, echo1, echo2} for every song of that category */
static void sr_w4_random_music(struct sr_ctx *c, uint8_t songs[SR_W4_NTYPES][SR_W4_NSONGS][3], const int count[SR_W4_NTYPES]) {
  int subr[SR_W4_NSONGS];
  static const uint8_t head[] = {0x0B, 0xC2, 0x20, 0xA9, 0x00, 0x03, 0x48, 0x2B, 0xE2, 0x20, 0x22, 0x9C, 0x38, 0xC0, 0x2B, 0xC2, 0x20, 0x29,
                                 0xFF, 0x00, 0x8F, 0x04, 0x42, 0x00, 0xE2, 0x20, 0xA9};
  static const uint8_t mid[] = {0x8F, 0x06, 0x42, 0x00, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xAF, 0x16, 0x42, 0x00};
  for (int s = 0; s < SR_W4_NSONGS; s++) {
    int t = sr_w4_song_types[s].type, n = count[t];
    sr_ensure_space_in_bank(&c->working_offset, 44 + n * 20);
    subr[s] = c->working_offset;
    if (n > 0) {
      for (size_t k = 0; k < sizeof head; k++) E(head[k]);
      E(n);
      for (size_t k = 0; k < sizeof mid; k++) E(mid[k]);
      for (int i = 0; i < n; i++) {
        E(0xC9); E(i);
        E(0xD0); E(0x10);
        E(0xA9); E(songs[t][i][0]);
        E(0x8D); E(0x01); E(0x1E);
        E(0xA9); E(songs[t][i][1]);
        E(0x8D); E(0x02); E(0x1E);
        E(0xA9); E(songs[t][i][2]);
        E(0x8D); E(0x03); E(0x1E);
        E(0x6B);
      }
    }
  }

  int last_song = 0x7E0400 + 47;
  int w = c->working_offset;
  P(0x301AD, 0x22);
  P(0x301AE, w);
  P(0x301AF, w >> 8);
  P(0x301B0, (w >> 16) + 0xC0);

  static const uint8_t pre[] = {0xC2, 0x20, 0x48, 0xE2, 0x20, 0xAD, 0x01, 0x1E};
  for (size_t k = 0; k < sizeof pre; k++) E(pre[k]);
  E(0xCF); E(last_song); E(last_song >> 8); E(last_song >> 16);
  E(0xD0); E(0x17);
  static const uint8_t same[] = {0xC2, 0x20, 0x68, 0xE2, 0x20, 0xA5, 0x02, 0x29, 0xF0, 0x68, 0x68, 0x68,
                                 0xC2, 0x20, 0xC2, 0x10, 0x7A, 0xFA, 0x68, 0x28, 0x2B, 0xAB, 0x6B};
  for (size_t k = 0; k < sizeof same; k++) E(same[k]);
  E(0x8F); E(last_song); E(last_song >> 8); E(last_song >> 16);

  for (int s = 0; s < SR_W4_NSONGS; s++) {
    E(0xC9); E(sr_w4_song_types[s].id);
    E(0xD0); E(0x0E);
    E(0x22); E(subr[s]); E(subr[s] >> 8); E((subr[s] >> 16) + 0xC0);
    E(0xC2); E(0x20); E(0x68); E(0xE2); E(0x20);
    E(0xA5); E(0x02); E(0x29); E(0xF0); E(0x6B);
  }
  E(0xC2); E(0x20); E(0x68); E(0xE2); E(0x20);
  E(0xA5); E(0x02); E(0x29); E(0xF0); E(0x6B);
}

/* FlammieDrum.process (processing/hacks/common/procgen) with no imported songs: the drum plays a random vanilla song */
static void sr_w4_flammie_drum(struct sr_ctx *c) {
  static const uint8_t vanilla[] = {
    0x00, 0x1A, 0x8F, 0x01, 0x09, 0xFF, 0x02, 0x0D, 0xFF, 0x03, 0x0B, 0xFF, 0x04, 0x04, 0xFF, 0x05, 0x10, 0x8F,
    0x06, 0x13, 0x8F, 0x07, 0x18, 0x8F, 0x08, 0x1B, 0x8F, 0x09, 0x15, 0x8F, 0x0A, 0x13, 0x8F, 0x0B, 0x17, 0x8F,
    0x0C, 0x06, 0xFF, 0x0D, 0x12, 0x8F, 0x0E, 0x1B, 0x8F, 0x0F, 0x18, 0x8F, 0x10, 0x1B, 0x8F, 0x11, 0x12, 0x8F,
    0x12, 0x42, 0x4F, 0x13, 0x1A, 0x8F, 0x14, 0x10, 0x8F, 0x15, 0x12, 0x8F, 0x16, 0x1A, 0x8F, 0x1B, 0x13, 0x8F,
    0x1C, 0x1A, 0x8F, 0x1D, 0x1B, 0x8F, 0x1F, 0x1A, 0x8F, 0x20, 0x1A, 0x8F, 0x24, 0x1C, 0x8F, 0x25, 0x14, 0x8F,
    0x26, 0x15, 0x8F, 0x27, 0x1C, 0x8F, 0x29, 0x1C, 0x8F, 0x2A, 0x18, 0x8F, 0x2B, 0x0C, 0x8F, 0x2C, 0x4F, 0x8F,
    0x31, 0x16, 0x8F, 0x34, 0x1B, 0x8F, 0x36, 0x16, 0x8F, 0x37, 0x12, 0x8F, 0x38, 0x1F, 0x8F, 0x39, 0x14, 0xFF,
    0x3A, 0x1B, 0x8F,
  };
  uint8_t *rom = c->out;
  int n = (int)sizeof vanilla;
  int songs_loc = c->working_offset + 0xC00000;
  int songs_loc1 = songs_loc + 1;
  for (int i = 0; i < n; i++) E(vanilla[i]);

  int play = c->working_offset + 0xC00000;
  for (int i = 0x1DBCD; i <= 0x1DC21; i++) rom[i] = 0xEA;
  P(0x1DC24, 0x22);
  P(0x1DC25, play);
  P(0x1DC26, play >> 8);
  P(0x1DC27, play >> 16);
  for (int i = 0x1DC28; i <= 0x1DC2F; i++) rom[i] = 0xEA;

  static const uint8_t a[] = {0xE2, 0x20, 0xA9, 0x01, 0x8F, 0x00, 0x1E, 0x7E, 0x8F, 0x28, 0xCF, 0x7E,
                              0x0B, 0xC2, 0x20, 0xA9, 0x00, 0x03, 0x48, 0x2B, 0xE2, 0x20, 0x22, 0x9C, 0x38, 0xC0, 0x2B,
                              0xC2, 0x20, 0x29, 0xFF, 0x00, 0x8F, 0x04, 0x42, 0x00, 0xE2, 0x20, 0xA9};
  for (size_t k = 0; k < sizeof a; k++) E(a[k]);
  E(n / 3);
  static const uint8_t b[] = {0x8F, 0x06, 0x42, 0x00, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
                              0xAF, 0x16, 0x42, 0x00, 0x0A, 0x6F, 0x16, 0x42, 0x00, 0xC2, 0x20, 0xDA, 0xAA, 0xBF};
  for (size_t k = 0; k < sizeof b; k++) E(b[k]);
  E(songs_loc); E(songs_loc >> 8); E(songs_loc >> 16);
  E(0x8F); E(0x01); E(0x1E); E(0x7E);
  E(0xBF); E(songs_loc1); E(songs_loc1 >> 8); E(songs_loc1 >> 16);
  static const uint8_t d[] = {0x8F, 0x02, 0x1E, 0x7E, 0xFA, 0xE2, 0x20, 0x22, 0x04, 0x00, 0xC3, 0xC2, 0x20, 0x6B};
  for (size_t k = 0; k < sizeof d; k++) E(d[k]);
  sr_names_set(c, 0xBA + 7, "Random Music");
}

static int sr_w4_mute_music_hack(struct sr_ctx *c) {
  sr_w4_mute_music(c);
  return 1;
}

int sr_hack_custom_music(struct sr_ctx *c) {
  if (SB("muteMusic")) {
    if (!strcmp(sr_settings_get(c->settings, "mode"), "open")) sr_p4_nested(c, "Mute music", sr_w4_mute_music_hack);
    else sr_w4_mute_music(c);
    return 0;
  }
  const char *mode = sr_settings_get(c->settings, "mode");
  int flammie_drum = 0, random_music = 1;
  if (!strcmp(mode, "ancientcave")) {
    /* ac, boss rush, chaos add the flammie drum; the random music hack only when acMusic is set (and always before the drum) */
    flammie_drum = 1;
    random_music = SB("acMusic");
  } else if (!strcmp(mode, "bossrush") || !strcmp(mode, "chaos")) {
    flammie_drum = 1;
    random_music = 0;
  } else if (strcmp(mode, "rando") != 0 && strcmp(mode, "open") != 0) {
    sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: music in this mode");
  }
  if (!flammie_drum && !SB(!strcmp(mode, "open") ? "opMusic" : "rMusic")) return 0;

  /* vanillaSongParameters from the first command of events 0x700..0x73A */
  uint8_t params[256][2];
  memset(params, 0, sizeof params);
  uint8_t have[256];
  memset(have, 0, sizeof have);
  for (int i = 0; i <= 0x3A; i++) {
    int start = sr_event_start_offset(c, c->orig, i + 0x700);
    int id = sr_peek_orig(c, start + 2);
    params[id][0] = sr_peek_orig(c, start + 3);
    params[id][1] = sr_peek_orig(c, start + 4);
    have[id] = 1;
  }
  uint8_t songs[SR_W4_NTYPES][SR_W4_NSONGS][3];
  int count[SR_W4_NTYPES] = {0};
  for (int s = 0; s < SR_W4_NSONGS; s++) {
    int id = sr_w4_song_types[s].id, t = sr_w4_song_types[s].type;
    if (!have[id]) sr_fail(c, SOMRANDO_ERR_FAILED, "vanilla song %d has no event parameters", id);
    songs[t][count[t]][0] = (uint8_t)id;
    songs[t][count[t]][1] = params[id][0];
    songs[t][count[t]][2] = params[id][1];
    count[t]++;
  }
  sr_w4_expand_music(c);
  if (random_music) sr_w4_random_music(c, songs, count);
  if (flammie_drum) sr_w4_flammie_drum(c);
  return 1;
}
