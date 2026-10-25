/* Palette tables of CharacterPaletteRandomizer.cs (author Moppleton), extracted mechanically; list contents are those of the C# after all Add calls (including the eye-white colors it appends to the skin lists). */

static const struct sr_w2_pal_def boy_originalShoes = {2, {{216,64,72},{248,168,40}}};
static const struct sr_w2_pal_def boy_grayShoes = {2, {{64,64,64},{128,128,128}}};
static const struct sr_w2_pal_def boy_blueShoes = {2, {{32,32,96},{64,64,160}}};
static const struct sr_w2_pal_def boy_redWhiteShoes = {2, {{216,64,72},{232,200,200}}};
static const struct sr_w2_pal_def boy_blueWhiteShoes = {2, {{72,64,216},{200,200,232}}};
static const struct sr_w2_pal_def boy_greenWhiteShoes = {2, {{32,120,72},{200,232,200}}};
static const struct sr_w2_pal_def boy_originalPants = {3, {{32,72,112},{56,144,192},{136,224,240}}};
static const struct sr_w2_pal_def boy_grayPants = {3, {{80,80,80},{144,144,144},{200,200,200}}};
static const struct sr_w2_pal_def boy_greenPants = {3, {{0,80,24},{0,128,48},{72,176,96}}};
static const struct sr_w2_pal_def boy_brownPants = {3, {{64,32,32},{96,64,64},{160,104,80}}};
static const struct sr_w2_pal_def boy_pinkPants = {3, {{128,32,64},{224,56,96},{232,112,144}}};
static const struct sr_w2_pal_def boy_originalBandana = {2, {{232,40,168},{248,176,160}}};
static const struct sr_w2_pal_def boy_greenBandana = {2, {{0,168,104},{112,184,96}}};
static const struct sr_w2_pal_def boy_blueBandana = {2, {{56,96,160},{120,136,208}}};
static const struct sr_w2_pal_def boy_grayBandana = {2, {{80,80,80},{160,160,160}}};
static const struct sr_w2_pal_def boy_yellowBandana = {2, {{176,176,40},{248,248,160}}};
static const struct sr_w2_pal_def boy_purpleBandana = {2, {{64,0,160},{192,72,248}}};
static const struct sr_w2_pal_def boy_originalSkin = {3, {{208,120,80},{240,184,120},{248,248,248}}};
static const struct sr_w2_pal_def boy_originalSkinEye = {0, {{0,0,0}}};
static const struct sr_w2_pal_def boy_paleSkin = {2, {{224,152,112},{248,216,152}}};
static const struct sr_w2_pal_def boy_darkSkin = {2, {{184,96,96},{232,160,96}}};
static const struct sr_w2_pal_def boy_blueSkin = {2, {{88,128,208},{128,192,232}}};
static const struct sr_w2_pal_def boy_originalHair = {4, {{128,64,64},{200,88,32},{248,152,32},{136,80,56}}};
static const struct sr_w2_pal_def boy_darkHair = {4, {{96,32,0},{168,56,0},{200,104,0},{104,48,24}}};
static const struct sr_w2_pal_def boy_blueHair = {4, {{16,72,104},{24,96,176},{16,128,216},{48,88,112}}};
static const struct sr_w2_pal_def boy_greenHair = {4, {{0,88,56},{8,160,80},{0,200,112},{32,96,72}}};
static const struct sr_w2_pal_def boy_pinkHair = {4, {{128,24,64},{200,32,88},{248,32,152},{136,56,80}}};
static const struct sr_w2_pal_def boy_purpleHair = {4, {{64,24,128},{88,32,200},{152,32,248},{80,56,136}}};
static const struct sr_w2_pal_def boy_testColors = {1, {{0,0,0}}};
static const struct sr_w2_pal_def *const boy_shoesColors[] = {&boy_originalShoes, &boy_grayShoes, &boy_blueShoes, &boy_redWhiteShoes, &boy_blueWhiteShoes, &boy_greenWhiteShoes};
#define BOY_SHOESCOLORS_N 6
static const struct sr_w2_pal_def *const boy_pantsColors[] = {&boy_originalPants, &boy_grayPants, &boy_greenPants, &boy_brownPants, &boy_pinkPants};
#define BOY_PANTSCOLORS_N 5
static const struct sr_w2_pal_def *const boy_bandanaColors[] = {&boy_originalBandana, &boy_greenBandana, &boy_blueBandana, &boy_grayBandana, &boy_yellowBandana, &boy_purpleBandana};
#define BOY_BANDANACOLORS_N 6
static const struct sr_w2_pal_def *const boy_skinColors[] = {&boy_originalSkin, &boy_paleSkin, &boy_darkSkin, &boy_blueSkin};
#define BOY_SKINCOLORS_N 4
static const struct sr_w2_pal_def *const boy_eyeWhiteColors[] = {&boy_originalSkinEye, &boy_originalSkinEye, &boy_originalSkinEye, &boy_originalSkinEye};
#define BOY_EYEWHITECOLORS_N 4
static const struct sr_w2_pal_def *const boy_hairColors[] = {&boy_originalHair, &boy_darkHair, &boy_blueHair, &boy_greenHair, &boy_pinkHair, &boy_purpleHair};
#define BOY_HAIRCOLORS_N 6

static const struct sr_w2_pal_def girl_originalEyes = {1, {{0,88,112}}};
static const struct sr_w2_pal_def girl_greenEyes = {1, {{0,160,88}}};
static const struct sr_w2_pal_def girl_brownEyes = {1, {{144,40,0}}};
static const struct sr_w2_pal_def girl_redEyes = {1, {{192,40,40}}};
static const struct sr_w2_pal_def girl_brightBlueEyes = {1, {{56,160,232}}};
static const struct sr_w2_pal_def girl_graySpecular = {1, {{232,232,232}}};
static const struct sr_w2_pal_def girl_originalPants = {3, {{160,40,112},{232,80,144},{248,144,232}}};
static const struct sr_w2_pal_def girl_boyPants = {3, {{32,72,112},{56,144,192},{136,224,240}}};
static const struct sr_w2_pal_def girl_grayPants = {3, {{80,80,80},{144,144,144},{200,200,200}}};
static const struct sr_w2_pal_def girl_darkGrayPants = {3, {{48,48,48},{112,112,112},{168,168,168}}};
static const struct sr_w2_pal_def girl_greenPants = {3, {{32,104,0},{8,160,32},{104,208,144}}};
static const struct sr_w2_pal_def girl_yellowPants = {3, {{144,96,8},{216,144,48},{232,216,96}}};
static const struct sr_w2_pal_def girl_purplePants = {3, {{112,40,160},{144,80,232},{232,144,248}}};
static const struct sr_w2_pal_def girl_originalShoes = {2, {{80,168,104},{152,232,168}}};
static const struct sr_w2_pal_def girl_boyShoes = {2, {{232,40,168},{248,176,160}}};
static const struct sr_w2_pal_def girl_greenShoes = {2, {{0,168,104},{112,184,96}}};
static const struct sr_w2_pal_def girl_blueShoes = {2, {{56,96,160},{120,136,208}}};
static const struct sr_w2_pal_def girl_grayShoes = {2, {{80,80,80},{160,160,160}}};
static const struct sr_w2_pal_def girl_originalSkin = {3, {{216,128,88},{248,216,168},{248,248,248}}};
static const struct sr_w2_pal_def girl_originalSkinEye = {0, {{0,0,0}}};
static const struct sr_w2_pal_def girl_boySkin = {2, {{208,120,80},{240,184,120}}};
static const struct sr_w2_pal_def girl_paleSkin = {2, {{224,152,112},{248,216,152}}};
static const struct sr_w2_pal_def girl_darkSkin = {2, {{184,96,96},{232,160,96}}};
static const struct sr_w2_pal_def girl_blueSkin = {2, {{112,152,232},{152,216,248}}};
static const struct sr_w2_pal_def girl_originalHair = {3, {{160,88,56},{216,120,0},{240,168,32}}};
static const struct sr_w2_pal_def girl_blueHair = {3, {{64,96,200},{32,152,216},{80,216,248}}};
static const struct sr_w2_pal_def girl_greenHair = {3, {{72,128,56},{40,176,112},{72,224,80}}};
static const struct sr_w2_pal_def girl_platinumHair = {3, {{168,152,96},{232,192,80},{248,248,104}}};
static const struct sr_w2_pal_def girl_strawberryHair = {3, {{192,88,88},{248,120,96},{248,168,96}}};
static const struct sr_w2_pal_def girl_purpleHair = {3, {{152,56,96},{208,32,168},{232,96,248}}};
static const struct sr_w2_pal_def girl_orangeHair = {3, {{192,40,0},{248,72,0},{248,128,0}}};
static const struct sr_w2_pal_def girl_testColors = {1, {{0,0,0}}};
static const struct sr_w2_pal_def *const girl_eyeColors[] = {&girl_originalEyes, &girl_greenEyes, &girl_brownEyes, &girl_redEyes, &girl_brightBlueEyes};
#define GIRL_EYECOLORS_N 5
static const struct sr_w2_pal_def *const girl_specularColors[] = {&girl_graySpecular};
#define GIRL_SPECULARCOLORS_N 1
static const struct sr_w2_pal_def *const girl_pantsColors[] = {&girl_originalPants, &girl_boyPants, &girl_grayPants, &girl_darkGrayPants, &girl_greenPants, &girl_yellowPants, &girl_purplePants};
#define GIRL_PANTSCOLORS_N 7
static const struct sr_w2_pal_def *const girl_shoesColors[] = {&girl_originalShoes, &girl_boyShoes, &girl_greenShoes, &girl_blueShoes, &girl_grayShoes};
#define GIRL_SHOESCOLORS_N 5
static const struct sr_w2_pal_def *const girl_skinColors[] = {&girl_originalSkin, &girl_boySkin, &girl_paleSkin, &girl_darkSkin, &girl_blueSkin};
#define GIRL_SKINCOLORS_N 5
static const struct sr_w2_pal_def *const girl_eyeWhiteColors[] = {&girl_originalSkinEye, &girl_originalSkinEye, &girl_originalSkinEye, &girl_originalSkinEye, &girl_originalSkinEye};
#define GIRL_EYEWHITECOLORS_N 5
static const struct sr_w2_pal_def *const girl_hairColors[] = {&girl_originalHair, &girl_blueHair, &girl_greenHair, &girl_platinumHair, &girl_strawberryHair, &girl_purpleHair, &girl_orangeHair};
#define GIRL_HAIRCOLORS_N 7

static const struct sr_w2_pal_def sprite_originalSkin = {3, {{136,96,40},{208,136,112},{248,192,168}}};
static const struct sr_w2_pal_def sprite_paleSkin = {3, {{160,120,64},{224,152,128},{248,208,184}}};
static const struct sr_w2_pal_def sprite_darkSkin = {3, {{120,64,8},{192,96,72},{224,152,112}}};
static const struct sr_w2_pal_def sprite_originalHair1 = {3, {{168,48,64},{208,88,104},{232,144,120}}};
static const struct sr_w2_pal_def sprite_blueHair1 = {3, {{64,48,168},{104,88,208},{120,144,232}}};
static const struct sr_w2_pal_def sprite_blondeHair1 = {3, {{184,112,80},{224,152,120},{248,208,136}}};
static const struct sr_w2_pal_def sprite_purpleHair1 = {3, {{128,8,72},{168,48,112},{200,112,128}}};
static const struct sr_w2_pal_def sprite_brownHair1 = {3, {{96,0,0},{136,32,0},{160,80,8}}};
static const struct sr_w2_pal_def sprite_originalHair2 = {2, {{248,192,128},{144,24,24}}};
static const struct sr_w2_pal_def sprite_blueHair2 = {2, {{128,192,248},{24,24,144}}};
static const struct sr_w2_pal_def sprite_blondeHair2 = {2, {{248,248,144},{160,88,40}}};
static const struct sr_w2_pal_def sprite_purpleHair2 = {2, {{224,168,136},{104,0,32}}};
static const struct sr_w2_pal_def sprite_brownHair2 = {2, {{184,104,8},{72,0,0}}};
static const struct sr_w2_pal_def sprite_originalShirt = {3, {{32,120,88},{80,176,104},{112,248,176}}};
static const struct sr_w2_pal_def sprite_blueShirt = {3, {{32,88,120},{80,104,176},{112,176,248}}};
static const struct sr_w2_pal_def sprite_pinkShirt = {3, {{120,32,88},{176,80,104},{248,112,176}}};
static const struct sr_w2_pal_def sprite_yellowShirt = {3, {{120,120,32},{176,176,64},{248,248,120}}};
static const struct sr_w2_pal_def sprite_testColors = {1, {{0,0,0}}};
static const struct sr_w2_pal_def *const sprite_skinColors[] = {&sprite_originalSkin, &sprite_paleSkin, &sprite_darkSkin};
#define SPRITE_SKINCOLORS_N 3
static const struct sr_w2_pal_def *const sprite_hairColors1[] = {&sprite_originalHair1, &sprite_blueHair1, &sprite_blondeHair1, &sprite_purpleHair1, &sprite_brownHair1};
#define SPRITE_HAIRCOLORS1_N 5
static const struct sr_w2_pal_def *const sprite_hairColors2[] = {&sprite_originalHair2, &sprite_blueHair2, &sprite_blondeHair2, &sprite_purpleHair2, &sprite_brownHair2};
#define SPRITE_HAIRCOLORS2_N 5
static const struct sr_w2_pal_def *const sprite_shirtColors[] = {&sprite_originalShirt, &sprite_blueShirt, &sprite_pinkShirt, &sprite_yellowShirt};
#define SPRITE_SHIRTCOLORS_N 4

