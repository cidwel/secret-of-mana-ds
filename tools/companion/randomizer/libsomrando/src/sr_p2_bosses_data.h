/* Generated from VanillaBossMaps.cs (author Moppleton) by a throw-away script: the per-map data BossSwaps reads. */
#define BM_MANTISANT 0
#define BM_WALLFACE 1
#define BM_TROPICALLO 2
#define BM_MINOTAUR 3
#define BM_SPIKEY 4
#define BM_JABBER 5
#define BM_SPRINGBEAK 6
#define BM_FROSTGIGAS 7
#define BM_SNAPDRAGON 8
#define BM_MECHRIDER1 9
#define BM_DOOMSWALL 10
#define BM_VAMPIRE 11
#define BM_METALMANTIS 12
#define BM_MECHRIDER2 13
#define BM_KILROY 14
#define BM_GORGON 15
#define BM_BOREAL 16
#define BM_GREATVIPER 17
#define BM_BLUESPIKE 18
#define BM_HYDRA 19
#define BM_AEGRTOJNFSDGHOPALNIAPLEIAN 20
#define BM_HEXAS 21
#define BM_KETTLEKIN 22
#define BM_TONPOLE 23
#define BM_MECHRIDER3 24
#define BM_SNOWDRAGON 25
#define BM_FIREGIGAS 26
#define BM_REDDRAGON 27
#define BM_AXEBEAK 28
#define BM_BLUEDRAGON 29
#define BM_BUFFY 30
#define BM_DARKLICH 31
#define BM_DRAGONWORM 32
#define BM_THUNDERGIGAS 33
/* Generated from VanillaBossMaps.cs by gen_boss.py: per-map data used by BossSwaps (swap mode). */
struct sr_w6_bmap {
  const char *name;
  int map_num, obj_num;
  int n_swap; const uint8_t *swap;
  int sp_c[2], sp_l[2], sp_r[2], beak_c[2];
  int n_jump; const int (*jump)[2];
  int n_pod; const int (*pod)[2];
  int n_trop; const int (*trop)[2];
  int n_bram; const int (*bram)[2];
  int wall_y;
};
static const uint8_t sw_MANTISANT[] = {89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_MANTISANT[][2] = {{176,176},{144,288},{272,160},{128,320},{144,192}};
static const int pod_MANTISANT[][2] = {{144,288},{272,160}};
static const int trop_MANTISANT[][2] = {{176,144},{144,272},{272,128}};
static const int bram_MANTISANT[][2] = {{96,208},{304,160}};
static const uint8_t sw_WALLFACE[] = {88,97,110,115,117,119,121};
static const int jump_WALLFACE[][2] = {{160,176},{272,176},{288,288},{192,240},{240,192},{208,160},{144,304}};
static const int pod_WALLFACE[][2] = {{144,240},{288,224}};
static const int trop_WALLFACE[][2] = {{224,192},{144,160},{288,256}};
static const int bram_WALLFACE[][2] = {{160,288},{272,208}};
static const uint8_t sw_TROPICALLO[] = {87,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_TROPICALLO[][2] = {{352,544},{272,640},{384,672},{480,640},{512,592}};
static const int pod_TROPICALLO[][2] = {{288,560},{464,544}};
static const int trop_TROPICALLO[][2] = {{352,576},{288,576},{448,544}};
static const int bram_TROPICALLO[][2] = {{336,544},{432,608}};
static const uint8_t sw_MINOTAUR[] = {87,89,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_MINOTAUR[][2] = {{208,208},{176,352},{304,256},{288,384},{224,288}};
static const int pod_MINOTAUR[][2] = {{160,256},{336,304}};
static const int trop_MINOTAUR[][2] = {{176,224},{144,272},{320,352}};
static const int bram_MINOTAUR[][2] = {{128,176},{368,400}};
static const uint8_t sw_SPIKEY[] = {87,89,90,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_SPIKEY[][2] = {{160,304},{240,240},{176,208},{224,352},{208,288}};
static const int pod_SPIKEY[][2] = {{112,208},{304,208}};
static const int trop_SPIKEY[][2] = {{192,224},{224,304},{304,208},{112,208}};
static const int bram_SPIKEY[][2] = {{184,160},{232,160}};
static const uint8_t sw_JABBER[] = {87,89,90,91,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_JABBER[][2] = {{336,576},{224,704},{192,576},{336,672},{256,576}};
static const int pod_JABBER[][2] = {{192,624},{336,688}};
static const int trop_JABBER[][2] = {{224,592},{272,608},{352,624}};
static const int bram_JABBER[][2] = {{240,544},{288,544}};
static const uint8_t sw_SPRINGBEAK[] = {87,89,90,91,92,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_SPRINGBEAK[][2] = {{144,144},{240,176},{320,160},{144,304},{240,288},{128,208},{336,224}};
static const int pod_SPRINGBEAK[][2] = {{128,208},{336,224}};
static const int trop_SPRINGBEAK[][2] = {{240,224},{128,208},{336,224}};
static const int bram_SPRINGBEAK[][2] = {{176,256},{272,288}};
static const uint8_t sw_FROSTGIGAS[] = {87,89,90,91,92,93,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_FROSTGIGAS[][2] = {{192,304},{112,288},{224,240},{144,176},{80,224},{144,256},{240,224}};
static const int pod_FROSTGIGAS[][2] = {{96,112},{208,112}};
static const int trop_FROSTGIGAS[][2] = {{160,176},{144,96},{128,256}};
static const int bram_FROSTGIGAS[][2] = {{64,192},{240,192}};
static const uint8_t sw_SNAPDRAGON[] = {87,89,90,91,92,93,94,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_SNAPDRAGON[][2] = {{416,368},{384,400},{336,448},{416,448},{336,384},{304,416},{464,416}};
static const int pod_SNAPDRAGON[][2] = {{336,384},{416,448}};
static const int trop_SNAPDRAGON[][2] = {{336,368},{416,400},{368,448}};
static const int bram_SNAPDRAGON[][2] = {{288,400},{464,400}};
static const uint8_t sw_MECHRIDER1[] = {87,89,90,91,93,94,95,99,100,101,102,104,105,107,111,112,113,114,116,118,123,125};
static const int jump_MECHRIDER1[][2] = {{224,304},{384,320},{288,272},{240,208},{416,224},{272,352},{336,352}};
static const int pod_MECHRIDER1[][2] = {{160,224},{448,256}};
static const int trop_MECHRIDER1[][2] = {{304,240},{160,224},{448,256}};
static const int bram_MECHRIDER1[][2] = {{352,336},{240,352}};
static const uint8_t sw_DOOMSWALL[] = {88,97};
static const int jump_DOOMSWALL[][2] = {{0,0}};
static const int pod_DOOMSWALL[][2] = {{0,0}};
static const int trop_DOOMSWALL[][2] = {{0,0}};
static const int bram_DOOMSWALL[][2] = {{0,0}};
static const uint8_t sw_VAMPIRE[] = {87,89,90,91,92,93,94,95,96,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_VAMPIRE[][2] = {{176,272},{256,272},{176,368},{256,368},{304,400},{128,400},{208,320}};
static const int pod_VAMPIRE[][2] = {{144,336},{288,336}};
static const int trop_VAMPIRE[][2] = {{224,320},{144,272},{288,368}};
static const int bram_VAMPIRE[][2] = {{160,224},{272,224}};
static const uint8_t sw_METALMANTIS[] = {87,89,90,91,93,94,95,96,98,100,101,102,104,105,107,111,112,113,114,116,118,120,123,125};
static const int jump_METALMANTIS[][2] = {{240,304},{160,240},{304,256},{288,224},{208,256},{176,208},{304,208}};
static const int pod_METALMANTIS[][2] = {{176,256},{304,256}};
static const int trop_METALMANTIS[][2] = {{192,192},{288,192},{240,304}};
static const int bram_METALMANTIS[][2] = {{208,160},{272,160}};
static const uint8_t sw_MECHRIDER2[] = {87,89,90,91,93,94,95,96,99,101,102,104,105,107,111,112,113,114,116,118,123,125};
static const int jump_MECHRIDER2[][2] = {{256,304},{560,336},{336,336},{512,304},{336,288},{448,288},{384,336}};
static const int pod_MECHRIDER2[][2] = {{224,336},{528,336}};
static const int trop_MECHRIDER2[][2] = {{224,304},{528,304},{384,304}};
static const int bram_MECHRIDER2[][2] = {{240,272},{512,272}};
static const uint8_t sw_KILROY[] = {87,89,90,91,93,94,95,96,99,100,102,104,105,107,111,112,113,114,116,118,123,125};
static const int jump_KILROY[][2] = {{288,352},{240,336},{368,368},{304,384},{320,400},{304,288},{368,304}};
static const int pod_KILROY[][2] = {{240,352},{368,352}};
static const int trop_KILROY[][2] = {{304,352},{240,288},{368,416}};
static const int bram_KILROY[][2] = {{368,288},{240,416}};
static const uint8_t sw_GORGON[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,104,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_GORGON[][2] = {{144,240},{192,400},{320,368},{288,320},{256,256}};
static const int pod_GORGON[][2] = {{128,304},{352,240}};
static const int trop_GORGON[][2] = {{128,304},{240,288},{352,240}};
static const int bram_GORGON[][2] = {{208,160},{256,160}};
static const uint8_t sw_BOREAL[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,105,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_BOREAL[][2] = {{224,368},{336,304},{160,288},{336,288},{272,352}};
static const int pod_BOREAL[][2] = {{336,336},{176,176}};
static const int trop_BOREAL[][2] = {{176,160},{320,160},{176,336},{336,336}};
static const int bram_BOREAL[][2] = {{184,256},{328,256}};
static const uint8_t sw_GREATVIPER[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,107,109,111,112,113,114,116,118,120,123,125};
static const int jump_GREATVIPER[][2] = {{272,336},{416,368},{288,256},{416,272},{208,320}};
static const int pod_GREATVIPER[][2] = {{224,240},{416,288}};
static const int trop_GREATVIPER[][2] = {{240,240},{272,352},{416,336},{368,224}};
static const int bram_GREATVIPER[][2] = {{272,208},{320,256}};
static const uint8_t sw_BLUESPIKE[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,109,111,112,113,114,116,118,120,123,125};
static const int jump_BLUESPIKE[][2] = {{144,240},{192,400},{320,368},{288,320},{256,256}};
static const int pod_BLUESPIKE[][2] = {{128,304},{352,240}};
static const int trop_BLUESPIKE[][2] = {{128,304},{240,288},{352,240}};
static const int bram_BLUESPIKE[][2] = {{208,160},{256,160}};
static const uint8_t sw_HYDRA[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,111,112,113,114,116,118,120,123,125};
static const int jump_HYDRA[][2] = {{160,400},{288,432},{320,384},{224,384},{176,352}};
static const int pod_HYDRA[][2] = {{144,272},{320,288}};
static const int trop_HYDRA[][2] = {{176,352},{240,416},{304,352}};
static const int bram_HYDRA[][2] = {{160,432},{320,432}};
static const uint8_t sw_AEGRTOJNFSDGHOPALNIAPLEIAN[] = {88,97,110,115,117,119,121};
static const int jump_AEGRTOJNFSDGHOPALNIAPLEIAN[][2] = {{144,144},{240,176},{272,160},{144,240},{240,224},{128,208},{288,224}};
static const int pod_AEGRTOJNFSDGHOPALNIAPLEIAN[][2] = {{160,128},{272,192}};
static const int trop_AEGRTOJNFSDGHOPALNIAPLEIAN[][2] = {{160,208},{272,96},{160,112}};
static const int bram_AEGRTOJNFSDGHOPALNIAPLEIAN[][2] = {{240,176},{208,144}};
static const uint8_t sw_HEXAS[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,112,113,114,116,118,120,123,125};
static const int jump_HEXAS[][2] = {{160,368},{160,272},{240,320},{320,368},{320,272}};
static const int pod_HEXAS[][2] = {{160,288},{320,352}};
static const int trop_HEXAS[][2] = {{176,256},{240,368},{304,256}};
static const int bram_HEXAS[][2] = {{192,368},{288,368}};
static const uint8_t sw_KETTLEKIN[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,113,114,116,118,120,123,125};
static const int jump_KETTLEKIN[][2] = {{96,224},{224,240},{128,192},{256,192},{112,160}};
static const int pod_KETTLEKIN[][2] = {{96,208},{240,176}};
static const int trop_KETTLEKIN[][2] = {{96,160},{160,192},{256,176}};
static const int bram_KETTLEKIN[][2] = {{128,144},{208,144}};
static const uint8_t sw_TONPOLE[] = {87,90,92,94,95,96,98,99,100,101,102,105,109,111,112,114,116,120,123,125};
static const int jump_TONPOLE[][2] = {{320,224},{256,192},{208,192},{368,160},{272,160}};
static const int pod_TONPOLE[][2] = {{224,208},{368,176}};
static const int trop_TONPOLE[][2] = {{288,144},{224,224},{320,224}};
static const int bram_TONPOLE[][2] = {{368,240},{208,160}};
static const uint8_t sw_MECHRIDER3[] = {87,89,90,91,93,94,95,96,99,100,101,102,104,105,107,111,112,113,116,118,123,125};
static const int jump_MECHRIDER3[][2] = {{192,384},{288,384},{384,384},{192,192},{288,192},{384,192},{288,288}};
static const int pod_MECHRIDER3[][2] = {{192,288},{384,288}};
static const int trop_MECHRIDER3[][2] = {{240,240},{320,240},{320,320},{240,320}};
static const int bram_MECHRIDER3[][2] = {{288,288},{320,272}};
static const uint8_t sw_SNOWDRAGON[] = {88,97,110,117,119,121};
static const int jump_SNOWDRAGON[][2] = {{224,176},{320,224},{288,176},{208,256},{208,144},{272,272},{272,256}};
static const int pod_SNOWDRAGON[][2] = {{224,176},{320,224}};
static const int trop_SNOWDRAGON[][2] = {{224,272},{224,176},{320,224}};
static const int bram_SNOWDRAGON[][2] = {{208,240},{320,288}};
static const uint8_t sw_FIREGIGAS[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,118,120,123,125};
static const int jump_FIREGIGAS[][2] = {{160,368},{160,272},{240,320},{320,368},{320,272}};
static const int pod_FIREGIGAS[][2] = {{160,288},{320,352}};
static const int trop_FIREGIGAS[][2] = {{176,256},{240,368},{304,256}};
static const int bram_FIREGIGAS[][2] = {{192,368},{288,368}};
static const uint8_t sw_REDDRAGON[] = {88,97,110,115,119,121};
static const int jump_REDDRAGON[][2] = {{224,176},{320,224},{288,176},{208,256},{208,144},{272,272},{272,256}};
static const int pod_REDDRAGON[][2] = {{224,176},{320,224}};
static const int trop_REDDRAGON[][2] = {{224,272},{224,176},{320,224}};
static const int bram_REDDRAGON[][2] = {{208,240},{320,288}};
static const uint8_t sw_AXEBEAK[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,120,123,125};
static const int jump_AXEBEAK[][2] = {{176,320},{480,320},{192,416},{208,384},{400,400},{432,416},{240,288},{368,288}};
static const int pod_AXEBEAK[][2] = {{232,288},{408,288}};
static const int trop_AXEBEAK[][2] = {{400,304},{368,432},{288,400}};
static const int bram_AXEBEAK[][2] = {{288,288},{352,288}};
static const uint8_t sw_BLUEDRAGON[] = {88,97,110,115,117,121};
static const int jump_BLUEDRAGON[][2] = {{144,144},{240,176},{272,160},{144,240},{240,224},{128,208},{288,224}};
static const int pod_BLUEDRAGON[][2] = {{160,128},{272,192}};
static const int trop_BLUEDRAGON[][2] = {{160,208},{272,256},{160,272}};
static const int bram_BLUEDRAGON[][2] = {{240,176},{208,144}};
static const uint8_t sw_BUFFY[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,123,125};
static const int jump_BUFFY[][2] = {{176,672},{320,624},{192,624},{368,704},{304,704},{256,608},{224,576}};
static const int pod_BUFFY[][2] = {{368,768},{160,768}};
static const int trop_BUFFY[][2] = {{224,640},{272,704},{336,624}};
static const int bram_BUFFY[][2] = {{240,560},{288,560}};
static const uint8_t sw_DARKLICH[] = {88,97,110,115,117,119};
static const int jump_DARKLICH[][2] = {{288,288},{224,224},{352,352},{224,352},{352,224},{288,352},{288,224}};
static const int pod_DARKLICH[][2] = {{224,256},{336,336}};
static const int trop_DARKLICH[][2] = {{224,256},{288,304},{352,352}};
static const int bram_DARKLICH[][2] = {{304,272},{240,320}};
static const uint8_t sw_DRAGONWORM[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,125};
static const int jump_DRAGONWORM[][2] = {{272,384},{192,336},{224,256},{256,224},{384,224}};
static const int pod_DRAGONWORM[][2] = {{224,272},{368,240}};
static const int trop_DRAGONWORM[][2] = {{336,192},{224,272},{256,208},{272,368}};
static const int bram_DRAGONWORM[][2] = {{304,256},{256,208}};
static const uint8_t sw_THUNDERGIGAS[] = {87,89,90,91,92,93,94,95,96,98,99,100,101,102,104,105,107,109,111,112,113,114,116,118,120,123};
static const int jump_THUNDERGIGAS[][2] = {{352,592},{416,544},{336,544},{272,576},{352,496},{400,544},{400,608}};
static const int pod_THUNDERGIGAS[][2] = {{288,592},{448,560}};
static const int trop_THUNDERGIGAS[][2] = {{384,528},{448,528},{272,576}};
static const int bram_THUNDERGIGAS[][2] = {{432,480},{320,592}};
static const struct sr_w6_bmap sr_w6_bmaps[] = {
  {"Mantis Ant", 260, 0, 26, sw_MANTISANT, {176,144}, {144,272}, {272,128}, {176,176}, 5, jump_MANTISANT, 2, pod_MANTISANT, 3, trop_MANTISANT, 2, bram_MANTISANT, 0},
  {"Wall Face", 166, 0, 7, sw_WALLFACE, {224,192}, {144,240}, {288,224}, {224,192}, 7, jump_WALLFACE, 2, pod_WALLFACE, 3, trop_WALLFACE, 2, bram_WALLFACE, 280},
  {"Tropicallo", 290, 0, 26, sw_TROPICALLO, {352,576}, {288,560}, {464,544}, {352,576}, 5, jump_TROPICALLO, 2, pod_TROPICALLO, 3, trop_TROPICALLO, 2, bram_TROPICALLO, 0},
  {"Minotaur", 347, 0, 26, sw_MINOTAUR, {176,224}, {160,256}, {336,304}, {176,224}, 5, jump_MINOTAUR, 2, pod_MINOTAUR, 3, trop_MINOTAUR, 2, bram_MINOTAUR, 0},
  {"Spikey Tiger", 310, 0, 26, sw_SPIKEY, {208,224}, {112,192}, {304,192}, {208,208}, 5, jump_SPIKEY, 2, pod_SPIKEY, 4, trop_SPIKEY, 2, bram_SPIKEY, 0},
  {"Jabberwocky", 143, 0, 26, sw_JABBER, {272,592}, {192,624}, {336,688}, {256,608}, 5, jump_JABBER, 2, pod_JABBER, 3, trop_JABBER, 2, bram_JABBER, 0},
  {"Spring Beak", 60, 0, 26, sw_SPRINGBEAK, {240,224}, {128,208}, {336,224}, {240,224}, 7, jump_SPRINGBEAK, 2, pod_SPRINGBEAK, 3, trop_SPRINGBEAK, 2, bram_SPRINGBEAK, 0},
  {"Frost Gigas", 364, 0, 26, sw_FROSTGIGAS, {160,192}, {96,112}, {208,112}, {144,208}, 7, jump_FROSTGIGAS, 2, pod_FROSTGIGAS, 3, trop_FROSTGIGAS, 2, bram_FROSTGIGAS, 0},
  {"Snap Dragon", 430, 0, 26, sw_SNAPDRAGON, {384,416}, {304,416}, {448,416}, {384,416}, 7, jump_SNAPDRAGON, 2, pod_SNAPDRAGON, 3, trop_SNAPDRAGON, 2, bram_SNAPDRAGON, 0},
  {"Mech Rider 1", 340, 0, 22, sw_MECHRIDER1, {304,240}, {160,224}, {448,256}, {304,240}, 7, jump_MECHRIDER1, 2, pod_MECHRIDER1, 3, trop_MECHRIDER1, 2, bram_MECHRIDER1, 0},
  {"Doom's Wall", 407, 0, 2, sw_DOOMSWALL, {0,0}, {0,0}, {0,0}, {0,0}, 0, jump_DOOMSWALL, 0, pod_DOOMSWALL, 0, trop_DOOMSWALL, 0, bram_DOOMSWALL, 280},
  {"Vampire", 408, 2, 26, sw_VAMPIRE, {224,288}, {144,336}, {288,336}, {224,288}, 7, jump_VAMPIRE, 2, pod_VAMPIRE, 3, trop_VAMPIRE, 2, bram_VAMPIRE, 0},
  {"Metal Mantis", 377, 0, 24, sw_METALMANTIS, {240,208}, {192,192}, {288,192}, {240,208}, 7, jump_METALMANTIS, 2, pod_METALMANTIS, 3, trop_METALMANTIS, 2, bram_METALMANTIS, 0},
  {"Mech Rider 2", 100, 0, 22, sw_MECHRIDER2, {384,304}, {224,304}, {528,304}, {384,336}, 7, jump_MECHRIDER2, 2, pod_MECHRIDER2, 3, trop_MECHRIDER2, 2, bram_MECHRIDER2, 0},
  {"Kilroy", 317, 0, 22, sw_KILROY, {304,352}, {240,352}, {368,352}, {304,352}, 7, jump_KILROY, 2, pod_KILROY, 3, trop_KILROY, 2, bram_KILROY, 0},
  {"Gorgon Bull", 119, 0, 26, sw_GORGON, {240,288}, {128,304}, {352,240}, {240,288}, 5, jump_GORGON, 2, pod_GORGON, 3, trop_GORGON, 2, bram_GORGON, 0},
  {"Boreal Face", 79, 0, 26, sw_BOREAL, {176,160}, {176,336}, {336,160}, {176,336}, 5, jump_BOREAL, 2, pod_BOREAL, 4, trop_BOREAL, 2, bram_BOREAL, 0},
  {"Great Viper", 325, 0, 26, sw_GREATVIPER, {288,240}, {224,224}, {400,272}, {320,304}, 5, jump_GREATVIPER, 2, pod_GREATVIPER, 4, trop_GREATVIPER, 2, bram_GREATVIPER, 0},
  {"Blue Spike", 111, 0, 26, sw_BLUESPIKE, {240,288}, {128,304}, {352,240}, {240,288}, 5, jump_BLUESPIKE, 2, pod_BLUESPIKE, 3, trop_BLUESPIKE, 2, bram_BLUESPIKE, 0},
  {"Hydra", 434, 3, 26, sw_HYDRA, {240,336}, {144,272}, {320,288}, {240,400}, 5, jump_HYDRA, 2, pod_HYDRA, 3, trop_HYDRA, 2, bram_HYDRA, 0},
  {"Watermelon", 238, 0, 7, sw_AEGRTOJNFSDGHOPALNIAPLEIAN, {224,160}, {144,176}, {288,144}, {224,160}, 7, jump_AEGRTOJNFSDGHOPALNIAPLEIAN, 2, pod_AEGRTOJNFSDGHOPALNIAPLEIAN, 3, trop_AEGRTOJNFSDGHOPALNIAPLEIAN, 2, bram_AEGRTOJNFSDGHOPALNIAPLEIAN, 248},
  {"Hexas", 418, 2, 26, sw_HEXAS, {240,320}, {160,288}, {320,352}, {240,320}, 5, jump_HEXAS, 2, pod_HEXAS, 3, trop_HEXAS, 2, bram_HEXAS, 0},
  {"Kettle Kin", 46, 3, 26, sw_KETTLEKIN, {176,176}, {96,160}, {240,144}, {160,208}, 5, jump_KETTLEKIN, 2, pod_KETTLEKIN, 3, trop_KETTLEKIN, 2, bram_KETTLEKIN, 0},
  {"Tonpole", 277, 0, 20, sw_TONPOLE, {288,160}, {256,128}, {352,176}, {304,208}, 5, jump_TONPOLE, 2, pod_TONPOLE, 3, trop_TONPOLE, 2, bram_TONPOLE, 0},
  {"Mech Rider 3", 237, 0, 22, sw_MECHRIDER3, {288,288}, {192,288}, {384,288}, {288,288}, 7, jump_MECHRIDER3, 2, pod_MECHRIDER3, 4, trop_MECHRIDER3, 2, bram_MECHRIDER3, 0},
  {"Snow Dragon", 230, 0, 6, sw_SNOWDRAGON, {224,272}, {224,176}, {320,224}, {224,272}, 7, jump_SNOWDRAGON, 2, pod_SNOWDRAGON, 3, trop_SNOWDRAGON, 2, bram_SNOWDRAGON, 248},
  {"Fire Gigas", 495, 0, 26, sw_FIREGIGAS, {240,320}, {160,288}, {320,352}, {240,320}, 5, jump_FIREGIGAS, 2, pod_FIREGIGAS, 3, trop_FIREGIGAS, 2, bram_FIREGIGAS, 0},
  {"Red Dragon", 231, 0, 6, sw_REDDRAGON, {224,272}, {224,176}, {320,224}, {224,272}, 7, jump_REDDRAGON, 2, pod_REDDRAGON, 3, trop_REDDRAGON, 2, bram_REDDRAGON, 264},
  {"Axe Beak", 213, 0, 26, sw_AXEBEAK, {320,352}, {224,416}, {416,288}, {304,432}, 8, jump_AXEBEAK, 2, pod_AXEBEAK, 3, trop_AXEBEAK, 2, bram_AXEBEAK, 0},
  {"Blue Dragon", 232, 0, 6, sw_BLUEDRAGON, {224,160}, {144,176}, {288,144}, {224,160}, 7, jump_BLUEDRAGON, 2, pod_BLUEDRAGON, 3, trop_BLUEDRAGON, 2, bram_BLUEDRAGON, 280},
  {"Buffy", 244, 0, 26, sw_BUFFY, {272,624}, {208,656}, {352,688}, {256,656}, 7, jump_BUFFY, 2, pod_BUFFY, 3, trop_BUFFY, 2, bram_BUFFY, 0},
  {"Dark Lich", 246, 2, 6, sw_DARKLICH, {288,288}, {224,288}, {368,288}, {288,288}, 7, jump_DARKLICH, 2, pod_DARKLICH, 3, trop_DARKLICH, 2, bram_DARKLICH, 416},
  {"Dragon Worm", 204, 0, 26, sw_DRAGONWORM, {256,208}, {224,272}, {336,176}, {272,224}, 5, jump_DRAGONWORM, 2, pod_DRAGONWORM, 4, trop_DRAGONWORM, 2, bram_DRAGONWORM, 0},
  {"Thunder Gigas", 226, 0, 26, sw_THUNDERGIGAS, {384,528}, {272,576}, {448,544}, {384,528}, 7, jump_THUNDERGIGAS, 2, pod_THUNDERGIGAS, 3, trop_THUNDERGIGAS, 2, bram_THUNDERGIGAS, 0},
};
