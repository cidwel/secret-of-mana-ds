/* phase 2 hacks (see sr_p2_*.c) */
#ifndef SR_P2_H
#define SR_P2_H
struct sr_ctx;
int sr_hack_weapon_randomizer(struct sr_ctx *c);
int sr_hack_magic_rebalance(struct sr_ctx *c);
int sr_hack_elemental_damage(struct sr_ctx *c);
int sr_hack_rabite_color(struct sr_ctx *c);
int sr_hack_character_palette(struct sr_ctx *c);
int sr_hack_status_glow(struct sr_ctx *c);
int sr_hack_free_walking(struct sr_ctx *c);
int sr_hack_speedup(struct sr_ctx *c);
int sr_hack_free_targetting(struct sr_ctx *c);
int sr_hack_buy_multiple(struct sr_ctx *c);
int sr_hack_minimap(struct sr_ctx *c);
int sr_hack_autosave(struct sr_ctx *c);
int sr_hack_custom_music(struct sr_ctx *c);
int sr_hack_faster_dialogue_events(struct sr_ctx *c);
int sr_hack_boss_swaps(struct sr_ctx *c);
int sr_hack_enemy_swaps(struct sr_ctx *c);
int sr_hack_enemy_scaling(struct sr_ctx *c);
int sr_hack_enemy_position_adjust(struct sr_ctx *c);
int sr_hack_faster_enemies(struct sr_ctx *c);
int sr_w3_initial_values_hack(struct sr_ctx *c);
#endif
