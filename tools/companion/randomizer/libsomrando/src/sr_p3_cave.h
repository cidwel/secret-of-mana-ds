/* libsomrando: internal interface of the Ancient Cave "cave" floor generator.  LGPL-2.1 or later, see sr_core.h and COPYING.LIB. */
#ifndef SR_P3_CAVE_H
#define SR_P3_CAVE_H

#include "sr_p3_ac.h"

/* CaveTileEdges.addTileEdges; start_node may be NULL (C# null) */
void sr_p3_cave_add_tile_edges(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2, const struct ac_node *start_node);

#endif
