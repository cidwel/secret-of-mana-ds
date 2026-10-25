/* libsomrando: Ancient Cave Manafort interior floors, internal interface between sr_p3_manafort.c and sr_p3_manafort_rooms.c.  LGPL-2.1 or later. */
#ifndef SR_P3_MANAFORT_H
#define SR_P3_MANAFORT_H
#include "sr_p3_ac.h"

enum { SR_P3_MF_CRYSTAL, SR_P3_MF_BRIDGE, SR_P3_MF_DIAGONAL, SR_P3_MF_CIRCLELINE, SR_P3_MF_MULTIDIAGONAL, SR_P3_MF_START, SR_P3_MF_END };

/* ManafortRoomMaker.generateRoom(right, left, top, bottom, door, randomFunctional): room[y * 20 + x] */
void sr_p3_manafort_room(struct sr_ctx *c, int kind, uint8_t *room, int right, int left, int top, int bottom, int door);
#endif
