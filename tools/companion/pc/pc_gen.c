/* See pc_gen.h. */
#include "pc_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "../launcher/som_launcher.h"
#include "../launcher/som_launcher_catalog.h"
#include "../launcher/somrando_menu.h"
#include "pc_compat.h"
#include "pc_store.h"

#define now_s pc_now_s

int pcg_looks_generated(const char *path) {
  struct stat s;
  char t[15] = {0};
  FILE *f;
  int ok;
  if (stat(path, &s) != 0 || !S_ISREG(s.st_mode) || s.st_size != 4 * 1024 * 1024) return 0;
  f = fopen(path, "rb");
  if (!f) return 0;
  ok = fseek(f, 0xFFC0, SEEK_SET) == 0 && fread(t, 1, 14, f) == 14 && strcmp(t, "Secret of MANA") == 0;
  fclose(f);
  return ok;
}

int pcg_generate(const struct sl_launcher *L, const char *src_rom, const char *dst_rom, somrando_progress_fn progress, void *user, struct pcg_result *r) {
  char lines[4096], part[1100], why[160] = "";
  const char *diff = NULL;
  struct somrando_request req;
  double t0 = now_s();
  memset(r, 0, sizeof *r);
  if (!somrando_available()) { r->rc = SOMRANDO_ERR_UNAVAILABLE; snprintf(r->err, sizeof r->err, "No generator in this build"); return r->rc; }
  if (!somrando_menu_supports(L, why, sizeof why)) { r->rc = SOMRANDO_ERR_UNSUPPORTED; snprintf(r->err, sizeof r->err, "%s", why); return r->rc; }
  if (sl_config_lines(L, lines, sizeof lines) < 0) { r->rc = SOMRANDO_ERR_FAILED; snprintf(r->err, sizeof r->err, "Configuration too long"); return r->rc; }
  snprintf(part, sizeof part, "%s.part", dst_rom);
  memset(&req, 0, sizeof req);
  req.mode = sl_modes[L->cfg.mode].key;
  req.seed = L->cfg.seed;
  req.difficulty = sl_difficulty(L, &diff) ? diff : NULL;
  req.settings = lines;
  req.src_rom = src_rom;
  req.dst_rom = part;
  r->rc = somrando_generate(&req, progress, user, r->err, sizeof r->err);
  if (r->rc != SOMRANDO_OK) { if (!r->err[0] && r->rc != SOMRANDO_ERR_CANCELLED) snprintf(r->err, sizeof r->err, "Generation failed"); return r->rc; }
  if (!pcg_looks_generated(part)) { r->rc = SOMRANDO_ERR_FAILED; snprintf(r->err, sizeof r->err, "The generated ROM is not valid"); return r->rc; }
  {
    size_t n;
    uint8_t *d = pc_slurp(part, &n), h[32];
    if (!d) { r->rc = SOMRANDO_ERR_FAILED; snprintf(r->err, sizeof r->err, "The generated ROM could not be read back"); return r->rc; }
    sl_sha256(d, n, h);
    free(d);
    for (int i = 0; i < 32; i++) sprintf(r->sha256 + i * 2, "%02x", h[i]);
  }
  if (pc_rename(part, dst_rom) != 0) { r->rc = SOMRANDO_ERR_FAILED; snprintf(r->err, sizeof r->err, "The generated ROM could not be stored"); return r->rc; }
  r->seconds = now_s() - t0;
  return r->rc;
}

int pcg_write_sidecar(const struct sl_launcher *L, const char *path, const struct pcg_result *r) {
  static char cj[4096], side[4600];
  char key[17];
  const char *diff = NULL;
  int n;
  if (sl_config_json(L, cj, sizeof cj) < 0) return -1;
  sl_config_key(L, key);
  sl_difficulty(L, &diff);
  n = snprintf(side, sizeof side,
               "{\n \"tool\": \"som-pc\",\n \"generator\": \"Moppu/SecretOfManaRandomizer %s (libsomrando C port)\",\n \"mode\": \"%s\",\n \"seed\": \"%s\",\n"
               " \"difficulty\": \"%s\",\n \"output_sha256\": \"%s\",\n \"output_size\": %d,\n \"seconds\": %.2f,\n \"config_key\": \"%s\",\n \"config\": %s\n}\n",
               somrando_version(), sl_modes[L->cfg.mode].key, L->cfg.seed, diff ? diff : "", r->sha256, 4 * 1024 * 1024, r->seconds, key, cj);
  if (n < 0 || (size_t)n >= sizeof side) return -1;
  return pc_spit(path, side, (size_t)n);
}
