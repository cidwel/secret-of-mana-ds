/* Generated from WeaponRandomizer.cs (Moppleton) by gen_weapons.py: palette and weapon name tables. */
struct sr_w1_pal { uint8_t weapon_pal, menu_pal; };
static const struct sr_w1_pal sr_w1_pal_dullGreen = {0x02, 11};
static const struct sr_w1_pal sr_w1_pal_greenBrown = {0x00, 11};
static const struct sr_w1_pal sr_w1_pal_manaSwordColorMaybe = {0x04, 2};
static const struct sr_w1_pal sr_w1_pal_manaSwordColorSimilar = {0x04, 59};
static const struct sr_w1_pal sr_w1_pal_pink = {0x0E, 74};
static const struct sr_w1_pal sr_w1_pal_darkDullPurple = {0x10, 12};
static const struct sr_w1_pal sr_w1_pal_plainPurple = {0x14, 94};
static const struct sr_w1_pal sr_w1_pal_lavender = {0x16, 101};
static const struct sr_w1_pal sr_w1_pal_dirtColor = {0x1C, 6};
static const struct sr_w1_pal sr_w1_pal_whiteish = {0x20, 85};
static const struct sr_w1_pal sr_w1_pal_white_fork_thing = {0x20, 32};
static const struct sr_w1_pal sr_w1_pal_lightBlue = {0x22, 103};
static const struct sr_w1_pal sr_w1_pal_pastelPink = {0x30, 117};
static const struct sr_w1_pal sr_w1_pal_niceGreen = {0x40, 91};
static const struct sr_w1_pal sr_w1_pal_purpleGray = {0x42, 94};
static const struct sr_w1_pal sr_w1_pal_gray = {0x44, 5};
static const struct sr_w1_pal sr_w1_pal_demonBlood = {0x3A, 41};
static const struct sr_w1_pal sr_w1_pal_pearlSpear = {0x22, 22};
static const struct sr_w1_pal sr_w1_pal_garnetGauntlet = {0x3A, 152};
static const struct sr_w1_pal sr_w1_pal_purpleRainbow = {0x0A, 10};
static const struct sr_w1_pal sr_w1_pal_orangeGoldish = {0x1A, 4};
static const struct sr_w1_pal sr_w1_pal_brightYellow = {0x1E, 30};
static const struct sr_w1_pal sr_w1_pal_grayBlue = {0x0C, 17};
static const struct sr_w1_pal sr_w1_pal_redBanana = {0x3C, 66};
static const struct sr_w1_pal sr_w1_pal_thunderSaberColor = {0x26, 24};
static const struct sr_w1_pal sr_w1_pal_brightPink = {0x2E, 74};
static const struct sr_w1_pal sr_w1_pal_brightLightBlue = {0x32, 36};
static const struct sr_w1_pal sr_w1_pal_stick = {0x18, 27};
static const struct sr_w1_pal sr_w1_pal_bluePinkRainbow = {0x08, 7};
static const struct sr_w1_pal sr_w1_pal_grayish = {0x20, 85};
static const struct sr_w1_pal sr_w1_pal_akkhan = {0x20, 53};
static const struct sr_w1_pal sr_w1_pal_rusty = {0x18, 15};
static const struct sr_w1_pal sr_w1_pal_rustySword = {0x18, 4};
static const struct sr_w1_pal sr_w1_pal_redCleaver = {0x24, 66};
static const struct sr_w1_pal sr_w1_pal_magicalBoomerang = {0x24, 115};
static const struct sr_w1_pal sr_w1_pal_darkPurple = {0x12, 14};
static const struct sr_w1_pal sr_w1_pal_lumina = {0x28, 36};
static const struct sr_w1_pal sr_w1_pal_glowyOrange = {0x2A, 30};
static const struct sr_w1_pal sr_w1_pal_neonGreen = {0x2C, 91};
static const struct sr_w1_pal sr_w1_pal_fiery = {0x24, 115};
static const struct sr_w1_pal sr_w1_pal_icyBlueAndPurple = {0x3E, 62};
static const struct sr_w1_pal sr_w1_pal_blueAndGreen = {0x06, 7};
enum { SR_W1_C_NONE, SR_W1_C_STUN, SR_W1_C_FREEZE, SR_W1_C_ENGULF, SR_W1_C_POISON, SR_W1_C_CONFUSE, SR_W1_C_STONE, SR_W1_C_NONE_GOLDEN, SR_W1_C_NONE_RED, SR_W1_C_NONE_HOLY, SR_W1_C_NONE_BLACK, SR_W1_C_NONE_DRAGON, SR_W1_C_NONE_CRIT, SR_W1_C_NONE_HIT, SR_W1_C_NONE_SUPERCRIT, SR_W1_C_NONE_AGIPLUS, SR_W1_C_NONE_CONPLUS, SR_W1_C_NONE_INTPLUS, SR_W1_C_NONE_STRPLUS, SR_W1_C_NONE_GARBAGE, SR_W1_C_MANASTR, SR_W1_C_COUNT };
struct sr_w1_spec { const char *name; const uint8_t *icons; int n_icons; int cond; const char *desc; const struct sr_w1_pal *pal; };
struct sr_w1_cat { const char *name; const uint8_t *menu_icons; int n_menu_icons; const struct sr_w1_spec *specs; int n_specs; const uint8_t *proj_keys; const uint8_t *proj_vals; int n_proj; };
static const uint8_t sr_w1_ic_fistWeapon_1[] = {101};
static const uint8_t sr_w1_ic_fistWeapon_5[] = {99};
static const uint8_t sr_w1_ic_fistWeapon_6[] = {99};
static const uint8_t sr_w1_ic_fistWeapon_7[] = {99};
static const uint8_t sr_w1_ic_fistWeapon_8[] = {102};
static const uint8_t sr_w1_ic_fistWeapon_9[] = {101};
static const uint8_t sr_w1_ic_fistWeapon_10[] = {101};
static const uint8_t sr_w1_ic_fistWeapon_11[] = {100};
static const uint8_t sr_w1_ic_fistWeapon_12[] = {100};
static const uint8_t sr_w1_ic_fistWeapon_13[] = {100};
static const uint8_t sr_w1_ic_fistWeapon_16[] = {102};
static const uint8_t sr_w1_ic_fistWeapon_18[] = {100};
static const uint8_t sr_w1_ic_fistWeapon_19[] = {100};
static const uint8_t sr_w1_ic_fistWeapon_21[] = {101};
static const uint8_t sr_w1_ic_fistWeapon_22[] = {102};
static const uint8_t sr_w1_ic_fistWeapon_23[] = {101};
static const uint8_t sr_w1_ic_fistWeapon_24[] = {101};
static const uint8_t sr_w1_ic_fistWeapon_26[] = {101};
static const uint8_t sr_w1_ic_fistWeapon_27[] = {101};
static const uint8_t sr_w1_ic_fistWeapon_28[] = {102};
static const struct sr_w1_spec sr_w1_specs_fistWeapon[] = {
  {"Obsidian", NULL, 0, SR_W1_C_NONE, "A monk's dark artifact weapon.", NULL},
  {"Tilian Claw", sr_w1_ic_fistWeapon_1, 1, SR_W1_C_NONE, "A treasure of Loures Canals.", NULL},
  {"Kalkuri", NULL, 0, SR_W1_C_NONE, "A master monk's weapon.", NULL},
  {"Sura's Rampage", NULL, 0, SR_W1_C_NONE, "Best used against Porings.", NULL},
  {"Bone Fist", NULL, 0, SR_W1_C_NONE, "Like most fists, it's made of bone.", NULL},
  {"Cestus", sr_w1_ic_fistWeapon_5, 1, SR_W1_C_NONE, "Some sort of spiky hand thing.", NULL},
  {"Titan's Mitt", sr_w1_ic_fistWeapon_6, 1, SR_W1_C_NONE, "You can pick up level 2 rocks now!", NULL},
  {"Cestus", sr_w1_ic_fistWeapon_7, 1, SR_W1_C_NONE, "Some sort of spiky hand thing.", NULL},
  {"Garnet's Gauntlet", sr_w1_ic_fistWeapon_8, 1, SR_W1_C_NONE_STRPLUS, "A fusion of two other weapons.", &sr_w1_pal_garnetGauntlet},
  {"Dragon Claws", sr_w1_ic_fistWeapon_9, 1, SR_W1_C_NONE_DRAGON, "", NULL},
  {"Baghnakhs", sr_w1_ic_fistWeapon_10, 1, SR_W1_C_NONE_AGIPLUS, "Less gross than Terraria's [Fetid Baghnakhs.]", NULL},
  {"Power Glove", sr_w1_ic_fistWeapon_11, 1, SR_W1_C_NONE_STRPLUS, "It's so bad.", NULL},
  {"Heavy Glove", sr_w1_ic_fistWeapon_12, 1, SR_W1_C_NONE_STRPLUS, "Not actually very heavy.", NULL},
  {"Work Gloves", sr_w1_ic_fistWeapon_13, 1, SR_W1_C_NONE, "You could probably sell this for a lot of mesos.", &sr_w1_pal_whiteish},
  {"Chakra Hand", NULL, 0, SR_W1_C_NONE_INTPLUS, "", NULL},
  {"Hyper Fist", NULL, 0, SR_W1_C_NONE_AGIPLUS, "", NULL},
  {"Aura Glove", sr_w1_ic_fistWeapon_16, 1, SR_W1_C_NONE_GOLDEN, "", NULL},
  {"Deathreach", NULL, 0, SR_W1_C_STONE, "", NULL},
  {"Mitten", sr_w1_ic_fistWeapon_18, 1, SR_W1_C_NONE_GARBAGE, "It's just a soft, fuzzy mitten.", &sr_w1_pal_pastelPink},
  {"Super Slap", sr_w1_ic_fistWeapon_19, 1, SR_W1_C_NONE_AGIPLUS, "Princess Peach's mega-slap.", &sr_w1_pal_pastelPink},
  {"Sticky Glove", NULL, 0, SR_W1_C_STUN, "Mallow's sticky glove.", &sr_w1_pal_whiteish},
  {"Kaiser", sr_w1_ic_fistWeapon_21, 1, SR_W1_C_NONE_HOLY, "You feel like you could suplex a train.", NULL},
  {"Godhand", sr_w1_ic_fistWeapon_22, 1, SR_W1_C_NONE_HOLY, "Sorry, there's no materia slotted.", NULL},
  {"Flame Claws", sr_w1_ic_fistWeapon_23, 1, SR_W1_C_ENGULF, "Way warmer than mittens.", NULL},
  {"Ice Claws", sr_w1_ic_fistWeapon_24, 1, SR_W1_C_FREEZE, "AKA the [Santa Claws].", NULL},
  {"Touch of Death", NULL, 0, SR_W1_C_STONE, "Not too much death--just a touch.", NULL},
  {"Griffin Claws", sr_w1_ic_fistWeapon_26, 1, SR_W1_C_POISON, "I guess griffins have poisonous claws.", NULL},
  {"Moogle Claws", sr_w1_ic_fistWeapon_27, 1, SR_W1_C_POISON, "I didn't even know moogles had claws.", NULL},
  {"Crisis Arm", sr_w1_ic_fistWeapon_28, 1, SR_W1_C_CONFUSE, "Advanced weapon from 2300 AD.", NULL},
};
static const uint8_t sr_w1_menu_fistWeapon[] = {99, 100, 101, 102};
static const uint8_t sr_w1_pk_fistWeapon[] = {99, 100, 101, 102};
static const uint8_t sr_w1_pv_fistWeapon[] = {0, 0, 0, 0};
static const uint8_t sr_w1_ic_swordWeapon_29[] = {87};
static const uint8_t sr_w1_ic_swordWeapon_30[] = {87};
static const uint8_t sr_w1_ic_swordWeapon_39[] = {87};
static const uint8_t sr_w1_ic_swordWeapon_41[] = {86};
static const uint8_t sr_w1_ic_swordWeapon_48[] = {88};
static const uint8_t sr_w1_ic_swordWeapon_50[] = {84};
static const uint8_t sr_w1_ic_swordWeapon_51[] = {85};
static const uint8_t sr_w1_ic_swordWeapon_84[] = {86};
static const uint8_t sr_w1_ic_swordWeapon_90[] = {87};
static const uint8_t sr_w1_ic_swordWeapon_91[] = {87};
static const uint8_t sr_w1_ic_swordWeapon_92[] = {86};
static const uint8_t sr_w1_ic_swordWeapon_93[] = {86};
static const uint8_t sr_w1_ic_swordWeapon_94[] = {85};
static const uint8_t sr_w1_ic_swordWeapon_95[] = {87};
static const uint8_t sr_w1_ic_swordWeapon_96[] = {87};
static const uint8_t sr_w1_ic_swordWeapon_97[] = {86};
static const struct sr_w1_spec sr_w1_specs_swordWeapon[] = {
  {"Zweihander", NULL, 0, SR_W1_C_STUN, "A German two-handed sword.", NULL},
  {"Icebrand", NULL, 0, SR_W1_C_FREEZE, "Use it quickly before it melts.", NULL},
  {"Vorpal", NULL, 0, SR_W1_C_FREEZE, "A colder version of the Flamberge.", NULL},
  {"Frostmourne", NULL, 0, SR_W1_C_FREEZE, "Actually just a replica from Dalaran.", NULL},
  {"Frostbrand", NULL, 0, SR_W1_C_FREEZE, "Thanks for choosing Frost Brand Swords. Since 1972.", NULL},
  {"Shalla'tor", NULL, 0, SR_W1_C_FREEZE, "Half of the Shalamayne.", NULL},
  {"Flamberge", NULL, 0, SR_W1_C_ENGULF, "A warmer version of the Vorpal.", NULL},
  {"Ashbringer", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Laevatain", NULL, 0, SR_W1_C_ENGULF, "Not to be confused with Leviathan.", NULL},
  {"Marsil", NULL, 0, SR_W1_C_ENGULF, "A fiery blade from Dracula's castle.", NULL},
  {"Ellemayne", NULL, 0, SR_W1_C_ENGULF, "The other half of the Shalamayne.", NULL},
  {"Ashenrazor", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Sizzle Sword", NULL, 0, SR_W1_C_ENGULF, "Found on floor 87 of another game's Ancient Cave.", NULL},
  {"Hrunting", NULL, 0, SR_W1_C_POISON, "Not a typo of [Grunting.]", NULL},
  {"Terminus", NULL, 0, SR_W1_C_POISON, "", NULL},
  {"Tizona", NULL, 0, SR_W1_C_POISON, "", NULL},
  {"Milican's Sword", NULL, 0, SR_W1_C_STONE, "An old stoner's weapon.", NULL},
  {"Executioner", NULL, 0, SR_W1_C_STONE, "", NULL},
  {"Gorgon Blade", NULL, 0, SR_W1_C_STONE, "", NULL},
  {"Joyeuse", NULL, 0, SR_W1_C_NONE_GOLDEN, "Just holding it makes you happier.", NULL},
  {"Vjaya", NULL, 0, SR_W1_C_NONE_GOLDEN, "Not as dirty as it sounds.", NULL},
  {"Quel'Zaram", NULL, 0, SR_W1_C_NONE_GOLDEN, "", NULL},
  {"Tournesol", NULL, 0, SR_W1_C_NONE_GOLDEN, "Features both a sun and moon on the hilt.", NULL},
  {"Hy-Brasyl Escalon", NULL, 0, SR_W1_C_NONE_GOLDEN, "A master warrior's weapon.", NULL},
  {"Crimson Blade", NULL, 0, SR_W1_C_NONE_RED, "It's a red sword.", NULL},
  {"Crissaegrim", NULL, 0, SR_W1_C_CONFUSE, "", NULL},
  {"Valmanway", NULL, 0, SR_W1_C_CONFUSE, "", NULL},
  {"Moon Falux", NULL, 0, SR_W1_C_CONFUSE, "Found in Dhaos's Castle.", NULL},
  {"Moon Blade", NULL, 0, SR_W1_C_CONFUSE, "A larger version of a sword found in Lagoon Castle.", NULL},
  {"Masamune", sr_w1_ic_swordWeapon_29, 1, SR_W1_C_NONE, "Famously wielded by Sephiroth. And Frog. And Edge...", NULL},
  {"Muramasa", sr_w1_ic_swordWeapon_30, 1, SR_W1_C_NONE, "Almost as good as a Masamune.", NULL},
  {"Eclipse", NULL, 0, SR_W1_C_NONE, "A warrior's dark artifact weapon.", NULL},
  {"Shalamayne", NULL, 0, SR_W1_C_NONE, "Varian Wrynn's legendary sword.", NULL},
  {"Tsurugi", NULL, 0, SR_W1_C_NONE, "A double-edged Japanese blade.", NULL},
  {"Anathema", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Light Saber", NULL, 0, SR_W1_C_NONE, "It isn't very heavy at all!", &sr_w1_pal_brightLightBlue},
  {"Harkon's Sword", NULL, 0, SR_W1_C_NONE, "You're not a vampire, so it doesn't absorb health.", NULL},
  {"Atma Weapon", NULL, 0, SR_W1_C_NONE, "It's pure energy.", &sr_w1_pal_brightLightBlue},
  {"Sting", NULL, 0, SR_W1_C_NONE, "Every move you make, it'll be watching you.", NULL},
  {"Buster Sword", sr_w1_ic_swordWeapon_39, 1, SR_W1_C_NONE, "Cloud hasn't grown into it yet.", NULL},
  {"Sword of Kings", NULL, 0, SR_W1_C_NONE, "It took seven hours of Stonehenge grinding to find.", NULL},
  {"Rusty Sword", sr_w1_ic_swordWeapon_41, 1, SR_W1_C_NONE, "Beware of tetanus.", &sr_w1_pal_rustySword},
  {"Broad Sword", NULL, 0, SR_W1_C_NONE, "Men can wield it too.", NULL},
  {"Herald Sword", NULL, 0, SR_W1_C_NONE, "It's long been heralded as.. a pretty good sword.", NULL},
  {"Claymore", NULL, 0, SR_W1_C_NONE, "A marked improvement over the Clayless.", NULL},
  {"Sword Sword", NULL, 0, SR_W1_C_NONE, "Ticondera's greatest treasure.", NULL},
  {"Edge of Duality", NULL, 0, SR_W1_C_NONE, "A two-handed Hylian sword.", NULL},
  {"Grass Sword", NULL, 0, SR_W1_C_POISON, "May or may not be cursed.", &sr_w1_pal_greenBrown},
  {"Demon Blood Sword", sr_w1_ic_swordWeapon_48, 1, SR_W1_C_NONE, "Finn and Jake's family sword.", &sr_w1_pal_demonBlood},
  {"Gryffindor Sword", NULL, 0, SR_W1_C_NONE, "May present itself to any worthy Gryffindor.", &sr_w1_pal_greenBrown},
  {"Butter Knife", sr_w1_ic_swordWeapon_50, 1, SR_W1_C_NONE_GARBAGE, "It barely cuts butter, let alone Rabites.", &sr_w1_pal_rustySword},
  {"Excalipur", sr_w1_ic_swordWeapon_51, 1, SR_W1_C_NONE_GARBAGE, "Well, it was good enough for Gilgamesh.", &sr_w1_pal_rustySword},
  {"Dragon Buster", NULL, 0, SR_W1_C_NONE_DRAGON, "", NULL},
  {"Excalibur", NULL, 0, SR_W1_C_NONE_HOLY, "You may have seen it in every other game, ever.", NULL},
  {"Claimh Solais", NULL, 0, SR_W1_C_NONE_HOLY, "A sword with a sparkling blade.", NULL},
  {"Luminus", NULL, 0, SR_W1_C_NONE_HOLY, "A holy sword wielded by Alucard.", NULL},
  {"Mystletain", NULL, 0, SR_W1_C_NONE_HOLY, "It's made of wood, so be careful with it.", NULL},
  {"Master Sword", NULL, 0, SR_W1_C_NONE_HOLY, "The legendary blade of evil's bane.", NULL},
  {"Einlanzer", NULL, 0, SR_W1_C_NONE_HOLY, "A holy sword found at the Isle of the Damned.", NULL},
  {"Glamdring", NULL, 0, SR_W1_C_NONE_HOLY, "A rune-inscribed sword wielded by Gandalf.", NULL},
  {"Gurthang", NULL, 0, SR_W1_C_NONE_BLACK, "Alucard's demonic blood-draining sword.", NULL},
  {"Dainslef", NULL, 0, SR_W1_C_NONE_BLACK, "Alucard's demonic blood-draining sword.", NULL},
  {"Kaladbolg", NULL, 0, SR_W1_C_NONE_BLACK, "A demonic sword wielded by Soma Cruz.", NULL},
  {"Mormegil", NULL, 0, SR_W1_C_NONE_BLACK, "A black blade once owned by Alucard.", NULL},
  {"Tyrfingr", NULL, 0, SR_W1_C_NONE_BLACK, "A blade of Norse legend.", NULL},
  {"Katzbalger", NULL, 0, SR_W1_C_NONE_BLACK, "A short sword with an S-shaped guard.", NULL},
  {"Shadowblade", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Shard of Hate", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Defender", NULL, 0, SR_W1_C_NONE_CONPLUS, "", NULL},
  {"Mablung", NULL, 0, SR_W1_C_NONE_CONPLUS, "", NULL},
  {"Quel'Serrar", NULL, 0, SR_W1_C_NONE_CONPLUS, "", NULL},
  {"Schweizersabel", NULL, 0, SR_W1_C_NONE_CONPLUS, "It was a steal at 500,000 Zeny.", NULL},
  {"Enhancer", NULL, 0, SR_W1_C_NONE_INTPLUS, "", NULL},
  {"Apocalypse", NULL, 0, SR_W1_C_NONE_INTPLUS, "Triple materia growth!", NULL},
  {"Haedonggum", NULL, 0, SR_W1_C_NONE_INTPLUS, "", NULL},
  {"Rune Blade", NULL, 0, SR_W1_C_NONE_INTPLUS, "", NULL},
  {"Ragnarok", NULL, 0, SR_W1_C_NONE_STRPLUS, "", NULL},
  {"Quel'Delar", NULL, 0, SR_W1_C_NONE_STRPLUS, "", NULL},
  {"Byeollungum", NULL, 0, SR_W1_C_NONE_STRPLUS, "", NULL},
  {"Krasnaya", NULL, 0, SR_W1_C_NONE_STRPLUS, "", NULL},
  {"Dekar Blade", NULL, 0, SR_W1_C_NONE_STRPLUS, "Best used against jelly enemies.", NULL},
  {"Gades Blade", NULL, 0, SR_W1_C_NONE_STRPLUS, "It looked bigger when Gades was holding it.", NULL},
  {"Gigas Sword", NULL, 0, SR_W1_C_NONE_STRPLUS, "", NULL},
  {"In-Geom", NULL, 0, SR_W1_C_NONE_STRPLUS, "A Nephalem's legendary weapon.", NULL},
  {"Rainbow", sr_w1_ic_swordWeapon_84, 1, SR_W1_C_NONE_STRPLUS, "Melchior's finest weapon.", &sr_w1_pal_bluePinkRainbow},
  {"Arkhalis", NULL, 0, SR_W1_C_NONE_AGIPLUS, "A light sword designed for high-speed attacks.", NULL},
  {"Thunderfury", NULL, 0, SR_W1_C_NONE_AGIPLUS, "", NULL},
  {"Trisection", NULL, 0, SR_W1_C_NONE_AGIPLUS, "One of the better FFT battle themes.", NULL},
  {"Swallow", NULL, 0, SR_W1_C_NONE_AGIPLUS, "Once used by Crono. Or Serge, I guess.", NULL},
  {"Slasher", NULL, 0, SR_W1_C_NONE_AGIPLUS, "", NULL},
  {"Anemoi", sr_w1_ic_swordWeapon_90, 1, SR_W1_C_STUN, "Infused with wind power extracted from Honkai cores.", &sr_w1_pal_manaSwordColorSimilar},
  {"7th Sacred Relic", sr_w1_ic_swordWeapon_91, 1, SR_W1_C_NONE_HOLY, "An ancient sword powered by a Herrscher Core.", &sr_w1_pal_brightYellow},
  {"Swan Lake", sr_w1_ic_swordWeapon_92, 1, SR_W1_C_FREEZE, "Liliya Olenyeva's blade as smooth as a calm lake.", &sr_w1_pal_blueAndGreen},
  {"Sleeping Beauty", sr_w1_ic_swordWeapon_93, 1, SR_W1_C_STUN, "Rosaliya Olenyeva's sleeping blade.", &sr_w1_pal_brightPink},
  {"Godslayer Surtr", sr_w1_ic_swordWeapon_94, 1, SR_W1_C_ENGULF, "Wielded by the Vermillion Knight in her final battle.", &sr_w1_pal_orangeGoldish},
  {"Jizo Mitama", sr_w1_ic_swordWeapon_95, 1, SR_W1_C_NONE_BLACK, "A sword corrupted by Herrscher energy.", &sr_w1_pal_stick},
  {"Crystal Edge", sr_w1_ic_swordWeapon_96, 1, SR_W1_C_FREEZE, "Raiden Mei's frozen blade.", &sr_w1_pal_pearlSpear},
  {"Balmung", sr_w1_ic_swordWeapon_97, 1, SR_W1_C_NONE_DRAGON, "Sword capable of cutting through dragon scales.", &sr_w1_pal_darkPurple},
};
static const uint8_t sr_w1_menu_swordWeapon[] = {84, 85, 86, 87, 88};
static const uint8_t sr_w1_pk_swordWeapon[] = {84, 85, 86, 87, 88};
static const uint8_t sr_w1_pv_swordWeapon[] = {0, 0, 0, 0, 0};
static const struct sr_w1_spec sr_w1_specs_axeWeapon[] = {
  {"Bhuj", NULL, 0, SR_W1_C_NONE, "Named after the sound it makes when it hits stuff.", NULL},
  {"Voulge", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Ogre Killer", NULL, 0, SR_W1_C_NONE, "The reason they stopped making Shrek movies.", NULL},
  {"Watts's Axe", NULL, 0, SR_W1_C_NONE, "In a way, every axe is Watts's axe.", NULL},
  {"Lode Axe", NULL, 0, SR_W1_C_NONE, "Loading Axe...", NULL},
  {"Great Axe", NULL, 0, SR_W1_C_NONE, "It's really great.", NULL},
  {"Durin's Axe", NULL, 0, SR_W1_C_NONE, "Recovered from deep in the Mines of Moria.", NULL},
  {"Battle Axe", NULL, 0, SR_W1_C_NONE, "An axe used for battling.", NULL},
  {"Gorehowl", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Shadowmourne", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Doom Axe", NULL, 0, SR_W1_C_STONE, "DOOOOOOOOOOOOOOOOOOOM!", NULL},
  {"Bahamut's Tail", NULL, 0, SR_W1_C_NONE_BLACK, "Found in Morlia Gallery.", NULL},
  {"Talgonite Axe", NULL, 0, SR_W1_C_NONE_BLACK, "It's not hy-brasyl, but it'll do.", NULL},
  {"Were-Buster", NULL, 0, SR_W1_C_NONE_RED, "For turning Weres into Weren'ts.", NULL},
  {"Berdysz", NULL, 0, SR_W1_C_NONE_CONPLUS, "", NULL},
  {"Crimson Battle Axe", NULL, 0, SR_W1_C_NONE_RED, "It's just a big red axe.", NULL},
  {"Redner", NULL, 0, SR_W1_C_NONE_RED, "The Red Nirg's Versalian axe.", NULL},
  {"Rune Axe", NULL, 0, SR_W1_C_NONE_INTPLUS, "An axe with runes.", NULL},
  {"Francisca", NULL, 0, SR_W1_C_NONE_INTPLUS, "Whoever she is, they named this axe after her.", NULL},
  {"Giant's Axe", NULL, 0, SR_W1_C_POISON, "Not only is it giant, it's also poisonous.", NULL},
  {"Skorn", NULL, 0, SR_W1_C_POISON, "Hell hath no fury like... well, this.", NULL},
  {"Golden Axe", NULL, 0, SR_W1_C_NONE_GOLDEN, "They made a whole game series about this thing.", NULL},
  {"Helios", NULL, 0, SR_W1_C_NONE_GOLDEN, "From the tower by the same name.", NULL},
  {"Hy-Brasyl Battle Axe", NULL, 0, SR_W1_C_NONE_GOLDEN, "The Carnun Champion's axe.", NULL},
  {"Mega Ax", NULL, 0, SR_W1_C_NONE_STRPLUS, "Better than a Kilo Ax, worse than a Giga Ax.", NULL},
  {"Trisection", NULL, 0, SR_W1_C_NONE_AGIPLUS, "One of the better battle themes from FFT.", NULL},
  {"Atom Smasher", NULL, 0, SR_W1_C_NONE_STRPLUS, "Evermore's best axe. Toaster dog is still stronger.", NULL},
  {"Gigas Axe", NULL, 0, SR_W1_C_NONE_STRPLUS, "A really big axe.", NULL},
  {"Stout Axe", NULL, 0, SR_W1_C_NONE_CONPLUS, "", NULL},
  {"Great Value Axe", NULL, 0, SR_W1_C_NONE_GARBAGE, "Big sale on these at Pandora Walmart.", NULL},
};
static const uint8_t sr_w1_menu_axeWeapon[] = {89, 90, 91, 92};
static const uint8_t sr_w1_pk_axeWeapon[] = {89, 90, 91, 92};
static const uint8_t sr_w1_pv_axeWeapon[] = {0, 0, 0, 0};
static const uint8_t sr_w1_ic_spearWeapon_7[] = {104};
static const uint8_t sr_w1_ic_spearWeapon_8[] = {104};
static const uint8_t sr_w1_ic_spearWeapon_9[] = {104};
static const uint8_t sr_w1_ic_spearWeapon_17[] = {107};
static const uint8_t sr_w1_ic_spearWeapon_19[] = {108};
static const uint8_t sr_w1_ic_spearWeapon_20[] = {108};
static const uint8_t sr_w1_ic_spearWeapon_32[] = {106};
static const uint8_t sr_w1_ic_spearWeapon_33[] = {105};
static const struct sr_w1_spec sr_w1_specs_spearWeapon[] = {
  {"Luin of Celtchar", NULL, 0, SR_W1_C_ENGULF, "A flaming spear belonging to Celtchar mac Uthechar.", NULL},
  {"Areadbhair", NULL, 0, SR_W1_C_ENGULF, "Once belonged to Lugh of Tuatha De Danann.", NULL},
  {"Flame Lance", NULL, 0, SR_W1_C_ENGULF, "Keep it away from the Ice Lance.", NULL},
  {"Ice Lance", NULL, 0, SR_W1_C_FREEZE, "Possibly just a big icicle.", NULL},
  {"North Pole", NULL, 0, SR_W1_C_FREEZE, "From the weapons division of Santa's Workshop.", NULL},
  {"Spear", NULL, 0, SR_W1_C_NONE, "It's just a spear.", NULL},
  {"Heavy Spear", NULL, 0, SR_W1_C_NONE, "Heavier than your average spear.", NULL},
  {"Partisan", sr_w1_ic_spearWeapon_7, 1, SR_W1_C_NONE, "", NULL},
  {"Halberd", sr_w1_ic_spearWeapon_8, 1, SR_W1_C_NONE, "", NULL},
  {"Bovine Bardiche", sr_w1_ic_spearWeapon_9, 1, SR_W1_C_NONE, "Not from the cow level.", NULL},
  {"Amenonuhoko", NULL, 0, SR_W1_C_NONE, "An ancient Japanese spear.", NULL},
  {"Sky Piercer", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Rhongomyniad", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Dragonslayer", NULL, 0, SR_W1_C_NONE, "A spear born from the soul of Ornstein.", NULL},
  {"Zodiac Spear", NULL, 0, SR_W1_C_NONE, "You missed it in FF12, so now you can use it here instead.", NULL},
  {"Tjungkuletti", NULL, 0, SR_W1_C_NONE, "Said to drain the souls of the vicious porings it defeats.", NULL},
  {"Mecha-Halberd", NULL, 0, SR_W1_C_NONE, "The spoils of a really long walk to Midgard.", NULL},
  {"Pearl's Spear", sr_w1_ic_spearWeapon_17, 1, SR_W1_C_NONE_INTPLUS, "It's [strong in the real way.]", &sr_w1_pal_pearlSpear},
  {"Dragon Lance", NULL, 0, SR_W1_C_NONE_DRAGON, "", NULL},
  {"Ceremonial Trident", sr_w1_ic_spearWeapon_19, 1, SR_W1_C_NONE, "Found in the water around Zora's Domain.", NULL},
  {"Lightscale Trident", sr_w1_ic_spearWeapon_20, 1, SR_W1_C_NONE, "Mipha's legendary Zora spear.", NULL},
  {"Tonbogiri", NULL, 0, SR_W1_C_POISON, "", NULL},
  {"Gungnir", NULL, 0, SR_W1_C_NONE_HOLY, "Borrowed from Odin.", NULL},
  {"Longinus", NULL, 0, SR_W1_C_NONE_HOLY, "", NULL},
  {"Holy Lance", NULL, 0, SR_W1_C_NONE_HOLY, "", NULL},
  {"Pearl Lance", NULL, 0, SR_W1_C_NONE_HOLY, "Blame Ted Woolsey.", NULL},
  {"Daybreak", NULL, 0, SR_W1_C_NONE_GOLDEN, "Now you're ready to take on the Moon Lord.", NULL},
  {"Brionac", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Chauve-souris", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Ahlspiess", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Geiborg", NULL, 0, SR_W1_C_NONE_INTPLUS, "", NULL},
  {"Zephyrus", NULL, 0, SR_W1_C_NONE_INTPLUS, "", NULL},
  {"Daedalus Lance", sr_w1_ic_spearWeapon_32, 1, SR_W1_C_NONE_STRPLUS, "The best spear from vanilla SoM.", &sr_w1_pal_brightLightBlue},
  {"Spear of Justice", sr_w1_ic_spearWeapon_33, 1, SR_W1_C_NONE_CONPLUS, "It's filled with determination.", &sr_w1_pal_lightBlue},
  {"Gigas Lance", NULL, 0, SR_W1_C_NONE_STRPLUS, "A really big lance.", NULL},
  {"Crimson Lance", NULL, 0, SR_W1_C_NONE_RED, "It's a red lance.", NULL},
  {"Sprite's Spear", NULL, 0, SR_W1_C_CONFUSE, "The other two characters can use it too.", NULL},
  {"Oceanid Spear", NULL, 0, SR_W1_C_CONFUSE, "", NULL},
};
static const uint8_t sr_w1_menu_spearWeapon[] = {103, 104, 105, 106};
static const uint8_t sr_w1_pk_spearWeapon[] = {103, 104, 105, 106};
static const uint8_t sr_w1_pv_spearWeapon[] = {0, 0, 0, 0};
static const uint8_t sr_w1_ic_whipWeapon_3[] = {97};
static const uint8_t sr_w1_ic_whipWeapon_4[] = {96};
static const uint8_t sr_w1_ic_whipWeapon_5[] = {97};
static const uint8_t sr_w1_ic_whipWeapon_19[] = {97};
static const struct sr_w1_spec sr_w1_specs_whipWeapon[] = {
  {"Whip", NULL, 0, SR_W1_C_NONE, "Just your standard whip.", NULL},
  {"Chain Whip", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Witch's Locks", NULL, 0, SR_W1_C_ENGULF, "A treasure of the Grand Archives.", &sr_w1_pal_darkPurple},
  {"Morning Star", sr_w1_ic_whipWeapon_3, 1, SR_W1_C_NONE, "Doubles as a mattock.", NULL},
  {"Hammer Flail", sr_w1_ic_whipWeapon_4, 1, SR_W1_C_NONE, "", NULL},
  {"Amethyst's Whip", sr_w1_ic_whipWeapon_5, 1, SR_W1_C_NONE_AGIPLUS, "Found in a huge room full of garbage.", &sr_w1_pal_purpleGray},
  {"Flametongue", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Flamelash", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Sunfury", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Shooting Star", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Cool Whip", NULL, 0, SR_W1_C_FREEZE, "Why are you putting so much emphasis on the H?", NULL},
  {"Black Whip", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Mortal Coil", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Vampire Killer", NULL, 0, SR_W1_C_NONE_HOLY, "Normally reserved for Belmonts.", NULL},
  {"Nebula", NULL, 0, SR_W1_C_NONE_HOLY, "Soma's evil-seeking chain whip.", NULL},
  {"Flail of Hope", NULL, 0, SR_W1_C_NONE_HOLY, "I sure hope it's good.", NULL},
  {"Rosevine", NULL, 0, SR_W1_C_NONE_RED, "Found somewhere near Prontera.", NULL},
  {"Crimson Flail", NULL, 0, SR_W1_C_NONE_RED, "It's a red whip.", NULL},
  {"Stem of Nephentes", NULL, 0, SR_W1_C_NONE_INTPLUS, "", NULL},
  {"Akkhan's Addendum", sr_w1_ic_whipWeapon_19, 1, SR_W1_C_NONE_INTPLUS, "", &sr_w1_pal_akkhan},
  {"Nimbus Chain", NULL, 0, SR_W1_C_NONE_AGIPLUS, "", NULL},
  {"Backhand Whip", NULL, 0, SR_W1_C_NONE_AGIPLUS, "It's just a normal whip, but held awkwardly.", NULL},
  {"Dragon Whisker", NULL, 0, SR_W1_C_NONE_DRAGON, "I'm not even sure they have whiskers.", NULL},
  {"Gigas Flail", NULL, 0, SR_W1_C_NONE_STRPLUS, "", NULL},
};
static const uint8_t sr_w1_menu_whipWeapon[] = {93, 94, 95, 96, 97, 98};
static const uint8_t sr_w1_pk_whipWeapon[] = {93, 94, 95, 96, 97, 98};
static const uint8_t sr_w1_pv_whipWeapon[] = {0, 0, 0, 0, 0, 0};
static const struct sr_w1_spec sr_w1_specs_bowWeapon[] = {
  {"Chobin's Bow", NULL, 0, SR_W1_C_NONE, "It's pronounced Sho-BAN.", NULL},
  {"Short Bow", NULL, 0, SR_W1_C_NONE, "Shorter than the long bow.", NULL},
  {"Long Bow", NULL, 0, SR_W1_C_NONE, "Quite a bit longer than the short bow.", NULL},
  {"Great Bow", NULL, 0, SR_W1_C_NONE, "It's pretty great.", NULL},
  {"Valkyrie", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Avelyn", NULL, 0, SR_W1_C_NONE, "Sorry, it only fires one arrow.", NULL},
  {"Phrenic Bow", NULL, 0, SR_W1_C_NONE, "An old Sheikah weapon.", NULL},
  {"Trueshot", NULL, 0, SR_W1_C_NONE, "Way more accurate than the Falseshot.", NULL},
  {"Garuda Buster", NULL, 0, SR_W1_C_NONE_STRPLUS, "If you manage to find a garuda, it won't be happy.", NULL},
  {"Ancient Bow", NULL, 0, SR_W1_C_NONE, "Worth the 1,000 Rupees.", NULL},
  {"Hellwing", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Firebolt", NULL, 0, SR_W1_C_ENGULF, "It fires bolts.", NULL},
  {"Rain of Fire", NULL, 0, SR_W1_C_ENGULF, "You're going to need a stronger umbrella.", NULL},
  {"Shooting Star", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Starfall", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Flamecannon", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Flare Spread", NULL, 0, SR_W1_C_ENGULF, "Somehow you won that awful race in Alvanista.", NULL},
  {"Broken Bow", NULL, 0, SR_W1_C_NONE_GARBAGE, "You may as well just throw the arrows.", NULL},
  {"Phantasm", NULL, 0, SR_W1_C_CONFUSE, "", NULL},
  {"Chaos Bolt", NULL, 0, SR_W1_C_CONFUSE, "", NULL},
  {"Wing Bow", NULL, 0, SR_W1_C_CONFUSE, "", NULL},
  {"Doom Bow", NULL, 0, SR_W1_C_STONE, "DOOOOOOOOOOOOOOOOOOOM!", NULL},
  {"Demonbolt", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Shadow Bolt", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Ebonbolt", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Danetta's Revenge", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Danetta's Spite", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Kridershot", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Black Arrow", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Red Arrow", NULL, 0, SR_W1_C_NONE_RED, "Also includes a bow.", NULL},
  {"Golden Arrow", NULL, 0, SR_W1_C_NONE_GOLDEN, "Complementary gift from a hotel in Lake Placid.", NULL},
  {"Darkmoon Bow", NULL, 0, SR_W1_C_NONE_GOLDEN, "It took 5,000 souls to craft.", NULL},
  {"Bow of Hope", NULL, 0, SR_W1_C_NONE_HOLY, "I sure hope you like this bow.", NULL},
  {"Bow & Silver Arrows", NULL, 0, SR_W1_C_NONE_HOLY, "Only needed one to kill Ganon, these are leftovers.", NULL},
  {"Bow of Light", NULL, 0, SR_W1_C_NONE_HOLY, "Legendary bow that took out Calamity Ganon.", NULL},
  {"Auriel's Bow", NULL, 0, SR_W1_C_NONE_HOLY, "Legendary undead-killer of ancient Tamriel.", NULL},
  {"Yoichi's Bow", NULL, 0, SR_W1_C_NONE_STRPLUS, "A famous samurai's bow.", NULL},
  {"Ballista", NULL, 0, SR_W1_C_NONE_STRPLUS, "I guess it's a tiny handheld ballista.", NULL},
  {"Arbalest", NULL, 0, SR_W1_C_NONE_AGIPLUS, "", NULL},
  {"Elven Bow", NULL, 0, SR_W1_C_NONE_AGIPLUS, "Made by elves, for elves, from elves.", NULL},
  {"Wondershot", NULL, 0, SR_W1_C_NONE_AGIPLUS, "Forged by Taban himself.", NULL},
  {"Artea's Bow", NULL, 0, SR_W1_C_NONE_INTPLUS, "Artea's stuck with a Cursed Bow, so you can use this for now.", NULL},
  {"Elfin Bow", NULL, 0, SR_W1_C_NONE_INTPLUS, "Made by elves in a hollow tree.", NULL},
  {"Siren", NULL, 0, SR_W1_C_STUN, "Marle's favorite bow.", NULL},
};
static const uint8_t sr_w1_menu_bowWeapon[] = {111, 112, 113, 114};
static const uint8_t sr_w1_pk_bowWeapon[] = {111, 112, 113, 114};
static const uint8_t sr_w1_pv_bowWeapon[] = {1, 1, 1, 1};
static const uint8_t sr_w1_ic_boomerangWeapon_0[] = {116, 122};
static const uint8_t sr_w1_ic_boomerangWeapon_2[] = {116, 122};
static const uint8_t sr_w1_ic_boomerangWeapon_3[] = {115};
static const uint8_t sr_w1_ic_boomerangWeapon_4[] = {116};
static const uint8_t sr_w1_ic_boomerangWeapon_5[] = {115};
static const uint8_t sr_w1_ic_boomerangWeapon_6[] = {115};
static const uint8_t sr_w1_ic_boomerangWeapon_7[] = {116};
static const uint8_t sr_w1_ic_boomerangWeapon_8[] = {121};
static const uint8_t sr_w1_ic_boomerangWeapon_9[] = {120};
static const uint8_t sr_w1_ic_boomerangWeapon_10[] = {123};
static const uint8_t sr_w1_ic_boomerangWeapon_11[] = {123};
static const uint8_t sr_w1_ic_boomerangWeapon_12[] = {116};
static const uint8_t sr_w1_ic_boomerangWeapon_13[] = {115};
static const uint8_t sr_w1_ic_boomerangWeapon_14[] = {120, 122};
static const uint8_t sr_w1_ic_boomerangWeapon_15[] = {123};
static const uint8_t sr_w1_ic_boomerangWeapon_16[] = {116, 122};
static const uint8_t sr_w1_ic_boomerangWeapon_17[] = {120};
static const uint8_t sr_w1_ic_boomerangWeapon_18[] = {115};
static const struct sr_w1_spec sr_w1_specs_boomerangWeapon[] = {
  {"Eclipse", sr_w1_ic_boomerangWeapon_0, 2, SR_W1_C_NONE, "", &sr_w1_pal_darkPurple},
  {"Valkyrie", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Crescent", sr_w1_ic_boomerangWeapon_2, 2, SR_W1_C_NONE, "", NULL},
  {"Boomerang", sr_w1_ic_boomerangWeapon_3, 1, SR_W1_C_NONE, "Best used on octoroks, tektites, and kangaroos.", NULL},
  {"Chakram", sr_w1_ic_boomerangWeapon_4, 1, SR_W1_C_NONE, "", NULL},
  {"Lode Boomerang", sr_w1_ic_boomerangWeapon_5, 1, SR_W1_C_NONE, "", NULL},
  {"Red Cleaver", sr_w1_ic_boomerangWeapon_6, 1, SR_W1_C_NONE_RED, "", &sr_w1_pal_redCleaver},
  {"Lizal Boomerang", sr_w1_ic_boomerangWeapon_7, 1, SR_W1_C_NONE, "Dropped by a Lizalfos.", NULL},
  {"Cobra Shuttle", sr_w1_ic_boomerangWeapon_8, 1, SR_W1_C_POISON, "A shuttle, with snakes.", NULL},
  {"Frizbar", sr_w1_ic_boomerangWeapon_9, 1, SR_W1_C_NONE, "For extra-hardcore games of [Ultimate Frisbee.]", NULL},
  {"Shuriken", sr_w1_ic_boomerangWeapon_10, 1, SR_W1_C_NONE, "What? This isn't a boomerang at all.", NULL},
  {"Ninja's Trump", sr_w1_ic_boomerangWeapon_11, 1, SR_W1_C_NONE_STRPLUS, "As far as Trumps go, you could do way worse.", NULL},
  {"Amarok", sr_w1_ic_boomerangWeapon_12, 1, SR_W1_C_FREEZE, "It's actually just a yoyo.", NULL},
  {"Flamarang", sr_w1_ic_boomerangWeapon_13, 1, SR_W1_C_ENGULF, "A flaming boomerang made from Hellstone bars.", NULL},
  {"Rising Sun", sr_w1_ic_boomerangWeapon_14, 2, SR_W1_C_ENGULF, "When it comes back, it's the Setting Sun.", NULL},
  {"Shooting Star", sr_w1_ic_boomerangWeapon_15, 1, SR_W1_C_ENGULF, "", NULL},
  {"Moonring", sr_w1_ic_boomerangWeapon_16, 2, SR_W1_C_CONFUSE, "", NULL},
  {"Steven's Shield", sr_w1_ic_boomerangWeapon_17, 1, SR_W1_C_NONE_CONPLUS, "A shield made of Rose Quartz. Doubles as a thrown weapon.", &sr_w1_pal_pink},
  {"Magical Boomerang", sr_w1_ic_boomerangWeapon_18, 1, SR_W1_C_NONE_INTPLUS, "If a rupee drops, you'll be ready.", &sr_w1_pal_magicalBoomerang},
};
static const uint8_t sr_w1_menu_boomerangWeapon[] = {115, 116, 120, 121, 122, 123};
static const uint8_t sr_w1_pk_boomerangWeapon[] = {115, 116, 120, 121, 122, 123};
static const uint8_t sr_w1_pv_boomerangWeapon[] = {3, 5, 5, 5, 5, 5};
static const uint8_t sr_w1_ic_javelinWeapon_12[] = {108};
static const struct sr_w1_spec sr_w1_specs_javelinWeapon[] = {
  {"Pole Dart", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Javelin", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Light Trident", NULL, 0, SR_W1_C_NONE, "So light you can throw it at stuff.", NULL},
  {"Lode Javelin", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Fork of Hope", NULL, 0, SR_W1_C_NONE_HOLY, "", NULL},
  {"Imp's Fork", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
  {"Elf's Harpoon", NULL, 0, SR_W1_C_NONE_INTPLUS, "", NULL},
  {"Dragon Dart", NULL, 0, SR_W1_C_NONE_DRAGON, "", NULL},
  {"Valkyrian", NULL, 0, SR_W1_C_NONE_STRPLUS, "", NULL},
  {"Valkyrie", NULL, 0, SR_W1_C_NONE, "", NULL},
  {"Shooting Star", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Starfall", NULL, 0, SR_W1_C_ENGULF, "", NULL},
  {"Plastic Fork", sr_w1_ic_javelinWeapon_12, 1, SR_W1_C_NONE_GARBAGE, "It'll probably break after the first use.", &sr_w1_pal_white_fork_thing},
  {"Skylar", NULL, 0, SR_W1_C_NONE_GOLDEN, "Bought for 125,000 Mesos.", NULL},
  {"Machlear", NULL, 0, SR_W1_C_NONE_BLACK, "", NULL},
};
static const uint8_t sr_w1_menu_javelinWeapon[] = {107, 108, 109, 110};
static const uint8_t sr_w1_pk_javelinWeapon[] = {107, 108, 109, 110};
static const uint8_t sr_w1_pv_javelinWeapon[] = {2, 2, 2, 2};
static const struct sr_w1_spec sr_w1_specs_sharedSpecialNames[] = {
  {"Plumbus", NULL, 0, SR_W1_C_NONE, "A regular old plumbus.", NULL},
  {"Big Chungus", NULL, 0, SR_W1_C_NONE, "Oh lawd.", NULL},
  {"Toxicity", NULL, 0, SR_W1_C_POISON, "Disorder, disorder...", NULL},
  {"Burninator", NULL, 0, SR_W1_C_ENGULF, "Trogdor, the burninator.", NULL},
  {"Snow Halation", NULL, 0, SR_W1_C_FREEZE, "Muse's most loved, and SilvaGunner's least loved song.", NULL},
  {"Apoplexy", NULL, 0, SR_W1_C_STUN, "Best known as that song from FFT.", NULL},
  {"Serenity Now", NULL, 0, SR_W1_C_STUN, "... Insanity later.", NULL},
  {"Nirvana", NULL, 0, SR_W1_C_NONE_INTPLUS, "It smells like teen spirit.", NULL},
  {"Determination", NULL, 0, SR_W1_C_NONE_CONPLUS, "Do you want to have a bad time?", NULL},
};
#define SR_W1_N(a) ((int)(sizeof(a) / sizeof((a)[0])))
static const struct sr_w1_cat sr_w1_cats[8] = {
  {"fist", sr_w1_menu_fistWeapon, SR_W1_N(sr_w1_menu_fistWeapon), sr_w1_specs_fistWeapon, SR_W1_N(sr_w1_specs_fistWeapon), sr_w1_pk_fistWeapon, sr_w1_pv_fistWeapon, SR_W1_N(sr_w1_pk_fistWeapon)},
  {"sword", sr_w1_menu_swordWeapon, SR_W1_N(sr_w1_menu_swordWeapon), sr_w1_specs_swordWeapon, SR_W1_N(sr_w1_specs_swordWeapon), sr_w1_pk_swordWeapon, sr_w1_pv_swordWeapon, SR_W1_N(sr_w1_pk_swordWeapon)},
  {"axe", sr_w1_menu_axeWeapon, SR_W1_N(sr_w1_menu_axeWeapon), sr_w1_specs_axeWeapon, SR_W1_N(sr_w1_specs_axeWeapon), sr_w1_pk_axeWeapon, sr_w1_pv_axeWeapon, SR_W1_N(sr_w1_pk_axeWeapon)},
  {"spear", sr_w1_menu_spearWeapon, SR_W1_N(sr_w1_menu_spearWeapon), sr_w1_specs_spearWeapon, SR_W1_N(sr_w1_specs_spearWeapon), sr_w1_pk_spearWeapon, sr_w1_pv_spearWeapon, SR_W1_N(sr_w1_pk_spearWeapon)},
  {"whip", sr_w1_menu_whipWeapon, SR_W1_N(sr_w1_menu_whipWeapon), sr_w1_specs_whipWeapon, SR_W1_N(sr_w1_specs_whipWeapon), sr_w1_pk_whipWeapon, sr_w1_pv_whipWeapon, SR_W1_N(sr_w1_pk_whipWeapon)},
  {"bow", sr_w1_menu_bowWeapon, SR_W1_N(sr_w1_menu_bowWeapon), sr_w1_specs_bowWeapon, SR_W1_N(sr_w1_specs_bowWeapon), sr_w1_pk_bowWeapon, sr_w1_pv_bowWeapon, SR_W1_N(sr_w1_pk_bowWeapon)},
  {"boomerang", sr_w1_menu_boomerangWeapon, SR_W1_N(sr_w1_menu_boomerangWeapon), sr_w1_specs_boomerangWeapon, SR_W1_N(sr_w1_specs_boomerangWeapon), sr_w1_pk_boomerangWeapon, sr_w1_pv_boomerangWeapon, SR_W1_N(sr_w1_pk_boomerangWeapon)},
  {"javelin", sr_w1_menu_javelinWeapon, SR_W1_N(sr_w1_menu_javelinWeapon), sr_w1_specs_javelinWeapon, SR_W1_N(sr_w1_specs_javelinWeapon), sr_w1_pk_javelinWeapon, sr_w1_pv_javelinWeapon, SR_W1_N(sr_w1_pk_javelinWeapon)},
};
static const struct sr_w1_spec *const sr_w1_shared_specs = sr_w1_specs_sharedSpecialNames;
#define SR_W1_N_SHARED SR_W1_N(sr_w1_specs_sharedSpecialNames)
