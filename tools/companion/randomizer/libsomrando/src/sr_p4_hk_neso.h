/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49.  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Layout of the nesoberi images generated at build time by tools/gen_p4_hmisc_resources.py (the images are third-party fan art and are not
 * stored in the repository).  Each image is the 16x24 pixel area read by Nesoberi.cs through Bitmap.GetPixel, as R,G,B bytes row by row.
 */
#ifndef SR_P4_HK_NESO_H
#define SR_P4_HK_NESO_H
#include <stdint.h>

#define SR_P4_NESO_IMG_BYTES (16 * 24 * 3)

struct sr_p4_neso_char {
  const char *name;
  int month, day;
  int first_outfit, n_outfits;
};

struct sr_p4_neso_data {
  int n_chars;
  const struct sr_p4_neso_char *chars;
  int n_outfits;
  const uint16_t *outfit_image;                 /* outfit index -> image index */
  const uint8_t (*images)[SR_P4_NESO_IMG_BYTES];
  const uint8_t *snow;                          /* snow_neso.bmp */
};

/* defined by the generated file (tools/gen_p4_hmisc_resources.py) */
extern const struct sr_p4_neso_data sr_p4_neso_data;

struct sr_ctx;
int sr_p4_hmisc_neso_tileset(struct sr_ctx *c, uint8_t *buf);
int sr_p4_hmisc_neso_buf_size(void);
int sr_p4_hmisc_neso_available(void);
#endif
