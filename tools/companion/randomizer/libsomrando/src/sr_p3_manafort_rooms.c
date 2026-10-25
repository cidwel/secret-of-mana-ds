/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave, Manafort interior floors: the 20x20 room makers.  Derived from (Author of every file: Moppleton)
 * processing/ancientcave/mapgen/manafortint/rooms/{ManafortRoomMaker,BridgeRoom,CircleLineRoom,CrystalRoom,DiagonalRoom,MultiDiagonalRoom,
 * StartEndRoom}.cs.  WhipRoom has weight 0 in the generator and is never chosen, so it is not ported.  Translated to C 2026-10-18.
 */
#include <stdint.h>
#include <string.h>

#include "sr_p3_manafort.h"

#include "sr_p3_manafort_rooms_data.h"

struct inj {
  const uint8_t *d;
  int len, x, y, w;
};
#define INJ(arr, x, y, w) {(arr), (int)sizeof(arr), (x), (y), (w)}
#define NOINJ {NULL, 0, 0, 0, 1}

struct room_def {
  const uint8_t *base;
  struct inj right[2], left[2], bottom[2], top[2], door;
};

/* ManafortRoomMaker.injectData: the C# array has 400 elements, a write outside it throws */
static void inject(struct sr_ctx *c, uint8_t *room, const struct inj *j) {
  if (!j->d) return;
  for (int yp = 0; yp < j->len / j->w; yp++)
    for (int xp = 0; xp < j->w; xp++) {
      int dst = (yp + j->y) * 20 + (xp + j->x);
      if (dst < 0 || dst >= 400) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
      room[dst] = j->d[yp * j->w + xp];
    }
}

#define SIMPLE(R, rx, ry, rw, bx, by, bw, tx, ty, tw)                                                                                       \
  {mf_##R##_baseRoom, {INJ(mf_##R##_rightPassage, rx, ry, rw), NOINJ}, {INJ(mf_##R##_leftPassage, 0, ry, rw), NOINJ},                       \
   {INJ(mf_##R##_bottomPassage, bx, by, bw), NOINJ}, {INJ(mf_##R##_topPassage, tx, ty, tw), NOINJ}, INJ(mf_##R##_doorBlock, 9, 2, 2)}

static const struct room_def def_bridge = {
    mf_BridgeRoom_baseRoom,
    {INJ(mf_BridgeRoom_rightPassage, 17, 0, 3), INJ(mf_BridgeRoom_rightBridge, 13, 12, 7)},
    {INJ(mf_BridgeRoom_leftPassage, 0, 0, 3), INJ(mf_BridgeRoom_leftBridge, 0, 12, 7)},
    {INJ(mf_BridgeRoom_bottomPassage, 3, 17, 14), INJ(mf_BridgeRoom_bottomBridge, 9, 16, 2)},
    {INJ(mf_BridgeRoom_topPassage, 3, 0, 14), INJ(mf_BridgeRoom_topBridge, 9, 8, 2)},
    INJ(mf_BridgeRoom_doorBlock, 9, 2, 2)};
static const struct room_def def_circle = SIMPLE(CircleLineRoom, 18, 3, 2, 8, 17, 4, 9, 0, 2);
static const struct room_def def_crystal = SIMPLE(CrystalRoom, 17, 0, 3, 3, 17, 14, 3, 0, 14);
static const struct room_def def_diagonal = SIMPLE(DiagonalRoom, 18, 3, 2, 8, 17, 4, 9, 0, 2);
static const struct room_def def_multi = SIMPLE(MultiDiagonalRoom, 18, 3, 2, 8, 17, 4, 9, 0, 2);
static const struct room_def def_startend = SIMPLE(StartEndRoom, 17, 0, 3, 3, 17, 14, 3, 0, 14);

static void build(struct sr_ctx *c, const struct room_def *d, uint8_t *room, int right, int left, int top, int bottom, int door) {
  memcpy(room, d->base, 400);
  if (right) { inject(c, room, &d->right[0]); inject(c, room, &d->right[1]); }
  if (left) { inject(c, room, &d->left[0]); inject(c, room, &d->left[1]); }
  if (bottom) { inject(c, room, &d->bottom[0]); inject(c, room, &d->bottom[1]); }
  if (top) {
    inject(c, room, &d->top[0]);
    inject(c, room, &d->top[1]);
  } else if (door) {
    inject(c, room, &d->top[0]);
    inject(c, room, &d->top[1]);
    inject(c, room, &d->door);
  }
}

static double noise40(int xp, int yp, int z) { return ac_perlin_noise(xp * 40 / (double)128, yp * 40 / (double)128, z); }
static double noise20(int xp, int yp, int z) { return ac_perlin_noise(xp * 20 / (double)128, yp * 20 / (double)128, z); }

static void plain_room(struct sr_ctx *c, const struct room_def *d, uint8_t *room, int right, int left, int top, int bottom, int door) {
  build(c, d, room, right, left, top, bottom, door);
}

static void crystal_room(struct sr_ctx *c, uint8_t *room, int right, int left, int top, int bottom, int door) {
  build(c, &def_crystal, room, right, left, top, bottom, door);
  int z = AC_RND(c, 2000);   /* perlinZ */
  for (int yp = 0; yp < 20; yp++)
    for (int xp = 0; xp < 20; xp++)
      if (room[yp * 20 + xp] == 122) {
        if (noise40(xp, yp, z) > 0.3) room[yp * 20 + xp] = 125;
        else if (noise40(xp, yp, z) > 0.0) room[yp * 20 + xp] = 124;
        else if (noise40(xp, yp, z) > -.3) room[yp * 20 + xp] = 123;
        if (noise20(xp, yp, z) > 0.1) room[yp * 20 + xp] = 48;
      }
}

static void startend_room(struct sr_ctx *c, uint8_t *room, int is_end, int right, int left, int top, int bottom, int door) {
  build(c, &def_startend, room, right, left, top, bottom, door);
  int z = AC_RND(c, 2000);
  for (int yp = 1; yp < 19; yp++)
    for (int xp = 1; xp < 19; xp++)
      if ((xp < 9 || xp > 11) && (yp < 9 || yp > 11) && room[yp * 20 + xp] == 48) {
        if (noise40(xp, yp, z) > 0.3) {
          room[yp * 20 + xp] = 122;
        } else if (noise20(xp, yp, z) > 0 && room[yp * 20 + xp + 1] == 48 && room[(yp + 1) * 20 + xp + 1] == 48 && room[(yp + 1) * 20 + xp] == 48) {
          room[yp * 20 + xp] = 1;
          room[yp * 20 + xp + 1] = 2;
          room[(yp + 1) * 20 + xp] = 17;
          room[(yp + 1) * 20 + xp + 1] = 18;
        }
      }
  if (is_end) {
    int ex = 10, ey = 12;
    room[ey * 20 + ex] = 54;   /* warper */
    room[ey * 20 + (ex + 1)] = 122;
    room[ey * 20 + (ex - 1)] = 122;
    room[(ey - 1) * 20 + ex] = 122;
    room[(ey + 1) * 20 + ex] = 122;
    room[ey * 20 + (ex + 2)] = 124;
    room[ey * 20 + (ex - 2)] = 124;
    room[(ey - 2) * 20 + ex] = 124;
    room[(ey + 2) * 20 + ex] = 124;
    room[(ey - 2) * 20 + (ex - 2)] = 5;
    room[(ey - 2) * 20 + (ex - 1)] = 6;
    room[(ey - 1) * 20 + (ex - 2)] = 21;
    room[(ey - 1) * 20 + (ex - 1)] = 22;
    room[(ey - 2) * 20 + (ex + 1)] = 5;
    room[(ey - 2) * 20 + (ex + 2)] = 6;
    room[(ey - 1) * 20 + (ex + 1)] = 21;
    room[(ey - 1) * 20 + (ex + 2)] = 22;
    room[(ey + 1) * 20 + (ex - 2)] = 3;
    room[(ey + 1) * 20 + (ex - 1)] = 4;
    room[(ey + 2) * 20 + (ex - 2)] = 19;
    room[(ey + 2) * 20 + (ex - 1)] = 20;
    room[(ey + 1) * 20 + (ex + 1)] = 3;
    room[(ey + 1) * 20 + (ex + 2)] = 4;
    room[(ey + 2) * 20 + (ex + 1)] = 19;
    room[(ey + 2) * 20 + (ex + 2)] = 20;
  }
}

/* ManafortRoomMaker.generateRoom: fills room[y * 20 + x] */
void sr_p3_manafort_room(struct sr_ctx *c, int kind, uint8_t *room, int right, int left, int top, int bottom, int door) {
  switch (kind) {
    case SR_P3_MF_CRYSTAL: crystal_room(c, room, right, left, top, bottom, door); break;
    case SR_P3_MF_BRIDGE: plain_room(c, &def_bridge, room, right, left, top, bottom, door); break;
    case SR_P3_MF_DIAGONAL: plain_room(c, &def_diagonal, room, right, left, top, bottom, door); break;
    case SR_P3_MF_CIRCLELINE: plain_room(c, &def_circle, room, right, left, top, bottom, door); break;
    case SR_P3_MF_MULTIDIAGONAL: plain_room(c, &def_multi, room, right, left, top, bottom, door); break;
    case SR_P3_MF_START: startend_room(c, room, 0, right, left, top, bottom, door); break;
    default: startend_room(c, room, 1, right, left, top, bottom, door); break;
  }
}
