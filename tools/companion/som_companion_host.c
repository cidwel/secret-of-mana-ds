/* PC harness: runs a libretro SNES core headless, feeds WRAM to som_companion and executes macros.
 *
 *   som_companion_host CORE.so ROM SCRIPT [--opt key=value]...
 *
 * Script lines (one per line, '#' comments):
 *   LOAD path            load a core save state
 *   SAVE path            write a core save state
 *   RUN n                run n frames with no input
 *   HOLD first last BTN[,BTN]   not supported; use CMD/RUN
 *   SPANS path n         run n frames driven by an input span file ("FIRST LAST BTN[,BTN]", 0-based frames of this run)
 *   WRITE hexaddr hex    TEST FIXTURE ONLY: poke WRAM (never used by the macros)
 *   PAD BTN[,BTN] n      press buttons for n frames (manual input)
 *   CMD equip HERO TYPE | CMD item HERO RECORD | CMD cast HERO SPIRIT SPELL    start a macro
 *   WAIT n               run until the macro finishes (max n frames); prints a JSON result line
 *   ABORT                abort the running macro
 *   STATE                print the decoded state as one JSON line
 *   TOP path             write the top screen (game frame without the HUD rows) as PPM
 *   SHOT path            write the full video frame as PPM
 *   DUMP path            write the 128 KiB WRAM image
 *   EVENT hexbytes       TEST FIXTURE: inject an event-VM script at $7F:FF00 and start it
 *
 * Core options (for example snes9x_layer_3=disabled) are passed with --opt.  No ROM data is written anywhere.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include "som_companion.h"

struct info { const char *path; const void *data; size_t size; const char *meta; };
struct var { const char *key; const char *value; };

static int fmt;
static unsigned W, H;
static size_t pitch;
static uint8_t *last;
static size_t lastsz;
static unsigned pad_mask, pad_manual;
static const char *opt_key[32], *opt_val[32];
static int nopt, opt_dirty;
static const char *names[] = {"B", "Y", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT", "A", "X", "L", "R"};
#define HUD_FIRST_ROW 199   /* measured: BG3 HUD frame/text rows 199..216, portraits are sprites in the same rows */

static int env(unsigned cmd, void *d) {
  switch (cmd) {
  case 10: fmt = *(int *)d; return 1;
  case 9: case 31: *(const char **)d = "."; return 1;
  case 3: *(uint8_t *)d = 0; return 1;
  case 15: {
    struct var *v = d;
    for (int i = 0; i < nopt; i++) if (!strcmp(opt_key[i], v->key)) { v->value = opt_val[i]; return 1; }
    return 0;
  }
  case 17: *(uint8_t *)d = (uint8_t)opt_dirty; opt_dirty = 0; return 1;
  default: return 0;
  }
}
static void vid(const void *d, unsigned w, unsigned h, size_t p) {
  if (!d) return;
  W = w; H = h; pitch = p;
  if (lastsz < p * h) { free(last); last = malloc(p * h); lastsz = p * h; }
  memcpy(last, d, p * h);
}
static void as(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t asb(const int16_t *d, size_t n) { (void)d; return n; }
static void poll(void) {}
static int16_t inp(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  (void)idx;
  if (port != 0 || dev != 1) return 0;
  if (id == 256) return (int16_t)pad_mask;
  return id < 16 ? (pad_mask >> id) & 1 : 0;
}
static void pix(unsigned x, unsigned y, uint8_t *rgb) {
  if (fmt == 2) { uint16_t p = *(uint16_t *)(last + y * pitch + x * 2); rgb[0] = (p >> 11) << 3; rgb[1] = ((p >> 5) & 63) << 2; rgb[2] = (p & 31) << 3; }
  else if (fmt == 1) { uint32_t p = *(uint32_t *)(last + y * pitch + x * 4); rgb[0] = p >> 16; rgb[1] = p >> 8; rgb[2] = p; }
  else { uint16_t p = *(uint16_t *)(last + y * pitch + x * 2); rgb[0] = ((p >> 10) & 31) << 3; rgb[1] = ((p >> 5) & 31) << 3; rgb[2] = (p & 31) << 3; }
}
static void write_ppm(const char *path, unsigned rows) {
  FILE *o = fopen(path, "wb");
  if (!o || !last) return;
  if (rows > H) rows = H;
  fprintf(o, "P6\n%u %u\n255\n", W, rows);
  for (unsigned y = 0; y < rows; y++)
    for (unsigned x = 0; x < W; x++) { uint8_t c[3]; pix(x, y, c); fwrite(c, 1, 3, o); }
  fclose(o);
}

static void (*run)(void);
static size_t (*ser_size)(void);
static int (*ser)(void *, size_t);
static int (*unser)(const void *, size_t);
static uint8_t *wram;
static unsigned long gframe;
static struct som_macro macro;

static unsigned *span_mask; static unsigned span_n;
static void load_spans(const char *path, unsigned n) {
  free(span_mask); span_mask = calloc(n + 1, sizeof *span_mask); span_n = n;
  FILE *f = fopen(path, "r"); char line[512];
  if (!f) { fprintf(stderr, "cannot open spans %s\n", path); exit(6); }
  while (fgets(line, sizeof line, f)) {
    unsigned a, b; char btn[200];
    if (line[0] == '#' || sscanf(line, "%u %u %199s", &a, &b, btn) != 3) continue;
    unsigned m = 0; char *t = strtok(btn, ",");
    for (; t; t = strtok(NULL, ",")) for (unsigned i = 0; i < 12; i++) if (!strcmp(t, names[i])) m |= 1u << i;
    for (unsigned k = a; k <= b && k < n; k++) span_mask[k] |= m;
  }
  fclose(f);
}
static void frame(void) {
  uint16_t m = som_macro_busy(&macro) ? som_macro_step(&macro, wram) : 0;
  pad_mask = m | pad_manual;
  run();
  gframe++;
}

static unsigned parse_buttons(const char *s) {
  char tmp[200]; strncpy(tmp, s, 199); tmp[199] = 0; unsigned m = 0;
  for (char *t = strtok(tmp, ","); t; t = strtok(NULL, ","))
    for (unsigned i = 0; i < 12; i++) if (!strcmp(t, names[i])) m |= 1u << i;
  return m;
}

static void print_state(const char *tag) {
  struct som_state s; som_read_state(wram, &s);
  printf("{\"tag\":\"%s\",\"frame\":%lu,\"mode\":\"%s\",\"safe\":%d,\"map\":%u,\"pad_hero\":%d,\"gold\":%u,", tag, gframe,
         som_mode_name(s.mode), s.safe_to_inject, s.map_id, s.pad_hero == 0xFF ? -1 : s.pad_hero, s.gold);
  printf("\"ring\":{\"open\":%d,\"ready\":%d,\"owner\":%d,\"category\":%d,\"cursor\":%d,\"count\":%d},\"party\":[",
         s.ring.open, s.ring.ready, s.ring.owner, s.ring.category, s.ring.cursor, s.ring.count);
  for (int i = 0; i < SOM_PARTY; i++) {
    struct som_hero *h = &s.hero[i];
    printf("%s{\"present\":%d,\"alive\":%d,\"id\":%d,\"level\":%d,\"hp\":%d,\"hp_max\":%d,\"mp\":%d,\"mp_max\":%d,\"recharge\":%d,"
           "\"recharge_pct\":%d,\"weapon_type\":%d,\"weapon_row\":%d,\"status\":%d,\"poison_timer\":%d,\"weapon_level\":[",
           i ? "," : "", h->present, h->alive, h->id, h->level, h->hp, h->hp_max, h->mp, h->mp_max, h->recharge, h->recharge_pct,
           h->weapon_type, h->weapon_row, h->status, h->poison_timer);
    for (int t = 0; t < SOM_WEAPON_TYPES; t++) printf("%s%d", t ? "," : "", h->weapon_level[t]);
    printf("],\"spirit_mask\":[");
    for (int t = 0; t < SOM_SPIRITS; t++) printf("%s%d", t ? "," : "", h->spirit_mask[t]);
    printf("],\"spirit_level\":[");
    for (int t = 0; t < SOM_SPIRITS; t++) printf("%s%d", t ? "," : "", h->spirit_level[t]);
    printf("]}");
  }
  printf("],\"weapons\":[");
  for (int t = 0; t < SOM_WEAPON_TYPES; t++) printf("%s%d", t ? "," : "", s.weapon_slot[t]);
  printf("],\"items\":[");
  for (int i = 0; i < SOM_ITEM_RECORDS; i++) printf("%s[%d,%d]", i ? "," : "", s.item[i].id, s.item[i].qty);
  printf("]}\n");
  fflush(stdout);
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: core rom script [--opt k=v]...\n"); return 2; }
  for (int i = 4; i + 1 < argc + 1 && i < argc; i++) {
    if (!strcmp(argv[i], "--opt") && i + 1 < argc) {
      char *eq = strchr(argv[++i], '='); if (!eq || nopt >= 32) return 2;
      *eq = 0; opt_key[nopt] = argv[i]; opt_val[nopt++] = eq + 1;
    }
  }
  void *h = dlopen(argv[1], RTLD_NOW);
  if (!h) { fprintf(stderr, "%s\n", dlerror()); return 3; }
#define SYM(t, n) t n = dlsym(h, #n)
  void (*set_env)(void *) = dlsym(h, "retro_set_environment");
  void (*set_vid)(void *) = dlsym(h, "retro_set_video_refresh");
  void (*set_as)(void *) = dlsym(h, "retro_set_audio_sample");
  void (*set_asb)(void *) = dlsym(h, "retro_set_audio_sample_batch");
  void (*set_poll)(void *) = dlsym(h, "retro_set_input_poll");
  void (*set_inp)(void *) = dlsym(h, "retro_set_input_state");
  void (*init)(void) = dlsym(h, "retro_init");
  int (*load)(const struct info *) = dlsym(h, "retro_load_game");
  uint8_t *(*memdata)(unsigned) = dlsym(h, "retro_get_memory_data");
  run = dlsym(h, "retro_run");
  ser_size = dlsym(h, "retro_serialize_size"); ser = dlsym(h, "retro_serialize"); unser = dlsym(h, "retro_unserialize");
  set_env(env); set_vid(vid); set_as(as); set_asb(asb); set_poll(poll); set_inp(inp); init();
  FILE *f = fopen(argv[2], "rb"); if (!f) { fprintf(stderr, "cannot open ROM\n"); return 4; }
  fseek(f, 0, SEEK_END); size_t n = ftell(f); rewind(f);
  uint8_t *rom = malloc(n); if (fread(rom, 1, n, f) != n) return 4; fclose(f);
  struct info gi = {argv[2], rom, n, NULL};
  if (!load(&gi)) { fprintf(stderr, "retro_load_game failed\n"); return 5; }
  wram = memdata(2);
  if (!wram) { fprintf(stderr, "core exposes no WRAM\n"); return 5; }
  som_macro_init(&macro);

  FILE *sc = fopen(argv[3], "r"); if (!sc) { fprintf(stderr, "cannot open script\n"); return 6; }
  char line[4096];
  while (fgets(line, sizeof line, sc)) {
    char w[32], a[1024], b[256], c[64], d[64];
    a[0] = b[0] = c[0] = d[0] = 0;
    if (sscanf(line, "%31s", w) != 1 || w[0] == '#') continue;
    char *p = line + strlen(w);
    if (!strcmp(w, "LOAD") || !strcmp(w, "SAVE")) {
      sscanf(p, "%1023s", a);
      if (w[0] == 'L') {
        FILE *s = fopen(a, "rb"); if (!s) { fprintf(stderr, "no state %s\n", a); return 9; }
        fseek(s, 0, SEEK_END); size_t sn = ftell(s); rewind(s); void *buf = malloc(sn);
        if (fread(buf, 1, sn, s) != sn) return 9;
        fclose(s);
        if (!unser(buf, sn)) { fprintf(stderr, "unserialize failed\n"); return 9; }
        free(buf);
      } else {
        size_t sn = ser_size(); void *buf = malloc(sn);
        if (!ser(buf, sn)) return 8;
        FILE *s = fopen(a, "wb"); fwrite(buf, 1, sn, s); fclose(s); free(buf);
      }
    } else if (!strcmp(w, "RUN")) { unsigned k; sscanf(p, "%u", &k); while (k--) frame(); }
    else if (!strcmp(w, "SPANS")) {
      unsigned k; sscanf(p, "%1023s %u", a, &k); load_spans(a, k);
      for (unsigned i = 0; i < k; i++) { pad_manual = span_mask[i]; frame(); }
      pad_manual = 0;
    }
    else if (!strcmp(w, "PAD")) { unsigned k; sscanf(p, "%1023s %u", a, &k); pad_manual = parse_buttons(a); while (k--) frame(); pad_manual = 0; }
    else if (!strcmp(w, "WRITE")) {
      unsigned addr; sscanf(p, "%x %1023s", &addr, a);
      for (size_t i = 0; a[2 * i] && a[2 * i + 1] && addr + i < SOM_WRAM_SIZE; i++) { unsigned v; sscanf(a + 2 * i, "%2x", &v); wram[addr + i] = (uint8_t)v; }
    } else if (!strcmp(w, "EVENT")) {
      sscanf(p, "%1023s", a);
      for (size_t i = 0; a[2 * i] && a[2 * i + 1]; i++) { unsigned v; sscanf(a + 2 * i, "%2x", &v); wram[0x1FF00 + i] = (uint8_t)v; }
      wram[0xD1] = 0x00; wram[0xD2] = 0xFF; wram[0xD3] = 0x7F; frame(); wram[0xD0] = 1;
    } else if (!strcmp(w, "CMD")) {
      struct som_cmd cmd; memset(&cmd, 0, sizeof cmd);
      unsigned hero = 0, arg = 0, spell = 0;
      sscanf(p, "%63s %u %u %u", a, &hero, &arg, &spell);
      cmd.hero = hero; cmd.arg = arg; cmd.spell = spell;
      cmd.kind = !strcmp(a, "equip") ? SOM_CMD_EQUIP_WEAPON : !strcmp(a, "item") ? SOM_CMD_USE_ITEM : SOM_CMD_CAST_MAGIC;
      int ok = som_macro_start(&macro, wram, &cmd);
      if (!ok) printf("{\"event\":\"refused\",\"frame\":%lu,\"result\":\"%s\",\"note\":\"%s\"}\n", gframe, som_result_name(macro.result), macro.note);
    } else if (!strcmp(w, "WAIT")) {
      unsigned k; sscanf(p, "%u", &k); unsigned long f0 = gframe;
      while (k-- && som_macro_busy(&macro)) frame();
      printf("{\"event\":\"macro\",\"start_frame\":%lu,\"frames\":%lu,\"busy\":%d,\"result\":\"%s\",\"note\":\"%s\"}\n", f0,
             gframe - f0, som_macro_busy(&macro), som_result_name(macro.result), macro.note);
    } else if (!strcmp(w, "ABORT")) som_macro_abort(&macro);
    else if (!strcmp(w, "STATE")) { sscanf(p, "%63s", a); print_state(a); }
    else if (!strcmp(w, "TOP")) { sscanf(p, "%1023s", a); write_ppm(a, HUD_FIRST_ROW); }
    else if (!strcmp(w, "SHOT")) { sscanf(p, "%1023s", a); write_ppm(a, H); }
    else if (!strcmp(w, "DUMP")) { sscanf(p, "%1023s", a); FILE *o = fopen(a, "wb"); fwrite(wram, 1, SOM_WRAM_SIZE, o); fclose(o); }
  }
  return 0;
}
