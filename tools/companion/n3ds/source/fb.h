/* 3DS framebuffer helpers: screens are column-major BGR8, 240 pixels per column, origin at the bottom-left. */
#ifndef SOM_N3DS_FB_H
#define SOM_N3DS_FB_H
#include <stdint.h>

static inline void fb_put(uint8_t *fb, int x, int y, uint32_t rgb) {
  uint8_t *p = fb + (x * 240 + (239 - y)) * 3;
  p[0] = rgb & 0xFF; p[1] = (rgb >> 8) & 0xFF; p[2] = (rgb >> 16) & 0xFF;
}

/* px: w x 240 pixels as 0x??RRGGBB, row-major, stride w. */
static inline void fb_blit(uint8_t *fb, const uint32_t *px, int w) {
  for (int x = 0; x < w; x++) {
    uint8_t *col = fb + (size_t)x * 240 * 3 + 239 * 3;
    for (int y = 0; y < 240; y++, col -= 3) {
      uint32_t rgb = px[y * w + x];
      col[0] = (uint8_t)rgb; col[1] = (uint8_t)(rgb >> 8); col[2] = (uint8_t)(rgb >> 16);
    }
  }
}
#endif
