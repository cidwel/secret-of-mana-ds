/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave "ruins" floors: the fixed courtyard at the bottom of the outdoor map.  Derived from
 * processing/ancientcave/mapgen/ruins/RuinsCourtyard.cs (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include "sr_p3_ruins_data.h"
#include "sr_p3_ruins_int.h"

#define D(n) sr_p3_ruins_court_##n
#define L1(x, y) AC_AT(c, l1, (x), (y))
#define L2(x, y) AC_AT(c, l2, (x), (y))

void sr_p3_ruins_make_courtyard(struct sr_ctx *c, struct ac_layer *l1, struct ac_layer *l2, struct ac_map *outside) {
  const uint8_t layer1dirt = 27;
  const int wall_len = 8;
  const int map_width = l1->w;
  const int map_height = l1->h;
  const int wall_y = map_height - 15 - wall_len;

  const int middle = map_width / 2;
  const int left_bound = middle - 4;
  const int right_bound = middle + 3;
  const int left_court = left_bound - 12;
  const int right_court = right_bound + 12;
  for (int y = 0; y < wall_len; y++)
    for (int x = 0; x < map_width; x++)
      if (x < left_bound || x > right_bound) L1(x, wall_y + y) = (x % 2) == 0 ? D(l1LargeWall1)[y] : D(l1LargeWall2)[y];

  for (int y = 0; y < 3; y++)
    for (int x = 0; x < map_width; x++)
      if (x < left_bound || x > right_bound) {
        if (x > left_court && x < right_court) L2(x, wall_y + y - 2) = D(layer2WallTop)[y];
        else if (x == left_court) L2(x, wall_y + y - 2) = D(layer2WallTopAlt1Left)[y];
        else if (x == right_court) L2(x, wall_y + y - 2) = D(layer2WallTopAlt1Right)[y];
        else L2(x, wall_y + y - 2) = D(layer2WallTopAlt2)[y];
      }

  for (int y = 0; y < 4; y++) {
    L1(left_bound, wall_y + 3 + y) = 12;
    L1(right_bound, wall_y + 3 + y) = 12;
  }
  for (int y = 0; y < 5; y++) {
    L2(left_bound, wall_y + y - 2) = D(l2DoubleFencePillar)[y];
    L2(right_bound, wall_y + y - 2) = D(l2DoubleFencePillar)[y];
  }
  for (int y = map_height - 35; y < map_height - 10; y++)
    for (int x = left_bound + 1; x <= right_bound - 1; x++) L1(x, y) = 168;

  for (int sy = 0; sy < 5; sy++)
    for (int sx = 0; sx < 4; sx++) {
      L2(left_bound - 5 + sx, map_height - 18 + sy) = D(layer2Statue)[sy * 4 + sx];
      if (L1(left_bound - 5 + sx, map_height - 18 + sy) == layer1dirt && L2(left_bound - 5 + sx, map_height - 18 + sy) != 0)
        L1(left_bound - 5 + sx, map_height - 18 + sy) = 189;
      L2(right_bound + 2 + sx, map_height - 18 + sy) = D(layer2Statue)[sy * 4 + sx];
      if (L1(right_bound + 2 + sx, map_height - 18 + sy) == layer1dirt && L2(right_bound + 2 + sx, map_height - 18 + sy) != 0)
        L1(right_bound + 2 + sx, map_height - 18 + sy) = 189;
    }

  for (int sy = 0; sy < 8; sy++)
    for (int sx = 0; sx < 2; sx++) {
      L2(left_bound - 8 + sx, map_height - 18 + sy) = D(layer2GateFence)[sy * 2 + sx];
      if (L1(left_bound - 8 + sx, map_height - 18 + sy) == layer1dirt) L1(left_bound - 8 + sx, map_height - 18 + sy) = 189;
      L2(right_bound + 7 + sx, map_height - 18 + sy) = D(layer2GateFence)[sy * 2 + sx];
      if (L1(right_bound + 7 + sx, map_height - 18 + sy) == layer1dirt) L1(right_bound + 7 + sx, map_height - 18 + sy) = 189;
    }

  for (int y = map_height - 50; y <= map_height - 37; y++)
    for (int x = left_court; x <= right_court; x++) L1(x, y) = y < map_height - 44 ? 170 : 171;

  for (int y = 0; y < 3; y++)
    for (int x = left_court; x < right_court; x += 2) {
      L1(x, map_height - 35 + y - 4) = D(l1PorchFrontWall)[(x % 2) + y * 2];
      L1(x + 1, map_height - 35 + y - 4) = D(l1PorchFrontWall)[(x % 2) + 1 + y * 2];
    }

  for (int y = map_height - 52; y <= map_height - 24; y++) {
    L1(left_court, y) = 149;
    L1(right_court, y) = 95;
    for (int x = 1; x <= 8; x++) {
      if (y == map_height - 25) {
        L1(left_court - x, y) = 1;
        L1(right_court + x, y) = 1;
      } else {
        L1(left_court - x, y) = 162;
        L1(right_court + x, y) = 162;
      }
    }
  }
  L1(left_court, map_height - 25) = 1;
  L1(right_court, map_height - 25) = 1;
  L1(left_court, map_height - 24) = 162;
  L1(right_court, map_height - 24) = 162;

  const int steps_width = 6, steps_repeat = 2;
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < steps_width; x++) {
      int sx = x;
      if (x > steps_repeat) {
        sx -= steps_repeat;
        if (sx < steps_repeat) sx = steps_repeat;
      }
      L1(left_bound + 1 + x, y + map_height - 40) = D(l1steps)[y * 4 + sx];
    }

  const int door_y = map_height - 47;
  for (int x = map_width / 2 - 1; x >= left_court + 2; x -= 3) {
    for (int y = 0; y < 5; y++) {
      L2(x, map_height - 48 + y) = D(l2ArchLeft)[y];
      L2(x - 1, map_height - 48 + y) = D(l2ArchMiddle)[y];
      L2(x - 2, map_height - 48 + y) = D(l2ArchRight)[y];
    }
    L1(x - 1, map_height - 44) = 188;
    if (x == map_width / 2 - 1) {
      L1(x, door_y) = D(l1DoorLeft)[0];
      L1(x, door_y + 1) = D(l1DoorLeft)[1];
      ac_dict_set(c, &outside->alt_exit, AC_XYKEY(x, door_y + 1), -1);
    } else {
      L1(x, door_y) = D(l1NonDoorLeft)[0];
      L1(x, door_y + 1) = D(l1NonDoorLeft)[1];
    }
    L1(x - 1, door_y) = 188;
    L1(x - 1, door_y + 1) = 188;
    L1(x - 2, door_y) = D(l1NonDoorRight)[0];
    L1(x - 2, door_y + 1) = D(l1NonDoorRight)[1];
  }

  for (int x = map_width / 2; x <= right_court - 2; x += 3) {
    for (int y = 0; y < 5; y++) {
      L2(x, map_height - 48 + y) = D(l2ArchRight)[y];
      L2(x + 1, map_height - 48 + y) = D(l2ArchMiddle)[y];
      L2(x + 2, map_height - 48 + y) = D(l2ArchLeft)[y];
    }
    L1(x + 1, map_height - 44) = 188;
    if (x == map_width / 2) {
      L1(x, door_y) = D(l1DoorRight)[0];
      L1(x, door_y + 1) = D(l1DoorRight)[1];
      ac_dict_set(c, &outside->alt_entry, -1, AC_VAL(ac_xy_new(c, x, door_y + 2)));
      ac_dict_set(c, &outside->alt_exit, AC_XYKEY(x, door_y + 1), -1);
    } else {
      L1(x, door_y) = D(l1NonDoorRight)[0];
      L1(x, door_y + 1) = D(l1NonDoorRight)[1];
    }
    L1(x + 1, door_y) = 188;
    L1(x + 1, door_y + 1) = 188;
    L1(x + 2, door_y) = D(l1NonDoorLeft)[0];
    L1(x + 2, door_y + 1) = D(l1NonDoorLeft)[1];
  }

  for (int x = left_court + 1; x < right_court; x++) {
    L1(x, map_height - 50) = 87;
    L1(x, map_height - 49) = 97;
  }
  L1(map_width / 2 - 2, map_height - 50) = 75;
  L1(map_width / 2 - 2, map_height - 49) = 91;
  L1(map_width / 2 + 1, map_height - 50) = 75;
  L1(map_width / 2 + 1, map_height - 49) = 91;
}
