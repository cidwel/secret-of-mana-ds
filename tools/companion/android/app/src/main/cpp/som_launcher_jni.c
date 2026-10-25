/* JNI side of the start menu (tools/companion/launcher).  One menu instance, driven from the UI thread; the generator, when there is one,
 * runs on a Java worker thread through nativeGenerate() and reports progress back to Java, which posts it to the UI thread. */
#include <jni.h>
#include <android/bitmap.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "launcher/som_launcher.h"
#include "launcher/som_launcher_draw.h"
#include "launcher/somrando_api.h"
#include "launcher/somrando_menu.h"
#include "som_gameplay_apply.h"
#include "som_start_pages.h"

static struct sl_launcher L;
static uint8_t *pack_copy;

#define FN(name) Java_org_somdecomp_companion_LauncherNative_##name

JNIEXPORT void JNICALL FN(init)(JNIEnv *e, jclass c, jlong entropy) { (void)e; (void)c; sl_init(&L, (uint64_t)entropy); }

JNIEXPORT void JNICALL FN(loadPack)(JNIEnv *e, jclass c, jbyteArray b) {
  (void)c;
  jsize n = (*e)->GetArrayLength(e, b);
  uint8_t *copy = (uint8_t *)malloc((size_t)n);
  if (!copy) return;
  (*e)->GetByteArrayRegion(e, b, 0, n, (jbyte *)copy);
  if (sl_draw_load_pack(copy, (size_t)n) == 0) { free(pack_copy); pack_copy = copy; } else free(copy);
}

JNIEXPORT void JNICALL FN(setStatus)(JNIEnv *e, jclass c, jstring o, jstring g) {
  (void)c;
  const char *a = (*e)->GetStringUTFChars(e, o, NULL), *b = (*e)->GetStringUTFChars(e, g, NULL);
  sl_set_status(&L, a, b);
  (*e)->ReleaseStringUTFChars(e, o, a);
  (*e)->ReleaseStringUTFChars(e, g, b);
}

JNIEXPORT void JNICALL FN(setView)(JNIEnv *e, jclass c, jint w, jint h) { (void)e; (void)c; sl_set_view(&L, w, h); }
/* X and Y on the main screen open the gameplay and credits pages (Thor round 4); while a page is open it takes every button. */
JNIEXPORT void JNICALL FN(key)(JNIEnv *e, jclass c, jint b) {
  (void)e; (void)c;
  if (ssp_page()) { ssp_key(b); return; }
  if (L.screen == SL_MAIN && b == SL_X) { ssp_open(SSP_GAMEPLAY); return; }
  if (L.screen == SL_MAIN && b == SL_Y) { ssp_open(SSP_CREDITS); return; }
  sl_key(&L, b);
}
JNIEXPORT void JNICALL FN(tap)(JNIEnv *e, jclass c, jint x, jint y, jint w, jint h) {
  (void)e; (void)c;
  if (ssp_page()) { ssp_tap(x, y, w, h); return; }
  if (L.screen == SL_MAIN) {
    int hit = ssp_footer_hit(x, y, w, h);
    if (hit) { ssp_open(hit == SSP_H_GAMEPLAY ? SSP_GAMEPLAY : SSP_CREDITS); return; }
  }
  sl_tap(&L, x, y, w, h);
}
JNIEXPORT void JNICALL FN(scroll)(JNIEnv *e, jclass c, jint rows) { (void)e; (void)c; if (ssp_page()) ssp_scroll(rows); else sl_scroll(&L, rows); }
JNIEXPORT void JNICALL FN(tick)(JNIEnv *e, jclass c) { (void)e; (void)c; sl_tick(&L); }
JNIEXPORT jint JNICALL FN(poll)(JNIEnv *e, jclass c) { (void)e; (void)c; int a = SL_ACT_NONE; sl_poll(&L, &a); return a; }
JNIEXPORT jint JNICALL FN(revision)(JNIEnv *e, jclass c) { (void)e; (void)c; return (jint)(L.rev * 2654435761u ^ ssp_rev() * 40503u ^ (uint32_t)ssp_page() << 28); }
/* screen | cursor << 8 | seed digit << 24 | notice flag << 30: what the host compares around an input to pick a sound */
JNIEXPORT jint JNICALL FN(snapshot)(JNIEnv *e, jclass c) { (void)e; (void)c; return (jint)((L.screen & 0xFF) | ((L.cursor & 0xFFFF) << 8) | ((L.seed_pos & 0x3F) << 24) | (L.notice_t > 0 ? 1 << 30 : 0)); }
JNIEXPORT jint JNICALL FN(screen)(JNIEnv *e, jclass c) { (void)e; (void)c; return (jint)L.screen; }
JNIEXPORT jint JNICALL FN(rowHeightPx)(JNIEnv *e, jclass c, jint w, jint h) {
  (void)e; (void)c;
  struct sl_metrics m;
  sl_metrics_for(w, h, &m);
  return m.row_h * m.s;
}

JNIEXPORT jboolean JNICALL FN(render)(JNIEnv *e, jclass c, jobject bmp) {
  (void)c;
  AndroidBitmapInfo info;
  void *px;
  if (AndroidBitmap_getInfo(e, bmp, &info) < 0 || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return JNI_FALSE;
  if (AndroidBitmap_lockPixels(e, bmp, &px) < 0) return JNI_FALSE;
  /* the renderer writes 0xAARRGGBB; RGBA_8888 in memory is R,G,B,A bytes, so swap R and B afterwards */
  uint32_t *p = (uint32_t *)px;
  int stride = (int)(info.stride / 4);
  if (ssp_page()) ssp_draw(p, stride, (int)info.width, (int)info.height);
  else {
    sl_draw(&L, p, stride, (int)info.width, (int)info.height);
    if (L.screen == SL_MAIN) ssp_draw_footer(p, stride, (int)info.width, (int)info.height, !ssp_gameplay_default());
  }
  for (uint32_t y = 0; y < info.height; y++)
    for (uint32_t x = 0; x < info.width; x++) {
      uint32_t v = p[y * stride + x];
      p[y * stride + x] = (v & 0xFF00FF00u) | ((v & 0xFF) << 16) | ((v >> 16) & 0xFF);
    }
  AndroidBitmap_unlockPixels(e, bmp);
  return JNI_TRUE;
}

/* The top screen: wallpaper on the main menu, otherwise the summary of the game being set up. */
JNIEXPORT jboolean JNICALL FN(renderTop)(JNIEnv *e, jclass c, jobject bmp) {
  (void)c;
  AndroidBitmapInfo info;
  void *px;
  if (AndroidBitmap_getInfo(e, bmp, &info) < 0 || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return JNI_FALSE;
  if (AndroidBitmap_lockPixels(e, bmp, &px) < 0) return JNI_FALSE;
  uint32_t *p = (uint32_t *)px;
  int stride = (int)(info.stride / 4);
  sl_draw_top(&L, p, stride, (int)info.width, (int)info.height);
  for (uint32_t y = 0; y < info.height; y++)
    for (uint32_t x = 0; x < info.width; x++) {
      uint32_t v = p[y * stride + x];
      p[y * stride + x] = (v & 0xFF00FF00u) | ((v & 0xFF) << 16) | ((v >> 16) & 0xFF);
    }
  AndroidBitmap_unlockPixels(e, bmp);
  return JNI_TRUE;
}
/* Changes only when the top screen would look different: the wallpaper does not depend on the cursor of the main menu. */
JNIEXPORT jint JNICALL FN(topRevision)(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  return (L.screen == SL_MAIN || L.screen == SL_PLAY) ? (jint)(1000000 + L.screen) : (jint)L.rev;
}

static jstring jstr(JNIEnv *e, const char *s) { return (*e)->NewStringUTF(e, s); }
JNIEXPORT jstring JNICALL FN(configJson)(JNIEnv *e, jclass c) { (void)c; char b[4096]; return jstr(e, sl_config_json(&L, b, sizeof b) < 0 ? "" : b); }
JNIEXPORT jstring JNICALL FN(configLines)(JNIEnv *e, jclass c) { (void)c; char b[4096]; return jstr(e, sl_config_lines(&L, b, sizeof b) < 0 ? "" : b); }
JNIEXPORT jstring JNICALL FN(configKey)(JNIEnv *e, jclass c) { (void)c; char k[17]; sl_config_key(&L, k); return jstr(e, k); }
JNIEXPORT jstring JNICALL FN(configField)(JNIEnv *e, jclass c, jint which) {
  (void)c;
  const char *d = NULL;
  switch (which) {
    case 0: return jstr(e, sl_modes[L.cfg.mode].key);
    case 1: return jstr(e, L.cfg.seed);
    case 2: return jstr(e, sl_difficulty(&L, &d) ? d : "");
    default: return jstr(e, "");
  }
}
JNIEXPORT jint JNICALL FN(actArg)(JNIEnv *e, jclass c) { (void)e; (void)c; return sl_act_arg(&L); }
static const char *utf(JNIEnv *e, jstring j) { return j ? (*e)->GetStringUTFChars(e, j, NULL) : ""; }
static void rel(JNIEnv *e, jstring j, const char *s) { if (j && s) (*e)->ReleaseStringUTFChars(e, j, s); }
JNIEXPORT void JNICALL FN(savedClear)(JNIEnv *e, jclass c) { (void)e; (void)c; sl_saved_clear(&L); }
JNIEXPORT void JNICALL FN(savedHidden)(JNIEnv *e, jclass c, jint n) { (void)e; (void)c; sl_saved_set_hidden(&L, n); }
JNIEXPORT jint JNICALL FN(savedAdd)(JNIEnv *e, jclass c, jstring key, jstring preset, jstring mode, jstring seed, jstring diff, jstring date,
                                    jstring path, jstring lines) {
  (void)c;
  const char *k = utf(e, key), *p = utf(e, preset), *m = utf(e, mode), *s = utf(e, seed), *d = utf(e, diff), *t = utf(e, date), *a = utf(e, path), *l = utf(e, lines);
  int slot = sl_saved_add(&L, k, p, m, s, d, t, a, l);
  rel(e, key, k); rel(e, preset, p); rel(e, mode, m); rel(e, seed, s); rel(e, diff, d); rel(e, date, t); rel(e, path, a); rel(e, lines, l);
  return slot;
}
JNIEXPORT jint JNICALL FN(savedCount)(JNIEnv *e, jclass c) { (void)e; (void)c; return sl_saved_count(&L); }
/* The selected saved game's key, "" when none (the slot the host just asked to delete is read with actArg). */
JNIEXPORT jstring JNICALL FN(savedKey)(JNIEnv *e, jclass c, jint slot) {
  (void)c;
  return jstr(e, slot >= 0 && slot < SL_SAVED_MAX && L.saved[slot].used ? L.saved[slot].key : "");
}
JNIEXPORT jstring JNICALL FN(romPath)(JNIEnv *e, jclass c) { (void)c; return jstr(e, sl_rom_path(&L)); }

JNIEXPORT void JNICALL FN(genCached)(JNIEnv *e, jclass c, jstring p) {
  (void)c;
  const char *s = (*e)->GetStringUTFChars(e, p, NULL);
  sl_gen_cached(&L, s);
  (*e)->ReleaseStringUTFChars(e, p, s);
}
JNIEXPORT void JNICALL FN(genDone)(JNIEnv *e, jclass c, jstring p) {
  (void)c;
  const char *s = (*e)->GetStringUTFChars(e, p, NULL);
  sl_gen_done(&L, s);
  (*e)->ReleaseStringUTFChars(e, p, s);
}
JNIEXPORT void JNICALL FN(genBegin)(JNIEnv *e, jclass c) { (void)e; (void)c; sl_gen_begin(&L); }
JNIEXPORT void JNICALL FN(genProgress)(JNIEnv *e, jclass c, jint step, jint total, jstring n) {
  (void)c;
  const char *s = (*e)->GetStringUTFChars(e, n, NULL);
  sl_gen_progress(&L, step, total, s);
  (*e)->ReleaseStringUTFChars(e, n, s);
}
JNIEXPORT void JNICALL FN(genFail)(JNIEnv *e, jclass c, jstring m) {
  (void)c;
  const char *s = (*e)->GetStringUTFChars(e, m, NULL);
  sl_gen_fail(&L, s);
  (*e)->ReleaseStringUTFChars(e, m, s);
}
JNIEXPORT void JNICALL FN(genUnavailable)(JNIEnv *e, jclass c, jstring m) {
  (void)c;
  const char *s = (*e)->GetStringUTFChars(e, m, NULL);
  sl_gen_unavailable(&L, s);
  (*e)->ReleaseStringUTFChars(e, m, s);
}

/* ---- the generator seam (somrando_api.h) */
/* True when the build advertises the generator AND it reproduces the menu's current configuration byte for byte (so the menu's GO works for
 * supported presets only; Java needs no change: it already asks this before generating). */
JNIEXPORT jboolean JNICALL FN(generatorAvailable)(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  return somrando_available() && somrando_menu_supports(&L, NULL, 0) ? JNI_TRUE : JNI_FALSE;
}
/* Why the current configuration is not supported ("" when it is). */
JNIEXPORT jstring JNICALL FN(generatorWhy)(JNIEnv *e, jclass c) {
  (void)c;
  char why[256] = "";
  if (!somrando_available()) snprintf(why, sizeof why, "generator not advertised in this build");
  else somrando_menu_supports(&L, why, sizeof why);
  return jstr(e, why);
}
/* Tells the menu which presets and which current configuration the generator can make (labels "PC only", dim GO). */
JNIEXPORT void JNICALL FN(refreshSupport)(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  for (int p = 0; p < sl_npresets; p++) sl_set_preset_ok(&L, p, somrando_available() ? somrando_menu_preset_supported(p, NULL, 0) == 1 : 0);
}
JNIEXPORT void JNICALL FN(refreshConfigSupport)(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  char why[256] = "";
  int ok = somrando_available() && somrando_menu_supports(&L, why, sizeof why);
  if (!somrando_available()) snprintf(why, sizeof why, "generator not advertised in this build");
  sl_set_gen_support(&L, ok ? 1 : 0, ok ? "" : why);
}
/* Preset index in sl_presets: 1 supported, 0 not, -1 bad index. */
JNIEXPORT jint JNICALL FN(presetSupported)(JNIEnv *e, jclass c, jint preset) {
  (void)e; (void)c;
  return somrando_available() ? somrando_menu_preset_supported((int)preset, NULL, 0) : 0;
}
JNIEXPORT jstring JNICALL FN(generatorVersion)(JNIEnv *e, jclass c) { (void)c; return jstr(e, somrando_version()); }

struct cbctx { JNIEnv *env; jobject cb; jmethodID mid; };
static int on_progress(void *user, int step, int total, const char *name) {
  struct cbctx *x = (struct cbctx *)user;
  jstring n = (*x->env)->NewStringUTF(x->env, name ? name : "");
  jboolean cancel = (*x->env)->CallBooleanMethod(x->env, x->cb, x->mid, (jint)step, (jint)total, n);
  (*x->env)->DeleteLocalRef(x->env, n);
  return cancel ? 1 : 0;
}

/* Blocking: call from a worker thread.  cb: LauncherNative.Progress.  Returns a SOMRANDO_* code; err gets the message. */
JNIEXPORT jint JNICALL FN(nativeGenerate)(JNIEnv *e, jclass c, jstring mode, jstring seed, jstring diff, jstring lines, jstring src, jstring dst,
                                          jstring work, jobject cb, jobjectArray errOut) {
  (void)c;
  const char *m = (*e)->GetStringUTFChars(e, mode, NULL), *s = (*e)->GetStringUTFChars(e, seed, NULL),
             *d = diff ? (*e)->GetStringUTFChars(e, diff, NULL) : NULL, *o = (*e)->GetStringUTFChars(e, lines, NULL),
             *a = (*e)->GetStringUTFChars(e, src, NULL), *b = (*e)->GetStringUTFChars(e, dst, NULL),
             *w = work ? (*e)->GetStringUTFChars(e, work, NULL) : NULL;
  struct somrando_request req = {m, s, d && d[0] ? d : NULL, o, a, b, w};
  struct cbctx x = {e, cb, 0};
  jclass k = (*e)->GetObjectClass(e, cb);
  x.mid = (*e)->GetMethodID(e, k, "onProgress", "(IILjava/lang/String;)Z");
  char err[256] = "";
  int rc = somrando_generate(&req, x.mid ? on_progress : NULL, &x, err, sizeof err);
  if (errOut && (*e)->GetArrayLength(e, errOut) > 0) {
    jstring js = jstr(e, err);
    (*e)->SetObjectArrayElement(e, errOut, 0, js);
  }
  (*e)->ReleaseStringUTFChars(e, mode, m); (*e)->ReleaseStringUTFChars(e, seed, s);
  if (d) (*e)->ReleaseStringUTFChars(e, diff, d);
  (*e)->ReleaseStringUTFChars(e, lines, o); (*e)->ReleaseStringUTFChars(e, src, a); (*e)->ReleaseStringUTFChars(e, dst, b);
  if (w) (*e)->ReleaseStringUTFChars(e, work, w);
  return rc;
}

/* ---- gameplay tweaks and credits pages (som_start_pages.c) ---- */
JNIEXPORT jint JNICALL FN(pagesInit)(JNIEnv *e, jclass c, jstring menu, jstring values, jstring credits) {
  const char *m = utf(e, menu), *v = values ? utf(e, values) : NULL, *cr = credits ? utf(e, credits) : NULL;
  int n = ssp_gameplay_load(m[0] ? m : NULL, v && v[0] ? v : NULL);
  (void)c;
  if (cr && cr[0]) ssp_set_credits_json(cr);
  rel(e, menu, m); if (values) rel(e, values, v); if (credits) rel(e, credits, cr);
  return n;
}
JNIEXPORT jint JNICALL FN(pageActive)(JNIEnv *e, jclass c) { (void)e; (void)c; return ssp_page(); }
JNIEXPORT jboolean JNICALL FN(gameplayDirty)(JNIEnv *e, jclass c) { (void)e; (void)c; return ssp_dirty(1) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL FN(gameplayIsDefault)(JNIEnv *e, jclass c) { (void)e; (void)c; return ssp_gameplay_default() ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jstring JNICALL FN(gameplayJson)(JNIEnv *e, jclass c) { (void)c; char b[4096]; return jstr(e, ssp_gameplay_json(b, sizeof b) < 0 ? "" : b); }
/* Flattened credits text of data/credits.json (the in-game page takes the same text). */
JNIEXPORT jstring JNICALL FN(creditsText)(JNIEnv *e, jclass c, jstring json) {
  const char *j = utf(e, json);
  static char t[8192];
  (void)c;
  if (ssp_credits_text(j, t, sizeof t) < 0) t[0] = 0;
  rel(e, json, j);
  return jstr(e, t);
}

/* Applies a gameplay options file to a ROM file (som_gameplay_apply.c).  Returns "" and fills the skipped list ("a,b" or "") in skippedOut[0]; else the error. */
JNIEXPORT jstring JNICALL FN(gameplayApply)(JNIEnv *e, jclass c, jstring in, jstring optionsJson, jstring out, jobjectArray skippedOut) {
  const char *pi = utf(e, in), *js = utf(e, optionsJson), *po = utf(e, out);
  char err[256] = "", skipped[256] = "", tmp[1100];
  struct sga_opts o;
  FILE *f;
  long n;
  uint8_t *rom = NULL;
  (void)c;
  f = fopen(pi, "rb");
  if (!f) snprintf(err, sizeof err, "cannot read the ROM");
  else {
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    rom = malloc((size_t)n);
    if (!rom || fread(rom, 1, (size_t)n, f) != (size_t)n) snprintf(err, sizeof err, "cannot read the ROM");
    fclose(f);
    if (!err[0] && sga_parse(&o, js, err, sizeof err) == 0 && sga_apply(rom, (size_t)n, &o, err, sizeof err, skipped, sizeof skipped) >= 0) {
      snprintf(tmp, sizeof tmp, "%s.part", po);
      f = fopen(tmp, "wb");
      if (!f || fwrite(rom, 1, (size_t)n, f) != (size_t)n) snprintf(err, sizeof err, "cannot write the patched ROM");
      if (f) fclose(f);
      if (!err[0] && rename(tmp, po) != 0) snprintf(err, sizeof err, "cannot write the patched ROM");
    }
  }
  free(rom);
  if (skippedOut && (*e)->GetArrayLength(e, skippedOut) > 0) (*e)->SetObjectArrayElement(e, skippedOut, 0, jstr(e, skipped));
  rel(e, in, pi); rel(e, optionsJson, js); rel(e, out, po);
  return jstr(e, err);
}
