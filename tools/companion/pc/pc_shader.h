/* Retro display filters of the SoM PC frontend (board item T12).  The renderer is plain SDL2, so a filter is made of two parts that work on
 * the software renderer as well as on OpenGL / Direct3D / Metal:
 *   - the picture is scaled sharp-bilinear: nearest to an integer multiple of the frame, then linear to the final size (PC_SH_PLAIN keeps a
 *     simple bilinear scale);
 *   - SCANLINES, CRT and LCD add a multiply layer (SDL_BLENDMODE_MOD) the size of the picture, made here by pc_shader_overlay().
 * Everything here is pure arithmetic (no libm), so the layers are byte-identical on every machine and the tests hash them.  Pure C99. */
#ifndef PC_SHADER_H
#define PC_SHADER_H

#include <stdint.h>

enum pc_shader { PC_SH_PLAIN = 0, PC_SH_SHARP, PC_SH_SCANLINES, PC_SH_CRT, PC_SH_LCD, PC_SH_COUNT };

const char *pc_shader_name(int mode);                 /* "PLAIN", "SHARP BILINEAR", "SCANLINES", "CRT LITE", "LCD GRID" */
const char *pc_shader_key(int mode);                  /* "plain", "sharp", "scanlines", "crt", "lcd" (command line, config) */
int pc_shader_from_key(const char *key);              /* -1 when unknown */
int pc_shader_valid(int mode);
int pc_shader_next(int mode, int dir);                /* wraps */
int pc_shader_has_overlay(int mode);                  /* 1 for SCANLINES, CRT, LCD */
int pc_shader_is_sharp(int mode);                     /* 1 when the picture is pre-scaled to an integer multiple first (every mode but PLAIN) */

/* Integer pre-scale for a frame src_w wide shown dst_w wide: the smallest k with k * src_w >= dst_w, from 1 to 8. */
int pc_shader_prescale(int src_w, int dst_w);

/* Fills out[dw * dh] with the multiply layer for a picture of dw x dh showing a src_w x src_h frame (0xFFRRGGBB, alpha always 255).  The
 * layer follows the source pixel grid, so it is what a CRT or an LCD would do to that frame.  Returns 0, or -1 when the mode has no layer
 * or a size is not positive. */
int pc_shader_overlay(int mode, int dw, int dh, int src_w, int src_h, uint32_t *out);

#endif
