// Mode-independent entry point over the upstream generator (Moppu/SecretOfManaRandomizer v1.49, LGPL-2.1).
// Original glue code of this repository; compiled together with the upstream sources.
using System;
using System.Collections.Generic;
using System.IO;
using SoMRandomizer.config;
using SoMRandomizer.config.settings;
using SoMRandomizer.processing.ancientcave;
using SoMRandomizer.processing.bossrush;
using SoMRandomizer.processing.chaos;
using SoMRandomizer.processing.common;
using SoMRandomizer.processing.openworld;
using SoMRandomizer.processing.vanillarando;

public static class SomRandoApi
{
    public const string Version = "1.49";

    public static void ListOptions(string mode, TextWriter w)
    {
        var common = new CommonSettings();
        var rs = Make(mode, common, out _);
        foreach (var k in common.keysStartingWith("")) w.WriteLine("common " + k + "=" + common.get(k));
        foreach (var k in rs.keysStartingWith("")) w.WriteLine(mode + " " + k + "=" + rs.get(k));
    }

    static RandoSettings Make(string mode, CommonSettings common, out RomGenerator gen)
    {
        switch (mode)
        {
            case "ancientcave": gen = new AncientCaveGenerator(); return new AncientCaveSettings(common);
            case "open": gen = new OpenWorldGenerator(); return new OpenWorldSettings(common);
            case "rando": gen = new VanillaRandoGenerator(); return new VanillaRandoSettings(common);
            case "bossrush": gen = new BossRushGenerator(); return new BossRushSettings(common);
            case "chaos": gen = new ChaosGenerator(); return new ChaosSettings(common);
            default: throw new ArgumentException("unknown mode " + mode);
        }
    }

    // Returns null on success, otherwise the error text. Writes dst (4 MiB). Logs go to the current directory.
    public static string Generate(string mode, string seed, string src, string dst, string difficulty,
                                  Dictionary<string, string> sets, bool flatDifficulty)
    {
        try
        {
            var common = new CommonSettings();
            RomGenerator gen;
            RandoSettings rs = Make(mode, common, out gen);
            common.set(CommonSettings.PROPERTYNAME_MODE, mode);
            common.set(CommonSettings.PROPERTYNAME_VERSION, RomGenerator.VERSION_NUMBER);
            if (flatDifficulty)
            {
                // Reproduces the earlier ancientcave_gen configuration (every difficulty field "1"), for byte comparisons only.
                foreach (var f in typeof(DifficultySettings).GetFields())
                    if (f.IsStatic && f.FieldType == typeof(string) && f.Name != "DIFFICULTY_PROPERTY_PREFIX")
                    {
                        string k = (string)f.GetValue(null);
                        string v = (k == "startingGold" || k == "restoreCost" || k == "continueCost") ? "100" : (k == "numWatts" || k == "numPhannas" || k == "numElements") ? "3" : "1";
                        try { rs.set(DifficultySettings.DIFFICULTY_PROPERTY_PREFIX + k, v); } catch (Exception) { }
                    }
            }
            else if (mode == "ancientcave" || mode == "bossrush" || mode == "chaos")
            {
                // The old modes read their difficulty preset exactly as the GUI does (MainForm.generate).
                string prefix = mode == "ancientcave" ? "" : mode + "_";
                var d = new DifficultySettings();
                d.readFromResourceFile("difficulty_" + prefix + difficulty + ".properties", null);
                foreach (string key in DifficultySettings.displayStrings.Keys)
                {
                    string k = DifficultySettings.DIFFICULTY_PROPERTY_PREFIX + DifficultySettings.displayStrings[key];
                    if (DifficultySettings.isGrowthValue(key))
                    {
                        var v = d.getGrowthValue(key);
                        common.set(k + "_base", v.baseValue.ToString());
                        common.set(k + "_growth", v.growthValue.ToString());
                        common.set(k + "_exp", v.exponent.ToString());
                    }
                    else common.set(k, d.getDoubleValue(key).ToString());
                }
                if (mode == "ancientcave") rs.set(AncientCaveSettings.PROPERTYNAME_DIFFICULTY, difficulty);
            }
            if (sets != null) rs.processNewSettings(sets);
            RomGenerator.initGeneration(src, dst, seed,
                new Dictionary<string, RomGenerator> { { mode, gen } }, common,
                new Dictionary<string, RandoSettings> { { mode, rs } });
            return null;
        }
        catch (Exception e)
        {
            return Environment.GetEnvironmentVariable("SOMRANDO_DEBUG") != null ? e.ToString() : e.Message;
        }
    }
}
