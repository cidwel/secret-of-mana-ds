/* libsomrando: Open World randomization model (phase 4).  LGPL-2.1 or later.
 *
 * Derived from processing/openworld/randomization/{PrizeItem,PrizeLocation,OpenWorldSimulator}.cs (Author: Moppleton).
 * INITIAL CONTRACT written by the coordinator so the core (sr_p4_ow_core*.c), the injector/hints (sr_p4_ow_inject*.c, sr_p4_ow_hints*.c) and the
 * event modules can be written in parallel.  The owner of the core may extend it; any change of an existing field or prototype must be reported
 * to the coordinator.  Strings are NUL terminated and live in the generation arena (sr_alloc). */
#ifndef SR_P4_OW_H
#define SR_P4_OW_H
#include "sr_core.h"

struct ow_prize {            /* PrizeItem */
  const char *name, *type, *hint_name;
  uint8_t *event_data;       /* byte[] eventData */
  int event_len;
  int got_item_event_flag;   /* byte gotItemEventFlag */
  double value;
  int uid;                   /* PRIZE_UID order of construction (Equals/GetHashCode) */
};

struct ow_location {         /* PrizeLocation */
  const char *name;
  int map_num, obj_num;      /* -1 when not a chest */
  int event_num, event_replacement_index;
  const char **prize_type_options; int n_prize_type_options;
  const char **location_hints;     int n_location_hints;
  const char **locked_by_prizes;   int n_locked_by_prizes;
  double reachability;
};

/* Dictionary<PrizeLocation, PrizeItem> in .NET enumeration order */
struct ow_placement { struct ow_location *loc; struct ow_prize *prize; };
struct ow_placements { struct ow_placement *v; int n; };

struct ow_sim_result {       /* OpenWorldSimulator.SimResult */
  double score;
  int n_cycles;
  int *cycle_len;
  const char ***cycles;      /* collectionCycles[i][j] */
};

/* OpenWorldResultInjector.injectRandomization (sr_p4_ow_inject.c); gift goal runs GiftModeProcessing.makeGiftModeEvents */
void sr_p4_ow_inject(struct sr_ctx *c, const struct ow_placements *p);
/* OpenWorldHints.addHints (sr_p4_ow_hints.c) */
void sr_p4_ow_add_hints(struct sr_ctx *c, const struct ow_placements *p, const struct ow_sim_result *r);
/* OpenWorldSpoilers.logOpenWorldSpoilers: logging only; a C no-op unless the C# can throw there (then reproduce the failure) */
void sr_p4_ow_log_spoilers(struct sr_ctx *c, const struct ow_placements *p, const struct ow_sim_result *r);
#endif
