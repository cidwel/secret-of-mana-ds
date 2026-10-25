/* libsomrando: internal interface of the ruins floor generator (sr_p3_ruins*.c).  LGPL-2.1 or later.
 * Derived from processing/ancientcave/mapgen/ruins/ (*.cs) (Author: Moppleton). */
#ifndef SR_P3_RUINS_INT_H
#define SR_P3_RUINS_INT_H
#include "sr_p3_ac.h"

#define RUINS_NUM_NODES_X 4
#define RUINS_NUM_NODES_Y 7
#define RUINS_NODE_WIDTH 20
#define RUINS_NODE_HEIGHT_OUTDOOR 9
#define RUINS_NODE_HEIGHT_INDOOR 15
#define RUINS_ROOF_NODE_NAME "roof"

/* "<prefix>_<a>_<b>" node name, heap allocated */
char *sr_p3_ruins_node_name(struct sr_ctx *c, char prefix, int a, int b);
/* allNodes["<prefix>_<a>_<b>"]: fails like the KeyNotFoundException */
struct ac_node *sr_p3_ruins_node_get(struct sr_ctx *c, const struct ac_dict *all, char prefix, int a, int b);
/* allNodes.ContainsKey + indexer: NULL when absent */
struct ac_node *sr_p3_ruins_node_find(struct sr_ctx *c, const struct ac_dict *all, char prefix, int a, int b);
int sr_p3_ruins_val(struct sr_ctx *c, const struct ac_node *n, const char *key);

void sr_p3_ruins_make_paths(struct sr_ctx *c, struct ac_gen_ctx *g);
void sr_p3_ruins_make_courtyard(struct sr_ctx *c, struct ac_layer *l1, struct ac_layer *l2, struct ac_map *outside);
void sr_p3_ruins_outside_rooms(struct sr_ctx *c, struct ac_layer *l1, struct ac_layer *l2, struct ac_map *outside, const struct ac_gen_ctx *g);
void sr_p3_ruins_inside_rooms(struct sr_ctx *c, struct ac_layer *l1, struct ac_layer *l2, struct ac_map *inside, const struct ac_gen_ctx *g);
#endif
