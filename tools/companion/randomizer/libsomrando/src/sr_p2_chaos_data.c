/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Chaos mode (group w9): vanilla map tables.  Generated from processing/chaos/ChaosVanillaMaps.cs (Author: Moppleton) by a throw-away
 * script; the order of the entries is that of the C# lists.  Translated to C 2026-10-18.
 */
#include "sr_p2_chaos_int.h"

static const int cm0_d0_t[] = {1, 2, 3, 4};
static const int cm0_d1_t[] = {5, 6, 7};
static const uint8_t cm0_pal[] = {34, 101, 102, 106};
static const int cm0_rem[] = {0};
static const int cm0_trem[] = {0};
static const struct w9_door cm0_doors[] = {{cm0_d0_t, 4, 0, 110, 8}, {cm0_d1_t, 3, 2, 27, 38}};
static const int cm1_d0_t[] = {0};
static const int cm1_d1_t[] = {1};
static const uint8_t cm1_pal[] = {0};
static const int cm1_rem[] = {0};
static const int cm1_trem[] = {0};
static const struct w9_door cm1_doors[] = {{cm1_d0_t, 1, 4, 28, 11}, {cm1_d1_t, 1, 4, 10, 37}};
static const int cm2_d0_t[] = {0};
static const int cm2_d1_t[] = {1};
static const uint8_t cm2_pal[] = {0};
static const int cm2_rem[] = {0};
static const int cm2_trem[] = {0};
static const struct w9_door cm2_doors[] = {{cm2_d0_t, 1, 4, 11, 11}, {cm2_d1_t, 1, 4, 25, 38}};
static const int cm3_d0_t[] = {0, 1, 2};
static const int cm3_d1_t[] = {3, 4, 5};
static const uint8_t cm3_pal[] = {63, 64, 65, 66};
static const int cm3_rem[] = {0};
static const int cm3_trem[] = {0};
static const struct w9_door cm3_doors[] = {{cm3_d0_t, 3, 0, 11, 21}, {cm3_d1_t, 3, 1, 39, 51}};
static const int cm4_d0_t[] = {0};
static const int cm4_d1_t[] = {2, 3, 4, 5};
static const uint8_t cm4_pal[] = {34, 37, 57};
static const int cm4_rem[] = {0};
static const int cm4_trem[] = {0};
static const struct w9_door cm4_doors[] = {{cm4_d0_t, 1, 0, 12, 13}, {cm4_d1_t, 4, 0, 6, 28}};
static const int cm5_d0_t[] = {0};
static const int cm5_d1_t[] = {1};
static const uint8_t cm5_pal[] = {0};
static const int cm5_rem[] = {0};
static const int cm5_trem[] = {0};
static const struct w9_door cm5_doors[] = {{cm5_d0_t, 1, 2, 8, 16}, {cm5_d1_t, 1, 2, 118, 16}};
static const int cm6_d0_t[] = {0};
static const int cm6_d1_t[] = {1};
static const uint8_t cm6_pal[] = {0};
static const int cm6_rem[] = {0};
static const int cm6_trem[] = {0};
static const struct w9_door cm6_doors[] = {{cm6_d0_t, 1, 2, 8, 16}, {cm6_d1_t, 1, 2, 118, 16}};
static const int cm7_d0_t[] = {0, 1};
static const int cm7_d1_t[] = {2, 3};
static const uint8_t cm7_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm7_rem[] = {0};
static const int cm7_trem[] = {0};
static const struct w9_door cm7_doors[] = {{cm7_d0_t, 2, 0, 30, 12}, {cm7_d1_t, 2, 2, 30, 33}};
static const int cm8_d0_t[] = {0, 1, 2};
static const int cm8_d1_t[] = {3, 4};
static const uint8_t cm8_pal[] = {63, 64, 65, 66};
static const int cm8_rem[] = {0};
static const int cm8_trem[] = {0};
static const struct w9_door cm8_doors[] = {{cm8_d0_t, 3, 1, 39, 20}, {cm8_d1_t, 2, 2, 11, 47}};
static const int cm9_d0_t[] = {0, 1};
static const int cm9_d1_t[] = {2, 3, 4};
static const uint8_t cm9_pal[] = {63, 64, 65, 66};
static const int cm9_rem[] = {0};
static const int cm9_trem[] = {0};
static const struct w9_door cm9_doors[] = {{cm9_d0_t, 2, 3, 8, 24}, {cm9_d1_t, 3, 2, 32, 47}};
static const int cm10_d0_t[] = {1, 2};
static const int cm10_d1_t[] = {3};
static const uint8_t cm10_pal[] = {0};
static const int cm10_rem[] = {0};
static const int cm10_trem[] = {0};
static const struct w9_door cm10_doors[] = {{cm10_d0_t, 2, 0, 27, 20}, {cm10_d1_t, 1, 2, 19, 55}};
static const int cm11_d0_t[] = {0, 1, 2};
static const int cm11_d1_t[] = {3, 4, 5};
static const uint8_t cm11_pal[] = {0};
static const int cm11_rem[] = {0};
static const int cm11_trem[] = {0};
static const struct w9_door cm11_doors[] = {{cm11_d0_t, 3, 0, 23, 14}, {cm11_d1_t, 3, 3, 7, 26}};
static const int cm12_d0_t[] = {0, 1};
static const int cm12_d1_t[] = {2, 3};
static const uint8_t cm12_pal[] = {0};
static const int cm12_rem[] = {0};
static const int cm12_trem[] = {0};
static const struct w9_door cm12_doors[] = {{cm12_d0_t, 2, 0, 19, 15}, {cm12_d1_t, 2, 2, 19, 24}};
static const int cm13_d0_t[] = {0, 1};
static const int cm13_d1_t[] = {2, 3};
static const uint8_t cm13_pal[] = {92, 94};
static const int cm13_rem[] = {0};
static const int cm13_trem[] = {0};
static const struct w9_door cm13_doors[] = {{cm13_d0_t, 2, 0, 16, 17}, {cm13_d1_t, 2, 1, 38, 31}};
static const int cm14_d0_t[] = {0};
static const int cm14_d1_t[] = {1, 2};
static const uint8_t cm14_pal[] = {92, 94};
static const int cm14_rem[] = {0};
static const int cm14_trem[] = {0};
static const struct w9_door cm14_doors[] = {{cm14_d0_t, 1, 1, 54, 26}, {cm14_d1_t, 2, 2, 13, 39}};
static const int cm15_d0_t[] = {0, 1};
static const int cm15_d1_t[] = {2, 3};
static const uint8_t cm15_pal[] = {92, 94};
static const int cm15_rem[] = {0};
static const int cm15_trem[] = {0};
static const struct w9_door cm15_doors[] = {{cm15_d0_t, 2, 0, 15, 17}, {cm15_d1_t, 2, 2, 15, 39}};
static const int cm16_d0_t[] = {0, 1};
static const int cm16_d1_t[] = {2, 3};
static const uint8_t cm16_pal[] = {55, 94};
static const int cm16_rem[] = {0};
static const int cm16_trem[] = {0};
static const struct w9_door cm16_doors[] = {{cm16_d0_t, 2, 0, 29, 19}, {cm16_d1_t, 2, 2, 25, 59}};
static const int cm17_d0_t[] = {0, 1, 2};
static const int cm17_d1_t[] = {3, 4, 5};
static const uint8_t cm17_pal[] = {0};
static const int cm17_rem[] = {0};
static const int cm17_trem[] = {0};
static const struct w9_door cm17_doors[] = {{cm17_d0_t, 3, 0, 23, 14}, {cm17_d1_t, 3, 1, 39, 29}};
static const int cm18_d0_t[] = {0, 1, 2, 3};
static const int cm18_d1_t[] = {4, 5, 6};
static const uint8_t cm18_pal[] = {0};
static const int cm18_rem[] = {0};
static const int cm18_trem[] = {0};
static const struct w9_door cm18_doors[] = {{cm18_d0_t, 4, 0, 23, 14}, {cm18_d1_t, 3, 2, 21, 40}};
static const int cm19_d0_t[] = {0, 1};
static const int cm19_d1_t[] = {2, 3, 4};
static const uint8_t cm19_pal[] = {37};
static const int cm19_rem[] = {0};
static const int cm19_trem[] = {0};
static const struct w9_door cm19_doors[] = {{cm19_d0_t, 2, 0, 12, 8}, {cm19_d1_t, 3, 3, 5, 15}};
static const int cm20_d0_t[] = {8, 9};
static const int cm20_d1_t[] = {10, 11};
static const uint8_t cm20_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm20_rem[] = {0};
static const int cm20_trem[] = {0};
static const struct w9_door cm20_doors[] = {{cm20_d0_t, 2, 0, 23, 27}, {cm20_d1_t, 2, 2, 11, 30}};
static const int cm21_d0_t[] = {0, 1};
static const int cm21_d1_t[] = {3};
static const uint8_t cm21_pal[] = {37};
static const int cm21_rem[] = {0};
static const int cm21_trem[] = {2};
static const struct w9_door cm21_doors[] = {{cm21_d0_t, 2, 3, 17, 37}, {cm21_d1_t, 1, 3, 12, 61}};
static const int cm22_d0_t[] = {1};
static const int cm22_d1_t[] = {2};
static const uint8_t cm22_pal[] = {37};
static const int cm22_rem[] = {0};
static const int cm22_trem[] = {0};
static const struct w9_door cm22_doors[] = {{cm22_d0_t, 1, 1, 31, 11}, {cm22_d1_t, 1, 1, 31, 31}};
static const int cm23_d0_t[] = {0, 1};
static const int cm23_d1_t[] = {2, 3};
static const uint8_t cm23_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm23_rem[] = {0};
static const int cm23_trem[] = {0};
static const struct w9_door cm23_doors[] = {{cm23_d0_t, 2, 0, 9, 8}, {cm23_d1_t, 2, 2, 9, 25}};
static const int cm24_d0_t[] = {8, 9};
static const int cm24_d1_t[] = {10, 11, 12};
static const uint8_t cm24_pal[] = {33, 60, 68, 75, 79};
static const int cm24_rem[] = {0};
static const int cm24_trem[] = {0};
static const struct w9_door cm24_doors[] = {{cm24_d0_t, 2, 2, 13, 48}, {cm24_d1_t, 3, 2, 7, 55}};
static const int cm25_d0_t[] = {0};
static const int cm25_d1_t[] = {1, 2};
static const uint8_t cm25_pal[] = {33, 60, 68, 75, 79};
static const int cm25_rem[] = {0};
static const int cm25_trem[] = {0};
static const struct w9_door cm25_doors[] = {{cm25_d0_t, 1, 0, 56, 11}, {cm25_d1_t, 2, 2, 12, 35}};
static const int cm26_d0_t[] = {0, 1};
static const int cm26_d1_t[] = {2, 3};
static const uint8_t cm26_pal[] = {38, 41, 56, 58, 81, 82, 83, 84, 85};
static const int cm26_rem[] = {0};
static const int cm26_trem[] = {0};
static const struct w9_door cm26_doors[] = {{cm26_d0_t, 2, 0, 23, 20}, {cm26_d1_t, 2, 2, 23, 48}};
static const int cm27_d0_t[] = {0, 1};
static const int cm27_d1_t[] = {2, 3};
static const uint8_t cm27_pal[] = {54, 104, 115};
static const int cm27_rem[] = {0};
static const int cm27_trem[] = {0};
static const struct w9_door cm27_doors[] = {{cm27_d0_t, 2, 0, 15, 15}, {cm27_d1_t, 2, 2, 15, 27}};
static const int cm28_d0_t[] = {0, 1};
static const int cm28_d1_t[] = {2, 3};
static const uint8_t cm28_pal[] = {54, 104, 115};
static const int cm28_rem[] = {0};
static const int cm28_trem[] = {0};
static const struct w9_door cm28_doors[] = {{cm28_d0_t, 2, 1, 32, 21}, {cm28_d1_t, 2, 2, 11, 27}};
static const int cm29_d0_t[] = {0, 1};
static const int cm29_d1_t[] = {2, 3};
static const uint8_t cm29_pal[] = {54, 104, 115};
static const int cm29_rem[] = {0};
static const int cm29_trem[] = {0};
static const struct w9_door cm29_doors[] = {{cm29_d0_t, 2, 0, 18, 15}, {cm29_d1_t, 2, 2, 18, 28}};
static const int cm30_d0_t[] = {0, 1};
static const int cm30_d1_t[] = {2, 3};
static const uint8_t cm30_pal[] = {34, 101, 102, 106};
static const int cm30_rem[] = {0};
static const int cm30_trem[] = {0};
static const struct w9_door cm30_doors[] = {{cm30_d0_t, 2, 0, 15, 16}, {cm30_d1_t, 2, 0, 9, 45}};
static const int cm31_d0_t[] = {0, 1};
static const int cm31_d1_t[] = {2, 3};
static const uint8_t cm31_pal[] = {34, 101, 102, 106};
static const int cm31_rem[] = {0};
static const int cm31_trem[] = {0};
static const struct w9_door cm31_doors[] = {{cm31_d0_t, 2, 2, 11, 19}, {cm31_d1_t, 2, 2, 18, 20}};
static const int cm32_d0_t[] = {0, 1};
static const int cm32_d1_t[] = {2, 3};
static const uint8_t cm32_pal[] = {54, 104, 115};
static const int cm32_rem[] = {0};
static const int cm32_trem[] = {0};
static const struct w9_door cm32_doors[] = {{cm32_d0_t, 2, 0, 15, 16}, {cm32_d1_t, 2, 0, 20, 24}};
static const int cm33_d0_t[] = {0, 1};
static const int cm33_d1_t[] = {2, 3};
static const uint8_t cm33_pal[] = {54, 104, 115};
static const int cm33_rem[] = {0};
static const int cm33_trem[] = {0};
static const struct w9_door cm33_doors[] = {{cm33_d0_t, 2, 0, 14, 20}, {cm33_d1_t, 2, 2, 19, 37}};
static const int cm34_d0_t[] = {0, 1};
static const int cm34_d1_t[] = {2, 3};
static const uint8_t cm34_pal[] = {54, 104, 115};
static const int cm34_rem[] = {0};
static const int cm34_trem[] = {0};
static const struct w9_door cm34_doors[] = {{cm34_d0_t, 2, 1, 32, 21}, {cm34_d1_t, 2, 2, 12, 27}};
static const int cm35_d0_t[] = {0, 1};
static const int cm35_d1_t[] = {2, 3};
static const uint8_t cm35_pal[] = {54, 104, 115};
static const int cm35_rem[] = {0};
static const int cm35_trem[] = {0};
static const struct w9_door cm35_doors[] = {{cm35_d0_t, 2, 3, 10, 17}, {cm35_d1_t, 2, 2, 18, 29}};
static const int cm36_d0_t[] = {0};
static const int cm36_d1_t[] = {1, 2};
static const uint8_t cm36_pal[] = {40, 51, 75, 78};
static const int cm36_rem[] = {0};
static const int cm36_trem[] = {0};
static const struct w9_door cm36_doors[] = {{cm36_d0_t, 1, 0, 30, 10}, {cm36_d1_t, 2, 2, 17, 47}};
static const int cm37_d0_t[] = {0, 1};
static const int cm37_d1_t[] = {2, 3};
static const uint8_t cm37_pal[] = {54, 104, 115};
static const int cm37_rem[] = {0};
static const int cm37_trem[] = {0};
static const struct w9_door cm37_doors[] = {{cm37_d0_t, 2, 0, 27, 5}, {cm37_d1_t, 2, 2, 21, 26}};
static const int cm38_d0_t[] = {1};
static const int cm38_d1_t[] = {2};
static const uint8_t cm38_pal[] = {0};
static const int cm38_rem[] = {0};
static const int cm38_trem[] = {0};
static const struct w9_door cm38_doors[] = {{cm38_d0_t, 1, 4, 97, 90}, {cm38_d1_t, 1, 4, 37, 96}};
static const int cm39_d0_t[] = {0, 1};
static const int cm39_d1_t[] = {10, 11};
static const uint8_t cm39_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm39_rem[] = {0};
static const int cm39_trem[] = {0};
static const struct w9_door cm39_doors[] = {{cm39_d0_t, 2, 0, 8, 10}, {cm39_d1_t, 2, 2, 15, 21}};
static const int cm40_d0_t[] = {4, 5};
static const int cm40_d1_t[] = {6, 7};
static const uint8_t cm40_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm40_rem[] = {0};
static const int cm40_trem[] = {0};
static const struct w9_door cm40_doors[] = {{cm40_d0_t, 2, 0, 27, 11}, {cm40_d1_t, 2, 2, 27, 20}};
static const int cm41_d0_t[] = {2, 3};
static const int cm41_d1_t[] = {8, 9};
static const uint8_t cm41_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm41_rem[] = {0};
static const int cm41_trem[] = {0};
static const struct w9_door cm41_doors[] = {{cm41_d0_t, 2, 0, 40, 10}, {cm41_d1_t, 2, 2, 40, 20}};
static const int cm42_d0_t[] = {0, 1};
static const int cm42_d1_t[] = {2, 3};
static const uint8_t cm42_pal[] = {34, 101, 102, 106};
static const int cm42_rem[] = {0};
static const int cm42_trem[] = {0};
static const struct w9_door cm42_doors[] = {{cm42_d0_t, 2, 0, 11, 10}, {cm42_d1_t, 2, 2, 18, 21}};
static const int cm43_d0_t[] = {0, 1};
static const int cm43_d1_t[] = {2, 3};
static const uint8_t cm43_pal[] = {34, 101, 102, 106};
static const int cm43_rem[] = {0};
static const int cm43_trem[] = {0};
static const struct w9_door cm43_doors[] = {{cm43_d0_t, 2, 2, 23, 14}, {cm43_d1_t, 2, 2, 28, 31}};
static const int cm44_d0_t[] = {4, 5};
static const int cm44_d1_t[] = {10, 11};
static const uint8_t cm44_pal[] = {34, 101, 102, 106};
static const int cm44_rem[] = {0};
static const int cm44_trem[] = {0};
static const struct w9_door cm44_doors[] = {{cm44_d0_t, 2, 0, 28, 48}, {cm44_d1_t, 2, 2, 23, 70}};
static const int cm45_d0_t[] = {0, 1};
static const int cm45_d1_t[] = {2, 3};
static const uint8_t cm45_pal[] = {34, 101, 102, 106};
static const int cm45_rem[] = {0};
static const int cm45_trem[] = {0};
static const struct w9_door cm45_doors[] = {{cm45_d0_t, 2, 0, 16, 17}, {cm45_d1_t, 2, 0, 20, 26}};
static const int cm46_d0_t[] = {0, 1, 2};
static const int cm46_d1_t[] = {3};
static const uint8_t cm46_pal[] = {56, 58, 85, 86, 87};
static const int cm46_rem[] = {0};
static const int cm46_trem[] = {0};
static const struct w9_door cm46_doors[] = {{cm46_d0_t, 3, 0, 10, 10}, {cm46_d1_t, 1, 2, 10, 20}};
static const int cm47_d0_t[] = {0, 1};
static const int cm47_d1_t[] = {2, 3};
static const uint8_t cm47_pal[] = {56, 58, 85, 86, 87};
static const int cm47_rem[] = {0, 3, 4, 5, 6};
static const int cm47_trem[] = {0};
static const struct w9_door cm47_doors[] = {{cm47_d0_t, 2, 0, 12, 12}, {cm47_d1_t, 2, 2, 12, 30}};
static const int cm48_d0_t[] = {0, 1};
static const int cm48_d1_t[] = {16, 17};
static const uint8_t cm48_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm48_rem[] = {0};
static const int cm48_trem[] = {0};
static const struct w9_door cm48_doors[] = {{cm48_d0_t, 2, 0, 12, 11}, {cm48_d1_t, 2, 2, 38, 38}};
static const int cm49_d0_t[] = {0, 1};
static const int cm49_d1_t[] = {2, 3};
static const uint8_t cm49_pal[] = {34, 101, 102, 106};
static const int cm49_rem[] = {0};
static const int cm49_trem[] = {0};
static const struct w9_door cm49_doors[] = {{cm49_d0_t, 2, 2, 6, 14}, {cm49_d1_t, 2, 2, 27, 17}};
static const int cm50_d0_t[] = {4, 5};
static const int cm50_d1_t[] = {8, 9};
static const uint8_t cm50_pal[] = {34, 101, 102, 106};
static const int cm50_rem[] = {0};
static const int cm50_trem[] = {0};
static const struct w9_door cm50_doors[] = {{cm50_d0_t, 2, 0, 10, 29}, {cm50_d1_t, 2, 2, 25, 32}};
static const int cm51_d0_t[] = {2, 3};
static const int cm51_d1_t[] = {4, 5};
static const uint8_t cm51_pal[] = {34, 101, 102, 106};
static const int cm51_rem[] = {0};
static const int cm51_trem[] = {0};
static const struct w9_door cm51_doors[] = {{cm51_d0_t, 2, 2, 13, 22}, {cm51_d1_t, 2, 2, 19, 34}};
static const int cm52_d0_t[] = {0, 1};
static const int cm52_d1_t[] = {2, 3};
static const uint8_t cm52_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm52_rem[] = {0};
static const int cm52_trem[] = {0};
static const struct w9_door cm52_doors[] = {{cm52_d0_t, 2, 0, 7, 11}, {cm52_d1_t, 2, 0, 17, 11}};
static const int cm53_d0_t[] = {4, 5};
static const int cm53_d1_t[] = {6, 7};
static const uint8_t cm53_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm53_rem[] = {0};
static const int cm53_trem[] = {0};
static const struct w9_door cm53_doors[] = {{cm53_d0_t, 2, 0, 13, 22}, {cm53_d1_t, 2, 0, 20, 22}};
static const int cm54_d0_t[] = {2, 3};
static const int cm54_d1_t[] = {6, 7};
static const uint8_t cm54_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm54_rem[] = {0};
static const int cm54_trem[] = {0};
static const struct w9_door cm54_doors[] = {{cm54_d0_t, 2, 0, 5, 17}, {cm54_d1_t, 2, 2, 40, 21}};
static const int cm55_d0_t[] = {4, 5};
static const int cm55_d1_t[] = {8, 9};
static const uint8_t cm55_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm55_rem[] = {0};
static const int cm55_trem[] = {0};
static const struct w9_door cm55_doors[] = {{cm55_d0_t, 2, 0, 34, 19}, {cm55_d1_t, 2, 2, 46, 21}};
static const int cm56_d0_t[] = {4, 5};
static const int cm56_d1_t[] = {8, 9};
static const uint8_t cm56_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm56_rem[] = {0};
static const int cm56_trem[] = {0};
static const struct w9_door cm56_doors[] = {{cm56_d0_t, 2, 0, 22, 16}, {cm56_d1_t, 2, 2, 26, 20}};
static const int cm57_d0_t[] = {0};
static const int cm57_d1_t[] = {1, 2, 3};
static const uint8_t cm57_pal[] = {56, 58, 85, 86, 87};
static const int cm57_rem[] = {0};
static const int cm57_trem[] = {0};
static const struct w9_door cm57_doors[] = {{cm57_d0_t, 1, 0, 10, 11}, {cm57_d1_t, 3, 2, 10, 18}};
static const int cm58_d0_t[] = {0, 1, 2};
static const int cm58_d1_t[] = {3};
static const uint8_t cm58_pal[] = {56, 58, 85, 86, 87};
static const int cm58_rem[] = {0};
static const int cm58_trem[] = {0};
static const struct w9_door cm58_doors[] = {{cm58_d0_t, 3, 0, 10, 10}, {cm58_d1_t, 1, 2, 10, 19}};
static const int cm59_d0_t[] = {0};
static const int cm59_d1_t[] = {1, 2};
static const uint8_t cm59_pal[] = {56, 58, 85, 86, 87};
static const int cm59_rem[] = {0};
static const int cm59_trem[] = {0};
static const struct w9_door cm59_doors[] = {{cm59_d0_t, 1, 0, 31, 12}, {cm59_d1_t, 2, 2, 4, 15}};
static const int cm60_d0_t[] = {1};
static const int cm60_d1_t[] = {2};
static const uint8_t cm60_pal[] = {56, 58, 85, 86, 87};
static const int cm60_rem[] = {0};
static const int cm60_trem[] = {0};
static const struct w9_door cm60_doors[] = {{cm60_d0_t, 1, 4, 15, 18}, {cm60_d1_t, 1, 2, 7, 33}};
static const int cm61_d0_t[] = {0, 1};
static const int cm61_d1_t[] = {2, 3};
static const uint8_t cm61_pal[] = {34, 101, 102, 106};
static const int cm61_rem[] = {0};
static const int cm61_trem[] = {0};
static const struct w9_door cm61_doors[] = {{cm61_d0_t, 2, 0, 9, 8}, {cm61_d1_t, 2, 2, 9, 13}};
static const int cm62_d0_t[] = {4, 5};
static const int cm62_d1_t[] = {8, 9};
static const uint8_t cm62_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm62_rem[] = {0};
static const int cm62_trem[] = {0};
static const struct w9_door cm62_doors[] = {{cm62_d0_t, 2, 0, 8, 10}, {cm62_d1_t, 2, 2, 8, 24}};
static const int cm63_d0_t[] = {6, 7};
static const int cm63_d1_t[] = {10, 11};
static const uint8_t cm63_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm63_rem[] = {0};
static const int cm63_trem[] = {0};
static const struct w9_door cm63_doors[] = {{cm63_d0_t, 2, 0, 26, 14}, {cm63_d1_t, 2, 2, 26, 24}};
static const int cm64_d0_t[] = {0, 1};
static const int cm64_d1_t[] = {2, 3};
static const uint8_t cm64_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm64_rem[] = {0};
static const int cm64_trem[] = {0};
static const struct w9_door cm64_doors[] = {{cm64_d0_t, 2, 0, 9, 5}, {cm64_d1_t, 2, 2, 9, 22}};
static const int cm65_d0_t[] = {0, 1, 2};
static const int cm65_d1_t[] = {3};
static const uint8_t cm65_pal[] = {56, 58, 85, 86, 87};
static const int cm65_rem[] = {0};
static const int cm65_trem[] = {0};
static const struct w9_door cm65_doors[] = {{cm65_d0_t, 3, 0, 10, 10}, {cm65_d1_t, 1, 2, 10, 19}};
static const int cm66_d0_t[] = {0};
static const int cm66_d1_t[] = {1, 2, 3};
static const uint8_t cm66_pal[] = {56, 58, 85, 86, 87};
static const int cm66_rem[] = {0};
static const int cm66_trem[] = {0};
static const struct w9_door cm66_doors[] = {{cm66_d0_t, 1, 2, 13, 17}, {cm66_d1_t, 3, 2, 8, 18}};
static const int cm67_d0_t[] = {0, 1};
static const int cm67_d1_t[] = {8};
static const uint8_t cm67_pal[] = {56, 58, 85, 86, 87};
static const int cm67_rem[] = {0};
static const int cm67_trem[] = {0};
static const struct w9_door cm67_doors[] = {{cm67_d0_t, 2, 2, 16, 17}, {cm67_d1_t, 1, 2, 10, 32}};
static const int cm68_d0_t[] = {4, 5};
static const int cm68_d1_t[] = {9};
static const uint8_t cm68_pal[] = {56, 58, 85, 86, 87};
static const int cm68_rem[] = {0};
static const int cm68_trem[] = {0};
static const struct w9_door cm68_doors[] = {{cm68_d0_t, 2, 0, 17, 26}, {cm68_d1_t, 1, 2, 22, 32}};
static const int cm69_d0_t[] = {2, 3};
static const int cm69_d1_t[] = {6, 7};
static const uint8_t cm69_pal[] = {56, 58, 85, 86, 87};
static const int cm69_rem[] = {0};
static const int cm69_trem[] = {0};
static const struct w9_door cm69_doors[] = {{cm69_d0_t, 2, 2, 23, 17}, {cm69_d1_t, 2, 0, 29, 29}};
static const int cm70_d0_t[] = {0};
static const int cm70_d1_t[] = {1, 2, 3};
static const uint8_t cm70_pal[] = {56, 58, 85, 86, 87};
static const int cm70_rem[] = {0};
static const int cm70_trem[] = {0};
static const struct w9_door cm70_doors[] = {{cm70_d0_t, 1, 0, 10, 11}, {cm70_d1_t, 3, 2, 10, 18}};
static const int cm71_d0_t[] = {0, 1};
static const int cm71_d1_t[] = {2, 3};
static const uint8_t cm71_pal[] = {56, 58, 85, 86, 87};
static const int cm71_rem[] = {0};
static const int cm71_trem[] = {0};
static const struct w9_door cm71_doors[] = {{cm71_d0_t, 2, 0, 6, 12}, {cm71_d1_t, 2, 2, 16, 28}};
static const int cm72_d0_t[] = {0};
static const int cm72_d1_t[] = {4};
static const uint8_t cm72_pal[] = {33, 60, 68, 75, 79};
static const int cm72_rem[] = {0};
static const int cm72_trem[] = {0};
static const struct w9_door cm72_doors[] = {{cm72_d0_t, 1, 0, 72, 14}, {cm72_d1_t, 1, 2, 72, 22}};
static const int cm73_d0_t[] = {1};
static const int cm73_d1_t[] = {2, 3};
static const uint8_t cm73_pal[] = {33, 60, 68, 75, 79};
static const int cm73_rem[] = {0};
static const int cm73_trem[] = {0};
static const struct w9_door cm73_doors[] = {{cm73_d0_t, 1, 2, 19, 18}, {cm73_d1_t, 2, 2, 11, 21}};
static const int cm74_d0_t[] = {5};
static const int cm74_d1_t[] = {9};
static const uint8_t cm74_pal[] = {33, 60, 68, 75, 79};
static const int cm74_rem[] = {0};
static const int cm74_trem[] = {0};
static const struct w9_door cm74_doors[] = {{cm74_d0_t, 1, 0, 9, 42}, {cm74_d1_t, 1, 2, 9, 52}};
static const int cm75_d0_t[] = {6};
static const int cm75_d1_t[] = {10};
static const uint8_t cm75_pal[] = {33, 60, 68, 75, 79};
static const int cm75_rem[] = {0};
static const int cm75_trem[] = {0};
static const struct w9_door cm75_doors[] = {{cm75_d0_t, 1, 0, 40, 42}, {cm75_d1_t, 1, 2, 40, 52}};
static const int cm76_d0_t[] = {7};
static const int cm76_d1_t[] = {8};
static const uint8_t cm76_pal[] = {33, 60, 68, 75, 79};
static const int cm76_rem[] = {0};
static const int cm76_trem[] = {0};
static const struct w9_door cm76_doors[] = {{cm76_d0_t, 1, 0, 72, 42}, {cm76_d1_t, 1, 2, 72, 50}};
static const int cm77_d0_t[] = {2};
static const int cm77_d1_t[] = {3};
static const uint8_t cm77_pal[] = {33, 60, 68, 75, 79};
static const int cm77_rem[] = {0};
static const int cm77_trem[] = {0};
static const struct w9_door cm77_doors[] = {{cm77_d0_t, 1, 2, 35, 18}, {cm77_d1_t, 1, 2, 53, 18}};
static const int cm78_d0_t[] = {15};
static const int cm78_d1_t[] = {16};
static const uint8_t cm78_pal[] = {33, 60, 68, 75, 79};
static const int cm78_rem[] = {0};
static const int cm78_trem[] = {0};
static const struct w9_door cm78_doors[] = {{cm78_d0_t, 1, 0, 13, 92}, {cm78_d1_t, 1, 0, 31, 92}};
static const int cm79_d0_t[] = {0};
static const int cm79_d1_t[] = {4};
static const uint8_t cm79_pal[] = {33, 60, 68, 75, 79};
static const int cm79_rem[] = {0};
static const int cm79_trem[] = {0};
static const struct w9_door cm79_doors[] = {{cm79_d0_t, 1, 0, 94, 14}, {cm79_d1_t, 1, 2, 94, 22}};
static const int cm80_d0_t[] = {14};
static const int cm80_d1_t[] = {17, 18};
static const uint8_t cm80_pal[] = {33, 60, 68, 75, 79};
static const int cm80_rem[] = {0};
static const int cm80_trem[] = {0};
static const struct w9_door cm80_doors[] = {{cm80_d0_t, 1, 0, 92, 78}, {cm80_d1_t, 2, 2, 93, 89}};
static const int cm81_d0_t[] = {0};
static const int cm81_d1_t[] = {1, 2};
static const uint8_t cm81_pal[] = {33, 60, 68, 75, 79};
static const int cm81_rem[] = {0};
static const int cm81_trem[] = {0};
static const struct w9_door cm81_doors[] = {{cm81_d0_t, 1, 2, 44, 18}, {cm81_d1_t, 2, 2, 35, 22}};
static const int cm82_d0_t[] = {0, 1};
static const int cm82_d1_t[] = {4, 5};
static const uint8_t cm82_pal[] = {33, 60, 68, 75, 79};
static const int cm82_rem[] = {0};
static const int cm82_trem[] = {0};
static const struct w9_door cm82_doors[] = {{cm82_d0_t, 2, 0, 10, 15}, {cm82_d1_t, 2, 0, 20, 32}};
static const int cm83_d0_t[] = {0};
static const int cm83_d1_t[] = {1};
static const uint8_t cm83_pal[] = {33, 60, 68, 75, 79};
static const int cm83_rem[] = {0};
static const int cm83_trem[] = {0};
static const struct w9_door cm83_doors[] = {{cm83_d0_t, 1, 2, 10, 18}, {cm83_d1_t, 1, 2, 28, 18}};
static const int cm84_d0_t[] = {2};
static const int cm84_d1_t[] = {3};
static const uint8_t cm84_pal[] = {33, 60, 68, 75, 79};
static const int cm84_rem[] = {0};
static const int cm84_trem[] = {0};
static const struct w9_door cm84_doors[] = {{cm84_d0_t, 1, 2, 54, 18}, {cm84_d1_t, 1, 2, 72, 18}};
static const int cm85_d0_t[] = {4};
static const int cm85_d1_t[] = {8};
static const uint8_t cm85_pal[] = {33, 60, 68, 75, 79};
static const int cm85_rem[] = {0};
static const int cm85_trem[] = {0};
static const struct w9_door cm85_doors[] = {{cm85_d0_t, 1, 0, 12, 39}, {cm85_d1_t, 1, 2, 12, 48}};
static const int cm86_d0_t[] = {5};
static const int cm86_d1_t[] = {9};
static const uint8_t cm86_pal[] = {33, 60, 68, 75, 79};
static const int cm86_rem[] = {0};
static const int cm86_trem[] = {0};
static const struct w9_door cm86_doors[] = {{cm86_d0_t, 1, 0, 33, 39}, {cm86_d1_t, 1, 2, 33, 48}};
static const int cm87_d0_t[] = {6};
static const int cm87_d1_t[] = {10};
static const uint8_t cm87_pal[] = {33, 60, 68, 75, 79};
static const int cm87_rem[] = {0};
static const int cm87_trem[] = {0};
static const struct w9_door cm87_doors[] = {{cm87_d0_t, 1, 0, 56, 39}, {cm87_d1_t, 1, 2, 56, 48}};
static const int cm88_d0_t[] = {7};
static const int cm88_d1_t[] = {11};
static const uint8_t cm88_pal[] = {33, 60, 68, 75, 79};
static const int cm88_rem[] = {0};
static const int cm88_trem[] = {0};
static const struct w9_door cm88_doors[] = {{cm88_d0_t, 1, 0, 77, 39}, {cm88_d1_t, 1, 2, 77, 48}};
static const int cm89_d0_t[] = {0, 1};
static const int cm89_d1_t[] = {3, 4};
static const uint8_t cm89_pal[] = {0};
static const int cm89_rem[] = {0};
static const int cm89_trem[] = {0};
static const struct w9_door cm89_doors[] = {{cm89_d0_t, 2, 0, 14, 18}, {cm89_d1_t, 2, 2, 67, 61}};
static const int cm90_d0_t[] = {4, 5};
static const int cm90_d1_t[] = {6, 7};
static const uint8_t cm90_pal[] = {0};
static const int cm90_rem[] = {0};
static const int cm90_trem[] = {0};
static const struct w9_door cm90_doors[] = {{cm90_d0_t, 2, 2, 10, 48}, {cm90_d1_t, 2, 2, 16, 54}};
static const int cm91_d0_t[] = {0, 1};
static const int cm91_d1_t[] = {3, 4};
static const int cm91_d2_t[] = {5, 6};
static const uint8_t cm91_pal[] = {25, 26, 37, 62, 63, 64, 65, 66};
static const int cm91_rem[] = {0};
static const int cm91_trem[] = {2};
static const struct w9_door cm91_doors[] = {{cm91_d0_t, 2, 0, 21, 25}, {cm91_d1_t, 2, 3, 14, 39}, {cm91_d2_t, 2, 1, 85, 92}};
static const int cm92_d0_t[] = {0, 1};
static const int cm92_d1_t[] = {3, 4, 5};
static const int cm92_d2_t[] = {6, 7, 8, 9, 10};
static const uint8_t cm92_pal[] = {25, 26, 37, 62, 63, 64, 65, 66};
static const int cm92_rem[] = {2};
static const int cm92_trem[] = {0};
static const struct w9_door cm92_doors[] = {{cm92_d0_t, 2, 0, 9, 14}, {cm92_d1_t, 3, 1, 26, 40}, {cm92_d2_t, 5, 2, 15, 48}};
static const int cm93_d0_t[] = {0};
static const int cm93_d1_t[] = {1};
static const int cm93_d2_t[] = {2};
static const uint8_t cm93_pal[] = {0};
static const int cm93_rem[] = {0};
static const int cm93_trem[] = {0};
static const struct w9_door cm93_doors[] = {{cm93_d0_t, 1, 4, 24, 11}, {cm93_d1_t, 1, 4, 22, 22}, {cm93_d2_t, 1, 4, 13, 38}};
static const int cm94_d0_t[] = {0, 1};
static const int cm94_d1_t[] = {2, 3};
static const int cm94_d2_t[] = {4, 5};
static const uint8_t cm94_pal[] = {63, 64, 65, 66};
static const int cm94_rem[] = {0};
static const int cm94_trem[] = {0};
static const struct w9_door cm94_doors[] = {{cm94_d0_t, 2, 0, 29, 17}, {cm94_d1_t, 2, 1, 41, 50}, {cm94_d2_t, 2, 2, 20, 65}};
static const int cm95_d0_t[] = {0, 1};
static const int cm95_d1_t[] = {2, 3};
static const int cm95_d2_t[] = {4, 5};
static const uint8_t cm95_pal[] = {0};
static const int cm95_rem[] = {0};
static const int cm95_trem[] = {0};
static const struct w9_door cm95_doors[] = {{cm95_d0_t, 2, 0, 11, 8}, {cm95_d1_t, 2, 0, 50, 8}, {cm95_d2_t, 2, 0, 33, 17}};
static const int cm96_d0_t[] = {0, 1};
static const int cm96_d1_t[] = {2, 3};
static const int cm96_d2_t[] = {4, 5};
static const uint8_t cm96_pal[] = {92, 94};
static const int cm96_rem[] = {0};
static const int cm96_trem[] = {0};
static const struct w9_door cm96_doors[] = {{cm96_d0_t, 2, 0, 16, 17}, {cm96_d1_t, 2, 0, 36, 17}, {cm96_d2_t, 2, 2, 35, 55}};
static const int cm97_d0_t[] = {0, 1};
static const int cm97_d1_t[] = {3, 4};
static const int cm97_d2_t[] = {5, 6, 7};
static const uint8_t cm97_pal[] = {92, 94};
static const int cm97_rem[] = {0};
static const int cm97_trem[] = {2};
static const struct w9_door cm97_doors[] = {{cm97_d0_t, 2, 0, 17, 17}, {cm97_d1_t, 2, 1, 39, 30}, {cm97_d2_t, 3, 3, 7, 38}};
static const int cm98_d0_t[] = {0, 1, 2};
static const int cm98_d1_t[] = {3, 4, 5};
static const int cm98_d2_t[] = {6, 7, 8};
static const uint8_t cm98_pal[] = {0};
static const int cm98_rem[] = {0};
static const int cm98_trem[] = {0};
static const struct w9_door cm98_doors[] = {{cm98_d0_t, 3, 3, 7, 26}, {cm98_d1_t, 3, 1, 40, 29}, {cm98_d2_t, 3, 2, 21, 40}};
static const int cm99_d0_t[] = {0, 1, 2};
static const int cm99_d1_t[] = {3, 4, 5};
static const int cm99_d2_t[] = {6, 7, 8};
static const uint8_t cm99_pal[] = {0};
static const int cm99_rem[] = {0};
static const int cm99_trem[] = {0};
static const struct w9_door cm99_doors[] = {{cm99_d0_t, 3, 0, 23, 14}, {cm99_d1_t, 3, 3, 7, 26}, {cm99_d2_t, 3, 1, 40, 29}};
static const int cm100_d0_t[] = {0, 1, 2};
static const int cm100_d1_t[] = {3, 4, 5};
static const int cm100_d2_t[] = {6, 7, 8};
static const uint8_t cm100_pal[] = {0};
static const int cm100_rem[] = {0};
static const int cm100_trem[] = {0};
static const struct w9_door cm100_doors[] = {{cm100_d0_t, 3, 3, 7, 26}, {cm100_d1_t, 3, 1, 40, 29}, {cm100_d2_t, 3, 2, 21, 40}};
static const int cm101_d0_t[] = {0, 1, 2};
static const int cm101_d1_t[] = {3, 4, 5};
static const int cm101_d2_t[] = {6, 7, 8};
static const uint8_t cm101_pal[] = {0};
static const int cm101_rem[] = {0};
static const int cm101_trem[] = {0};
static const struct w9_door cm101_doors[] = {{cm101_d0_t, 3, 0, 23, 14}, {cm101_d1_t, 3, 3, 7, 26}, {cm101_d2_t, 3, 1, 40, 29}};
static const int cm102_d0_t[] = {0, 1};
static const int cm102_d1_t[] = {2, 3};
static const int cm102_d2_t[] = {4, 5};
static const uint8_t cm102_pal[] = {54, 104, 115};
static const int cm102_rem[] = {0};
static const int cm102_trem[] = {0};
static const struct w9_door cm102_doors[] = {{cm102_d0_t, 2, 0, 18, 15}, {cm102_d1_t, 2, 0, 10, 20}, {cm102_d2_t, 2, 2, 18, 28}};
static const int cm103_d0_t[] = {0, 1};
static const int cm103_d1_t[] = {2, 3};
static const int cm103_d2_t[] = {4, 5};
static const uint8_t cm103_pal[] = {54, 104, 115};
static const int cm103_rem[] = {0};
static const int cm103_trem[] = {0};
static const struct w9_door cm103_doors[] = {{cm103_d0_t, 2, 0, 15, 19}, {cm103_d1_t, 2, 3, 13, 28}, {cm103_d2_t, 2, 2, 16, 44}};
static const int cm104_d0_t[] = {0, 1};
static const int cm104_d1_t[] = {2, 3};
static const int cm104_d2_t[] = {4, 5};
static const uint8_t cm104_pal[] = {54, 104, 115};
static const int cm104_rem[] = {0};
static const int cm104_trem[] = {0};
static const struct w9_door cm104_doors[] = {{cm104_d0_t, 2, 0, 19, 21}, {cm104_d1_t, 2, 1, 31, 37}, {cm104_d2_t, 2, 2, 17, 39}};
static const int cm105_d0_t[] = {0, 1};
static const int cm105_d1_t[] = {2, 3};
static const int cm105_d2_t[] = {4, 5};
static const uint8_t cm105_pal[] = {54, 104, 115};
static const int cm105_rem[] = {0};
static const int cm105_trem[] = {0};
static const struct w9_door cm105_doors[] = {{cm105_d0_t, 2, 0, 7, 15}, {cm105_d1_t, 2, 0, 23, 21}, {cm105_d2_t, 2, 2, 16, 29}};
static const int cm106_d0_t[] = {0, 1};
static const int cm106_d1_t[] = {2, 3};
static const int cm106_d2_t[] = {4, 5};
static const uint8_t cm106_pal[] = {54, 104, 115};
static const int cm106_rem[] = {0};
static const int cm106_trem[] = {0};
static const struct w9_door cm106_doors[] = {{cm106_d0_t, 2, 0, 34, 18}, {cm106_d1_t, 2, 3, 12, 19}, {cm106_d2_t, 2, 3, 15, 33}};
static const int cm107_d0_t[] = {8, 9};
static const int cm107_d1_t[] = {10, 11};
static const int cm107_d2_t[] = {12, 13};
static const uint8_t cm107_pal[] = {34, 101, 102, 106};
static const int cm107_rem[] = {0};
static const int cm107_trem[] = {0};
static const struct w9_door cm107_doors[] = {{cm107_d0_t, 2, 0, 9, 67}, {cm107_d1_t, 2, 0, 27, 77}, {cm107_d2_t, 2, 0, 24, 85}};
static const int cm108_d0_t[] = {0, 1};
static const int cm108_d1_t[] = {2, 3};
static const int cm108_d2_t[] = {6, 7};
static const uint8_t cm108_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm108_rem[] = {0};
static const int cm108_trem[] = {0};
static const struct w9_door cm108_doors[] = {{cm108_d0_t, 2, 2, 26, 9}, {cm108_d1_t, 2, 2, 7, 12}, {cm108_d2_t, 2, 2, 11, 20}};
static const int cm109_d0_t[] = {0};
static const int cm109_d1_t[] = {1};
static const int cm109_d2_t[] = {2, 3};
static const uint8_t cm109_pal[] = {56, 58, 85, 86, 87};
static const int cm109_rem[] = {0};
static const int cm109_trem[] = {0};
static const struct w9_door cm109_doors[] = {{cm109_d0_t, 1, 0, 7, 9}, {cm109_d1_t, 1, 0, 14, 9}, {cm109_d2_t, 2, 2, 11, 18}};
static const int cm110_d0_t[] = {0, 1};
static const int cm110_d1_t[] = {2, 3};
static const int cm110_d2_t[] = {12, 13};
static const uint8_t cm110_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm110_rem[] = {0};
static const int cm110_trem[] = {0};
static const struct w9_door cm110_doors[] = {{cm110_d0_t, 2, 0, 20, 8}, {cm110_d1_t, 2, 0, 32, 8}, {cm110_d2_t, 2, 2, 32, 24}};
static const int cm111_d0_t[] = {0, 1};
static const int cm111_d1_t[] = {2, 3};
static const int cm111_d2_t[] = {4, 5};
static const uint8_t cm111_pal[] = {56, 58, 85, 86, 87};
static const int cm111_rem[] = {0, 1};
static const int cm111_trem[] = {0};
static const struct w9_door cm111_doors[] = {{cm111_d0_t, 2, 0, 13, 12}, {cm111_d1_t, 2, 0, 18, 12}, {cm111_d2_t, 2, 2, 16, 28}};
static const int cm112_d0_t[] = {11};
static const int cm112_d1_t[] = {12};
static const int cm112_d2_t[] = {13};
static const uint8_t cm112_pal[] = {33, 60, 68, 75, 79};
static const int cm112_rem[] = {0};
static const int cm112_trem[] = {0};
static const struct w9_door cm112_doors[] = {{cm112_d0_t, 1, 0, 15, 77}, {cm112_d1_t, 1, 0, 37, 77}, {cm112_d2_t, 1, 0, 59, 77}};
static const int cm113_d0_t[] = {0, 1};
static const int cm113_d1_t[] = {2, 3};
static const int cm113_d2_t[] = {4, 5};
static const uint8_t cm113_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm113_rem[] = {0};
static const int cm113_trem[] = {0};
static const struct w9_door cm113_doors[] = {{cm113_d0_t, 2, 0, 25, 33}, {cm113_d1_t, 2, 0, 25, 40}, {cm113_d2_t, 2, 2, 25, 58}};
static const int cm114_d0_t[] = {0, 1};
static const int cm114_d1_t[] = {2, 3};
static const int cm114_d2_t[] = {4, 5};
static const uint8_t cm114_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm114_rem[] = {0};
static const int cm114_trem[] = {0};
static const struct w9_door cm114_doors[] = {{cm114_d0_t, 2, 0, 10, 18}, {cm114_d1_t, 2, 0, 31, 18}, {cm114_d2_t, 2, 2, 21, 25}};
static const int cm115_d0_t[] = {0, 1};
static const int cm115_d1_t[] = {3, 4};
static const int cm115_d2_t[] = {5, 6};
static const int cm115_d3_t[] = {7, 8};
static const uint8_t cm115_pal[] = {25, 26, 37, 62, 63, 64, 65, 66};
static const int cm115_rem[] = {0};
static const int cm115_trem[] = {2};
static const struct w9_door cm115_doors[] = {{cm115_d0_t, 2, 0, 21, 21}, {cm115_d1_t, 2, 0, 46, 46}, {cm115_d2_t, 2, 0, 51, 48}, {cm115_d3_t, 2, 2, 27, 84}};
static const int cm116_d0_t[] = {0, 1, 2, 3};
static const int cm116_d1_t[] = {4, 5, 6};
static const int cm116_d2_t[] = {7, 9};
static const int cm116_d3_t[] = {8, 10};
static const uint8_t cm116_pal[] = {63, 64, 65, 66};
static const int cm116_rem[] = {11};
static const int cm116_trem[] = {0};
static const struct w9_door cm116_doors[] = {{cm116_d0_t, 4, 0, 31, 19}, {cm116_d1_t, 3, 0, 11, 20}, {cm116_d2_t, 2, 3, 8, 41}, {cm116_d3_t, 2, 1, 41, 41}};
static const int cm117_d0_t[] = {0, 1, 2};
static const int cm117_d1_t[] = {3, 4, 5};
static const int cm117_d2_t[] = {6, 7, 8};
static const int cm117_d3_t[] = {9, 10, 11};
static const uint8_t cm117_pal[] = {0};
static const int cm117_rem[] = {0};
static const int cm117_trem[] = {0};
static const struct w9_door cm117_doors[] = {{cm117_d0_t, 3, 0, 23, 14}, {cm117_d1_t, 3, 3, 7, 26}, {cm117_d2_t, 3, 1, 40, 29}, {cm117_d3_t, 3, 2, 21, 40}};
static const int cm118_d0_t[] = {0, 1, 2};
static const int cm118_d1_t[] = {3, 4, 5};
static const int cm118_d2_t[] = {6, 7, 8};
static const int cm118_d3_t[] = {9, 10, 11};
static const uint8_t cm118_pal[] = {0};
static const int cm118_rem[] = {0};
static const int cm118_trem[] = {0};
static const struct w9_door cm118_doors[] = {{cm118_d0_t, 3, 0, 23, 14}, {cm118_d1_t, 3, 3, 7, 26}, {cm118_d2_t, 3, 1, 40, 29}, {cm118_d3_t, 3, 2, 21, 40}};
static const int cm119_d0_t[] = {0, 1, 2};
static const int cm119_d1_t[] = {3, 4, 5};
static const int cm119_d2_t[] = {6, 7, 8};
static const int cm119_d3_t[] = {9, 10, 11};
static const uint8_t cm119_pal[] = {0};
static const int cm119_rem[] = {0};
static const int cm119_trem[] = {0};
static const struct w9_door cm119_doors[] = {{cm119_d0_t, 3, 0, 23, 14}, {cm119_d1_t, 3, 3, 7, 26}, {cm119_d2_t, 3, 1, 40, 29}, {cm119_d3_t, 3, 2, 21, 40}};
static const int cm120_d0_t[] = {0};
static const int cm120_d1_t[] = {1};
static const int cm120_d2_t[] = {2};
static const int cm120_d3_t[] = {3};
static const uint8_t cm120_pal[] = {56, 58, 85, 86, 87};
static const int cm120_rem[] = {0};
static const int cm120_trem[] = {0};
static const struct w9_door cm120_doors[] = {{cm120_d0_t, 1, 0, 9, 11}, {cm120_d1_t, 1, 0, 16, 11}, {cm120_d2_t, 1, 0, 23, 11}, {cm120_d3_t, 1, 2, 16, 25}};
static const int cm121_d0_t[] = {0, 1};
static const int cm121_d1_t[] = {2, 3};
static const int cm121_d2_t[] = {4, 5};
static const int cm121_d3_t[] = {6, 7};
static const uint8_t cm121_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm121_rem[] = {0};
static const int cm121_trem[] = {0};
static const struct w9_door cm121_doors[] = {{cm121_d0_t, 2, 0, 17, 25}, {cm121_d1_t, 2, 2, 29, 24}, {cm121_d2_t, 2, 2, 6, 25}, {cm121_d3_t, 2, 2, 17, 27}};
static const int cm122_d0_t[] = {4, 5};
static const int cm122_d1_t[] = {8, 9};
static const uint8_t cm122_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm122_rem[] = {0};
static const int cm122_trem[] = {0};
static const struct w9_door cm122_doors[] = {{cm122_d0_t, 2, 2, 16, 13}, {cm122_d1_t, 2, 0, 9, 24}};
static const int cm123_d0_t[] = {0, 1};
static const int cm123_d1_t[] = {6, 7};
static const uint8_t cm123_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm123_rem[] = {0};
static const int cm123_trem[] = {0};
static const struct w9_door cm123_doors[] = {{cm123_d0_t, 2, 0, 34, 11}, {cm123_d1_t, 2, 0, 44, 20}};
static const int cm124_d0_t[] = {2, 3};
static const int cm124_d1_t[] = {10, 11};
static const uint8_t cm124_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm124_rem[] = {0};
static const int cm124_trem[] = {0};
static const struct w9_door cm124_doors[] = {{cm124_d0_t, 2, 0, 55, 11}, {cm124_d1_t, 2, 2, 58, 31}};
static const int cm125_d0_t[] = {14, 15};
static const int cm125_d1_t[] = {18, 19};
static const uint8_t cm125_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm125_rem[] = {0};
static const int cm125_trem[] = {0};
static const struct w9_door cm125_doors[] = {{cm125_d0_t, 2, 0, 9, 44}, {cm125_d1_t, 2, 2, 8, 64}};
static const int cm126_d0_t[] = {12, 13};
static const int cm126_d1_t[] = {20, 21};
static const uint8_t cm126_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm126_rem[] = {0};
static const int cm126_trem[] = {0};
static const struct w9_door cm126_doors[] = {{cm126_d0_t, 2, 0, 30, 43}, {cm126_d1_t, 2, 2, 38, 65}};
static const int cm127_d0_t[] = {16, 17};
static const int cm127_d1_t[] = {22, 23};
static const uint8_t cm127_pal[] = {46, 47, 55, 74, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};
static const int cm127_rem[] = {0};
static const int cm127_trem[] = {0};
static const struct w9_door cm127_doors[] = {{cm127_d0_t, 2, 2, 61, 48}, {cm127_d1_t, 2, 2, 57, 65}};
static const int cm128_d0_t[] = {4, 5};
static const int cm128_d1_t[] = {6, 7, 8, 9};
static const uint8_t cm128_pal[] = {38, 41, 56, 58, 81, 82, 83, 84, 85};
static const int cm128_rem[] = {0};
static const int cm128_trem[] = {0};
static const struct w9_door cm128_doors[] = {{cm128_d0_t, 2, 0, 22, 33}, {cm128_d1_t, 4, 2, 22, 77}};
static const int cm129_d0_t[] = {0, 3};
static const int cm129_d1_t[] = {1, 2};
static const int cm129_d2_t[] = {4, 5};
static const uint8_t cm129_pal[] = {0};
static const int cm129_rem[] = {0};
static const int cm129_trem[] = {0};
static const struct w9_door cm129_doors[] = {{cm129_d0_t, 2, 1, 50, 27}, {cm129_d1_t, 2, 0, 23, 29}, {cm129_d2_t, 2, 2, 24, 54}};
static const int cstart_d0_t[] = {0, 1};
static const uint8_t cstart_pal[] = {0};
static const int cstart_rem[] = {0, 1, 2};
static const int cstart_trem[] = {2, 3};
static const struct w9_door cstart_doors[] = {{cstart_d0_t, 2, 1, 45, 51}};
static const int ccred_d0_t[] = {0};
static const uint8_t ccred_pal[] = {0};
static const int ccred_rem[] = {0};
static const int ccred_trem[] = {0};
static const struct w9_door ccred_doors[] = {{ccred_d0_t, 0, 0, 8, 6}};
const struct w9_vmap w9_chaos_maps[] = {
  {17, cm0_doors, 2, 0, cm0_pal, 4, 0, cm0_rem, 0, 0, cm0_trem, 0},
  {28, cm1_doors, 2, 0, cm1_pal, 0, 1, cm1_rem, 0, 1, cm1_trem, 0},
  {31, cm2_doors, 2, 0, cm2_pal, 0, 1, cm2_rem, 0, 0, cm2_trem, 0},
  {40, cm3_doors, 2, 0, cm3_pal, 4, 1, cm3_rem, 0, 0, cm3_trem, 0},
  {43, cm4_doors, 2, 0, cm4_pal, 3, 0, cm4_rem, 0, 0, cm4_trem, 0},
  {44, cm5_doors, 2, 0, cm5_pal, 0, 0, cm5_rem, 0, 1, cm5_trem, 0},
  {45, cm6_doors, 2, 0, cm6_pal, 0, 0, cm6_rem, 0, 1, cm6_trem, 0},
  {48, cm7_doors, 2, 0, cm7_pal, 17, 0, cm7_rem, 0, 1, cm7_trem, 0},
  {49, cm8_doors, 2, 0, cm8_pal, 4, 1, cm8_rem, 0, 0, cm8_trem, 0},
  {54, cm9_doors, 2, 0, cm9_pal, 4, 1, cm9_rem, 0, 0, cm9_trem, 0},
  {62, cm10_doors, 2, 0, cm10_pal, 0, 1, cm10_rem, 1, 0, cm10_trem, 0},
  {70, cm11_doors, 2, 0, cm11_pal, 0, 1, cm11_rem, 0, 0, cm11_trem, 0},
  {75, cm12_doors, 2, 0, cm12_pal, 0, 1, cm12_rem, 0, 1, cm12_trem, 0},
  {77, cm13_doors, 2, 0, cm13_pal, 2, 1, cm13_rem, 0, 0, cm13_trem, 0},
  {80, cm14_doors, 2, 0, cm14_pal, 2, 1, cm14_rem, 0, 0, cm14_trem, 0},
  {83, cm15_doors, 2, 0, cm15_pal, 2, 1, cm15_rem, 0, 1, cm15_trem, 0},
  {90, cm16_doors, 2, 0, cm16_pal, 2, 1, cm16_rem, 0, 0, cm16_trem, 0},
  {93, cm17_doors, 2, 0, cm17_pal, 0, 1, cm17_rem, 0, 0, cm17_trem, 0},
  {96, cm18_doors, 2, 0, cm18_pal, 0, 1, cm18_rem, 0, 0, cm18_trem, 0},
  {109, cm19_doors, 2, 0, cm19_pal, 1, 1, cm19_rem, 0, 1, cm19_trem, 0},
  {118, cm20_doors, 2, 0, cm20_pal, 17, 0, cm20_rem, 0, 1, cm20_trem, 0},
  {129, cm21_doors, 2, 0, cm21_pal, 1, 1, cm21_rem, 0, 0, cm21_trem, 1},
  {130, cm22_doors, 2, 0, cm22_pal, 1, 1, cm22_rem, 0, 1, cm22_trem, 1},
  {140, cm23_doors, 2, 0, cm23_pal, 17, 0, cm23_rem, 0, 1, cm23_trem, 0},
  {161, cm24_doors, 2, 1, cm24_pal, 5, 1, cm24_rem, 0, 1, cm24_trem, 0},
  {163, cm25_doors, 2, 1, cm25_pal, 5, 0, cm25_rem, 0, 0, cm25_trem, 0},
  {196, cm26_doors, 2, 0, cm26_pal, 9, 1, cm26_rem, 0, 0, cm26_trem, 0},
  {200, cm27_doors, 2, 0, cm27_pal, 3, 0, cm27_rem, 0, 1, cm27_trem, 0},
  {202, cm28_doors, 2, 0, cm28_pal, 3, 0, cm28_rem, 0, 1, cm28_trem, 0},
  {208, cm29_doors, 2, 0, cm29_pal, 3, 0, cm29_rem, 0, 1, cm29_trem, 0},
  {212, cm30_doors, 2, 0, cm30_pal, 4, 0, cm30_rem, 0, 1, cm30_trem, 0},
  {214, cm31_doors, 2, 0, cm31_pal, 4, 0, cm31_rem, 0, 1, cm31_trem, 0},
  {215, cm32_doors, 2, 0, cm32_pal, 3, 0, cm32_rem, 0, 1, cm32_trem, 0},
  {217, cm33_doors, 2, 0, cm33_pal, 3, 0, cm33_rem, 0, 1, cm33_trem, 0},
  {218, cm34_doors, 2, 0, cm34_pal, 3, 0, cm34_rem, 0, 1, cm34_trem, 0},
  {221, cm35_doors, 2, 0, cm35_pal, 3, 0, cm35_rem, 0, 1, cm35_trem, 0},
  {222, cm36_doors, 2, 1, cm36_pal, 4, 1, cm36_rem, 0, 1, cm36_trem, 0},
  {227, cm37_doors, 2, 0, cm37_pal, 3, 0, cm37_rem, 0, 1, cm37_trem, 0},
  {250, cm38_doors, 2, 0, cm38_pal, 0, 0, cm38_rem, 0, 0, cm38_trem, 0},
  {269, cm39_doors, 2, 0, cm39_pal, 17, 0, cm39_rem, 0, 0, cm39_trem, 0},
  {269, cm40_doors, 2, 0, cm40_pal, 17, 0, cm40_rem, 0, 0, cm40_trem, 0},
  {269, cm41_doors, 2, 0, cm41_pal, 17, 0, cm41_rem, 0, 0, cm41_trem, 0},
  {275, cm42_doors, 2, 0, cm42_pal, 4, 1, cm42_rem, 0, 1, cm42_trem, 0},
  {284, cm43_doors, 2, 0, cm43_pal, 4, 1, cm43_rem, 0, 0, cm43_trem, 0},
  {284, cm44_doors, 2, 0, cm44_pal, 4, 1, cm44_rem, 0, 0, cm44_trem, 0},
  {286, cm45_doors, 2, 0, cm45_pal, 4, 1, cm45_rem, 0, 0, cm45_trem, 0},
  {301, cm46_doors, 2, 0, cm46_pal, 5, 0, cm46_rem, 0, 1, cm46_trem, 0},
  {306, cm47_doors, 2, 0, cm47_pal, 5, 1, cm47_rem, 5, 1, cm47_trem, 0},
  {313, cm48_doors, 2, 0, cm48_pal, 17, 1, cm48_rem, 0, 0, cm48_trem, 0},
  {322, cm49_doors, 2, 0, cm49_pal, 4, 1, cm49_rem, 0, 0, cm49_trem, 0},
  {322, cm50_doors, 2, 0, cm50_pal, 4, 1, cm50_rem, 0, 0, cm50_trem, 0},
  {323, cm51_doors, 2, 0, cm51_pal, 4, 0, cm51_rem, 0, 0, cm51_trem, 0},
  {328, cm52_doors, 2, 0, cm52_pal, 17, 0, cm52_rem, 0, 0, cm52_trem, 0},
  {328, cm53_doors, 2, 0, cm53_pal, 17, 0, cm53_rem, 0, 0, cm53_trem, 0},
  {330, cm54_doors, 2, 0, cm54_pal, 17, 0, cm54_rem, 0, 0, cm54_trem, 0},
  {330, cm55_doors, 2, 0, cm55_pal, 17, 0, cm55_rem, 0, 0, cm55_trem, 0},
  {346, cm56_doors, 2, 0, cm56_pal, 17, 0, cm56_rem, 0, 0, cm56_trem, 0},
  {353, cm57_doors, 2, 0, cm57_pal, 5, 0, cm57_rem, 0, 0, cm57_trem, 0},
  {358, cm58_doors, 2, 0, cm58_pal, 5, 0, cm58_rem, 0, 0, cm58_trem, 0},
  {359, cm59_doors, 2, 0, cm59_pal, 5, 0, cm59_rem, 0, 0, cm59_trem, 0},
  {362, cm60_doors, 2, 0, cm60_pal, 5, 0, cm60_rem, 0, 0, cm60_trem, 0},
  {367, cm61_doors, 2, 0, cm61_pal, 4, 1, cm61_rem, 0, 0, cm61_trem, 0},
  {369, cm62_doors, 2, 0, cm62_pal, 17, 0, cm62_rem, 0, 0, cm62_trem, 0},
  {369, cm63_doors, 2, 0, cm63_pal, 17, 0, cm63_rem, 0, 0, cm63_trem, 0},
  {373, cm64_doors, 2, 0, cm64_pal, 17, 0, cm64_rem, 0, 1, cm64_trem, 0},
  {379, cm65_doors, 2, 0, cm65_pal, 5, 0, cm65_rem, 0, 1, cm65_trem, 0},
  {381, cm66_doors, 2, 0, cm66_pal, 5, 0, cm66_rem, 0, 1, cm66_trem, 0},
  {385, cm67_doors, 2, 0, cm67_pal, 5, 0, cm67_rem, 0, 0, cm67_trem, 0},
  {385, cm68_doors, 2, 0, cm68_pal, 5, 0, cm68_rem, 0, 0, cm68_trem, 0},
  {385, cm69_doors, 2, 0, cm69_pal, 5, 0, cm69_rem, 0, 0, cm69_trem, 0},
  {388, cm70_doors, 2, 0, cm70_pal, 5, 0, cm70_rem, 0, 0, cm70_trem, 0},
  {389, cm71_doors, 2, 0, cm71_pal, 5, 0, cm71_rem, 0, 1, cm71_trem, 0},
  {401, cm72_doors, 2, 1, cm72_pal, 5, 0, cm72_rem, 0, 0, cm72_trem, 0},
  {401, cm73_doors, 2, 1, cm73_pal, 5, 0, cm73_rem, 0, 0, cm73_trem, 0},
  {401, cm74_doors, 2, 1, cm74_pal, 5, 0, cm74_rem, 0, 0, cm74_trem, 0},
  {401, cm75_doors, 2, 1, cm75_pal, 5, 0, cm75_rem, 0, 0, cm75_trem, 0},
  {401, cm76_doors, 2, 1, cm76_pal, 5, 0, cm76_rem, 0, 0, cm76_trem, 0},
  {403, cm77_doors, 2, 1, cm77_pal, 5, 0, cm77_rem, 0, 0, cm77_trem, 0},
  {403, cm78_doors, 2, 1, cm78_pal, 5, 0, cm78_rem, 0, 0, cm78_trem, 0},
  {403, cm79_doors, 2, 1, cm79_pal, 5, 0, cm79_rem, 0, 0, cm79_trem, 0},
  {403, cm80_doors, 2, 1, cm80_pal, 5, 0, cm80_rem, 0, 0, cm80_trem, 0},
  {404, cm81_doors, 2, 1, cm81_pal, 5, 0, cm81_rem, 0, 0, cm81_trem, 0},
  {405, cm82_doors, 2, 1, cm82_pal, 5, 1, cm82_rem, 0, 1, cm82_trem, 0},
  {406, cm83_doors, 2, 1, cm83_pal, 5, 0, cm83_rem, 0, 0, cm83_trem, 0},
  {406, cm84_doors, 2, 1, cm84_pal, 5, 0, cm84_rem, 0, 0, cm84_trem, 0},
  {406, cm85_doors, 2, 1, cm85_pal, 5, 0, cm85_rem, 0, 0, cm85_trem, 0},
  {406, cm86_doors, 2, 1, cm86_pal, 5, 0, cm86_rem, 0, 0, cm86_trem, 0},
  {406, cm87_doors, 2, 1, cm87_pal, 5, 0, cm87_rem, 0, 0, cm87_trem, 0},
  {406, cm88_doors, 2, 1, cm88_pal, 5, 0, cm88_rem, 0, 0, cm88_trem, 0},
  {433, cm89_doors, 2, 1, cm89_pal, 0, 1, cm89_rem, 0, 0, cm89_trem, 0},
  {436, cm90_doors, 2, 1, cm90_pal, 0, 1, cm90_rem, 0, 1, cm90_trem, 0},
  {23, cm91_doors, 3, 0, cm91_pal, 8, 1, cm91_rem, 0, 0, cm91_trem, 1},
  {27, cm92_doors, 3, 0, cm92_pal, 8, 1, cm92_rem, 1, 1, cm92_trem, 0},
  {29, cm93_doors, 3, 0, cm93_pal, 0, 0, cm93_rem, 0, 0, cm93_trem, 0},
  {39, cm94_doors, 3, 0, cm94_pal, 4, 1, cm94_rem, 0, 0, cm94_trem, 0},
  {61, cm95_doors, 3, 0, cm95_pal, 0, 1, cm95_rem, 0, 1, cm95_trem, 0},
  {76, cm96_doors, 3, 0, cm96_pal, 2, 1, cm96_rem, 0, 0, cm96_trem, 0},
  {81, cm97_doors, 3, 0, cm97_pal, 2, 1, cm97_rem, 0, 0, cm97_trem, 1},
  {86, cm98_doors, 3, 0, cm98_pal, 0, 1, cm98_rem, 0, 0, cm98_trem, 0},
  {92, cm99_doors, 3, 0, cm99_pal, 0, 1, cm99_rem, 0, 0, cm99_trem, 0},
  {94, cm100_doors, 3, 0, cm100_pal, 0, 1, cm100_rem, 0, 0, cm100_trem, 0},
  {95, cm101_doors, 3, 0, cm101_pal, 0, 1, cm101_rem, 0, 0, cm101_trem, 0},
  {199, cm102_doors, 3, 0, cm102_pal, 3, 0, cm102_rem, 0, 1, cm102_trem, 0},
  {203, cm103_doors, 3, 0, cm103_pal, 3, 0, cm103_rem, 0, 1, cm103_trem, 0},
  {205, cm104_doors, 3, 0, cm104_pal, 3, 0, cm104_rem, 0, 1, cm104_trem, 0},
  {209, cm105_doors, 3, 0, cm105_pal, 3, 0, cm105_rem, 0, 1, cm105_trem, 0},
  {219, cm106_doors, 3, 0, cm106_pal, 3, 0, cm106_rem, 0, 1, cm106_trem, 0},
  {285, cm107_doors, 3, 0, cm107_pal, 4, 1, cm107_rem, 0, 0, cm107_trem, 0},
  {346, cm108_doors, 3, 0, cm108_pal, 17, 0, cm108_rem, 0, 0, cm108_trem, 0},
  {354, cm109_doors, 3, 0, cm109_pal, 5, 0, cm109_rem, 0, 0, cm109_trem, 0},
  {369, cm110_doors, 3, 0, cm110_pal, 17, 0, cm110_rem, 0, 0, cm110_trem, 0},
  {384, cm111_doors, 3, 0, cm111_pal, 5, 0, cm111_rem, 2, 1, cm111_trem, 0},
  {401, cm112_doors, 3, 1, cm112_pal, 5, 0, cm112_rem, 0, 0, cm112_trem, 0},
  {412, cm113_doors, 3, 0, cm113_pal, 17, 1, cm113_rem, 0, 0, cm113_trem, 0},
  {413, cm114_doors, 3, 0, cm114_pal, 17, 0, cm114_rem, 0, 0, cm114_trem, 0},
  {25, cm115_doors, 4, 0, cm115_pal, 8, 1, cm115_rem, 0, 0, cm115_trem, 1},
  {53, cm116_doors, 4, 0, cm116_pal, 4, 1, cm116_rem, 1, 1, cm116_trem, 0},
  {85, cm117_doors, 4, 0, cm117_pal, 0, 1, cm117_rem, 0, 1, cm117_trem, 0},
  {85, cm118_doors, 4, 0, cm118_pal, 0, 1, cm118_rem, 0, 1, cm118_trem, 0},
  {88, cm119_doors, 4, 0, cm119_pal, 0, 1, cm119_rem, 0, 1, cm119_trem, 0},
  {304, cm120_doors, 4, 0, cm120_pal, 5, 0, cm120_rem, 0, 1, cm120_trem, 0},
  {329, cm121_doors, 4, 0, cm121_pal, 17, 0, cm121_rem, 0, 1, cm121_trem, 0},
  {20, cm122_doors, 2, 0, cm122_pal, 17, 1, cm122_rem, 0, 0, cm122_trem, 0},
  {20, cm123_doors, 2, 0, cm123_pal, 17, 1, cm123_rem, 0, 0, cm123_trem, 0},
  {20, cm124_doors, 2, 0, cm124_pal, 17, 1, cm124_rem, 0, 0, cm124_trem, 0},
  {20, cm125_doors, 2, 0, cm125_pal, 17, 1, cm125_rem, 0, 0, cm125_trem, 0},
  {20, cm126_doors, 2, 0, cm126_pal, 17, 1, cm126_rem, 0, 0, cm126_trem, 0},
  {20, cm127_doors, 2, 0, cm127_pal, 17, 1, cm127_rem, 0, 0, cm127_trem, 0},
  {21, cm128_doors, 2, 0, cm128_pal, 9, 1, cm128_rem, 0, 0, cm128_trem, 0},
  {136, cm129_doors, 3, 0, cm129_pal, 0, 1, cm129_rem, 1, 0, cm129_trem, 0}
};
const int w9_chaos_map_count = 130;
const struct w9_vmap w9_chaos_start_map = {128, cstart_doors, 1, 0, cstart_pal, 0, 0, cstart_rem, 3, 0, cstart_trem, 2};
const struct w9_vmap w9_chaos_credits_map = {180, ccred_doors, 1, 0, ccred_pal, 0, 0, ccred_rem, 0, 0, ccred_trem, 0};
static const uint8_t bm0_show[] = {0};
static const uint8_t bm0_hide[] = {0};
static const uint8_t bm1_show[] = {0};
static const uint8_t bm1_hide[] = {0};
static const uint8_t bm2_show[] = {0};
static const uint8_t bm2_hide[] = {1, 2};
static const uint8_t bm3_show[] = {0};
static const uint8_t bm3_hide[] = {0};
static const uint8_t bm4_show[] = {0};
static const uint8_t bm4_hide[] = {0};
static const uint8_t bm5_show[] = {0};
static const uint8_t bm5_hide[] = {1, 2};
static const uint8_t bm6_show[] = {0};
static const uint8_t bm6_hide[] = {0};
static const uint8_t bm7_show[] = {0};
static const uint8_t bm7_hide[] = {0};
static const uint8_t bm8_show[] = {0};
static const uint8_t bm8_hide[] = {1, 2, 3, 4, 5, 6};
static const uint8_t bm9_show[] = {0};
static const uint8_t bm9_hide[] = {0};
static const uint8_t bm10_show[] = {0};
static const uint8_t bm10_hide[] = {0};
static const uint8_t bm11_show[] = {2};
static const uint8_t bm11_hide[] = {0, 1};
static const uint8_t bm12_show[] = {0};
static const uint8_t bm12_hide[] = {0};
static const uint8_t bm13_show[] = {0};
static const uint8_t bm13_hide[] = {1};
static const uint8_t bm14_show[] = {0};
static const uint8_t bm14_hide[] = {0};
static const uint8_t bm15_show[] = {0};
static const uint8_t bm15_hide[] = {0};
static const uint8_t bm16_show[] = {0};
static const uint8_t bm16_hide[] = {0};
static const uint8_t bm17_show[] = {0};
static const uint8_t bm17_hide[] = {0};
static const uint8_t bm18_show[] = {0};
static const uint8_t bm18_hide[] = {0};
static const uint8_t bm19_show[] = {3};
static const uint8_t bm19_hide[] = {0, 1, 2};
static const uint8_t bm20_show[] = {0};
static const uint8_t bm20_hide[] = {0};
static const uint8_t bm21_show[] = {2};
static const uint8_t bm21_hide[] = {0, 1};
static const uint8_t bm22_show[] = {3};
static const uint8_t bm22_hide[] = {0, 1, 2};
static const uint8_t bm23_show[] = {0};
static const uint8_t bm23_hide[] = {0};
static const uint8_t bm24_show[] = {0};
static const uint8_t bm24_hide[] = {0};
static const uint8_t bm25_show[] = {0};
static const uint8_t bm25_hide[] = {0};
static const uint8_t bm26_show[] = {0};
static const uint8_t bm26_hide[] = {0};
static const uint8_t bm27_show[] = {0};
static const uint8_t bm27_hide[] = {0};
static const uint8_t bm28_show[] = {0};
static const uint8_t bm28_hide[] = {0};
static const uint8_t bm29_show[] = {0};
static const uint8_t bm29_hide[] = {0};
static const uint8_t bm30_show[] = {0};
static const uint8_t bm30_hide[] = {0};
static const uint8_t bm31_show[] = {2};
static const uint8_t bm31_hide[] = {0, 1};
static const uint8_t bm32_show[] = {0};
static const uint8_t bm32_hide[] = {0};
static const uint8_t bm33_show[] = {0};
static const uint8_t bm33_hide[] = {0};
static const uint8_t bm34_show[] = {0};
static const uint8_t bm34_hide[] = {0};
static const uint8_t bmf_show[] = {0};
static const uint8_t bmf_hide[] = {0};
const struct w9_bmap w9_chaos_boss_maps[] = {
  {0x57, 260, 17, 19, 0, bm0_show, 1, bm0_hide, 0},
  {0x58, 166, 13, 19, 0, bm1_show, 1, bm1_hide, 0},
  {0x59, 290, 25, 42, 0, bm2_show, 1, bm2_hide, 2},
  {0x5A, 347, 16, 24, 0, bm3_show, 1, bm3_hide, 0},
  {0x5B, 310, 13, 23, 0, bm4_show, 1, bm4_hide, 0},
  {0x5C, 143, 16, 46, 0, bm5_show, 1, bm5_hide, 2},
  {0x5D, 60, 18, 22, 0, bm6_show, 1, bm6_hide, 0},
  {0x5E, 364, 9, 19, 0, bm7_show, 1, bm7_hide, 0},
  {0x5F, 430, 32, 28, 0, bm8_show, 1, bm8_hide, 6},
  {0x60, 340, 19, 15, 0, bm9_show, 1, bm9_hide, 0},
  {0x61, 407, 13, 19, 0, bm10_show, 1, bm10_hide, 0},
  {0x62, 408, 14, 26, 1, bm11_show, 1, bm11_hide, 2},
  {0x63, 377, 20, 12, 0, bm12_show, 1, bm12_hide, 0},
  {0x64, 100, 9, 19, 0, bm13_show, 1, bm13_hide, 1},
  {0x65, 317, 19, 26, 0, bm14_show, 1, bm14_hide, 0},
  {0x66, 119, 15, 25, 0, bm15_show, 1, bm15_hide, 0},
  {0x68, 79, 11, 10, 0, bm16_show, 1, bm16_hide, 0},
  {0x69, 325, 22, 23, 1, bm17_show, 1, bm17_hide, 0},
  {0x6A, 489, 11, 16, 0, bm18_show, 1, bm18_hide, 0},
  {0x6D, 434, 15, 27, 1, bm19_show, 1, bm19_hide, 3},
  {0x6E, 238, 9, 16, 0, bm20_show, 1, bm20_hide, 0},
  {0x6F, 418, 11, 24, 0, bm21_show, 1, bm21_hide, 2},
  {0x70, 46, 11, 15, 0, bm22_show, 1, bm22_hide, 3},
  {0x71, 277, 16, 17, 0, bm23_show, 1, bm23_hide, 0},
  {0x72, 237, 6, 11, 0, bm24_show, 1, bm24_hide, 0},
  {0x73, 230, 12, 23, 0, bm25_show, 1, bm25_hide, 0},
  {0x74, 495, 16, 25, 0, bm26_show, 1, bm26_hide, 0},
  {0x75, 231, 20, 23, 0, bm27_show, 1, bm27_hide, 0},
  {0x76, 213, 19, 29, 0, bm28_show, 1, bm28_hide, 0},
  {0x77, 232, 13, 20, 0, bm29_show, 1, bm29_hide, 0},
  {0x78, 244, 17, 47, 0, bm30_show, 1, bm30_hide, 0},
  {0x79, 246, 18, 29, 0, bm31_show, 1, bm31_hide, 2},
  {0x7B, 204, 18, 27, 0, bm32_show, 1, bm32_hide, 0},
  {0x7C, 488, 11, 16, 0, bm33_show, 1, bm33_hide, 0},
  {0x7D, 226, 24, 40, 0, bm34_show, 1, bm34_hide, 0}
};
const int w9_chaos_boss_map_count = 35;
const struct w9_bmap w9_chaos_final_boss_map = {0x7F, 253, 18, 22, 0, bmf_show, 1, bmf_hide, 0};

/* difficulty_chaos_{casual,hard,reallyhard}.properties; same keys and order as the boss rush table */
const struct sr_w8_diff_row w9_chaos_diff_rows[] = {
  {"enemyLevel_base", {1.0, 1.0, 1.0}},
  {"enemyLevel_growth", {2.0, 2.0, 2.0}},
  {"enemyLevel_exp", {1.0, 1.0, 1.0}},
  {"enemyStr_base", {6.0, 7.0, 10.0}},
  {"enemyStr_growth", {10.0, 12.0, 15.0}},
  {"enemyStr_exp", {0.5, 0.5, 0.8}},
  {"enemyAgi_base", {6.0, 7.0, 4.0}},
  {"enemyAgi_growth", {2.0, 8.0, 4.0}},
  {"enemyAgi_exp", {1.0, 0.5, 1.0}},
  {"enemyInt_base", {4.0, 6.0, 10.0}},
  {"enemyInt_growth", {1.2, 1.2, 1.2}},
  {"enemyInt_exp", {1.2, 1.2, 1.2}},
  {"enemyWis_base", {4.0, 6.0, 10.0}},
  {"enemyWis_growth", {2.0, 2.0, 2.0}},
  {"enemyWis_exp", {1.0, 1.0, 1.0}},
  {"enemyEvade_base", {4.0, 4.0, 4.0}},
  {"enemyEvade_growth", {0.25, 0.5, 1.0}},
  {"enemyEvade_exp", {1.0, 1.0, 1.0}},
  {"enemyMevade_base", {4.0, 4.0, 4.0}},
  {"enemyMevade_growth", {1.0, 2.0, 4.0}},
  {"enemyMevade_exp", {1.0, 1.0, 1.0}},
  {"enemyHp_base", {30.0, 50.0, 100.0}},
  {"enemyHp_growth", {20.0, 30.0, 40.0}},
  {"enemyHp_exp", {0.8, 0.8, 0.8}},
  {"enemyMp_base", {4.0, 4.0, 4.0}},
  {"enemyMp_growth", {4.0, 4.0, 4.0}},
  {"enemyMp_exp", {1.0, 1.0, 1.0}},
  {"tonpoleMp", {6.0, 8.0, 10.0}},
  {"enemyDef_base", {10.0, 12.0, 12.0}},
  {"enemyDef_growth", {6.0, 7.0, 8.0}},
  {"enemyDef_exp", {1.0, 1.0, 1.0}},
  {"enemyMdef_base", {10.0, 12.0, 15.0}},
  {"enemyMdef_growth", {2.0, 2.4, 2.8}},
  {"enemyMdef_exp", {1.0, 1.2, 1.4}},
  {"enemyExp_base", {32.0, 32.0, 32.0}},
  {"enemyExp_growth", {24.0, 24.0, 24.0}},
  {"enemyExp_exp", {1.2, 1.2, 1.2}},
  {"enemyGold_base", {64.0, 64.0, 64.0}},
  {"enemyGold_growth", {32.0, 32.0, 32.0}},
  {"enemyGold_exp", {1.0, 1.0, 1.0}},
  {"enemyWeaponLev_base", {1.0, 1.0, 2.0}},
  {"enemyWeaponLev_growth", {0.0, 0.2, 0.2}},
  {"enemyWeaponLev_exp", {1.0, 1.0, 1.0}},
  {"enemyMagicLev_base", {0.0, 1.0, 2.0}},
  {"enemyMagicLev_growth", {0.0, 0.2, 0.3}},
  {"enemyMagicLev_exp", {1.0, 1.0, 1.0}},
  {"enemyWeaponDamage_base", {12.0, 16.0, 20.0}},
  {"enemyWeaponDamage_growth", {1.0, 1.0, 1.5}},
  {"enemyWeaponDamage_exp", {1.0, 1.0, 1.0}},
  {"manaPower_base", {0.0, 0.0, 0.0}},
  {"manaPower_growth", {0.0, 0.0, 0.0}},
  {"manaPower_exp", {0.0, 0.0, 0.0}},
  {"bossStatMultiplier", {1.2, 1.2, 1.4}},
  {"bossExpMultiplier", {4.0, 3.0, 2.0}},
  {"bossGoldMultiplier", {4.0, 3.0, 2.0}},
  {"bossHpMultiplier", {5.0, 7.0, 12.0}},
  {"startingGold", {5000.0, 1000.0, 0.0}},
  {"startingConsumables", {3.0, 2.0, 1.0}},
  {"restoreCost", {100.0, 400.0, 600.0}},
  {"numPhannas", {2.0, 1.0, 1.0}},
  {"numElements", {2.0, 1.0, 1.0}},
  {"numWatts", {1.0, 1.0, 1.0}},
  {"continueCost", {1000.0, 2000.0, 4000.0}},
  {"startingLevel", {1.0, 1.0, 1.0}},
  {"restoresLimit", {0.0, 0.0, 0.0}},
};
const int w9_chaos_diff_row_count = 65;
