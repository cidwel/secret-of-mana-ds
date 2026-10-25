// Prints .NET reference values for the libsomrando unit tests.  Original glue code of this repository (LGPL-2.1+).
//   OracleVectors rng            seeds -> hash code, raw/bounded/double streams
//   OracleVectors pow <props>    Math.Pow sweeps for every reachable call site (props = upstream Resources folder)
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using SoMRandomizer.util.rng;

public static class OracleVectors
{
    static string H(double d) => BitConverter.DoubleToInt64Bits(d).ToString("x16");

    static int Rng()
    {
        string[] seeds = { "1", "0", "42", "12345", "99999999", "-5", "", "a", "abcdefg", "Secret of Mana", "café", "日本語",
                           "emoji\U0001F600x", "2147483647", "00042", "seed with spaces", "x_cosmetic", "1_cosmetic" };
        var sb = new StringBuilder();
        foreach (string s in seeds)
        {
            int h = HashcodeUtil.GetDeterministicHashCode(s);
            var r = new DotNet110Random(h);
            sb.Append("seed b:").Append(Convert.ToBase64String(Encoding.UTF8.GetBytes(s))).Append(' ').Append(h).Append('\n');
            sb.Append("next"); for (int i = 0; i < 200; i++) sb.Append(' ').Append(r.Next()); sb.Append('\n');
            var r2 = new DotNet110Random(h);
            sb.Append("nextmax8"); for (int i = 0; i < 100; i++) sb.Append(' ').Append(r2.Next(8)); sb.Append('\n');
            sb.Append("nextmax1000003"); for (int i = 0; i < 100; i++) sb.Append(' ').Append(r2.Next(1000003)); sb.Append('\n');
            sb.Append("nextrange"); for (int i = 0; i < 100; i++) sb.Append(' ').Append(r2.Next(-50, 77)); sb.Append('\n');
            sb.Append("nextrangebig"); for (int i = 0; i < 40; i++) sb.Append(' ').Append(r2.Next(int.MinValue, int.MaxValue)); sb.Append('\n');
            sb.Append("nextrangeedge"); for (int i = 0; i < 40; i++) sb.Append(' ').Append(r2.Next(int.MinValue + 5, int.MaxValue - 5)); sb.Append('\n');
            sb.Append("double"); for (int i = 0; i < 100; i++) sb.Append(' ').Append(H(r2.NextDouble())); sb.Append('\n');
            var b = new byte[64]; r2.NextBytes(b);
            sb.Append("bytes"); foreach (byte x in b) sb.Append(' ').Append(x); sb.Append('\n');
            // the generator derives a second stream and, in race mode, re-seeds from Next()
            int hc = HashcodeUtil.GetDeterministicHashCode(s + "_cosmetic");
            var rc = new DotNet110Random(hc);
            sb.Append("cosmetic ").Append(hc); for (int i = 0; i < 20; i++) sb.Append(' ').Append(rc.Next()); sb.Append('\n');
            var rr = new DotNet110Random(new DotNet110Random(h).Next());
            sb.Append("race"); for (int i = 0; i < 20; i++) sb.Append(' ').Append(rr.Next()); sb.Append('\n');
        }
        // direct integer seeds, including the Int32.MinValue special case in the constructor
        foreach (int sd in new[] { 0, 1, -1, 7, int.MaxValue, int.MinValue, int.MinValue + 1, 161803398, -161803398 })
        {
            var r = new DotNet110Random(sd);
            sb.Append("iseed ").Append(sd); for (int i = 0; i < 120; i++) sb.Append(' ').Append(r.Next()); sb.Append('\n');
        }
        Console.Out.Write(sb.ToString());
        return 0;
    }

    // line: site|a|b|c|d|formula|powbits|final  (a,b,c,d as hex bits of doubles; formula 0 = (int)p, 1 = Math.Round(c+p*d), 2 = (int)(c+p*d))
    static void Line(string site, double a, double b, double c, double d, int formula)
    {
        double p = Math.Pow(a, b);
        long fin;
        if (formula == 0) fin = (int)p;
        else if (formula == 1) fin = (long)Math.Round(c + p * d);
        else fin = (int)(c + p * d);
        Console.Out.Write(site + "|" + H(a) + "|" + H(b) + "|" + H(c) + "|" + H(d) + "|" + formula + "|" + H(p) + "|" + fin + "\n");
    }

    static int Pow(string props)
    {
        // SomLz77.encodeSomLz77: (int)Math.Pow(2, windowSize + 2) + 2 and (int)Math.Pow(2, 13 - windowSize)
        for (int ws = 0; ws <= 13; ws++) { Line("lz77len", 2, ws + 2, 0, 0, 0); Line("lz77dist", 2, 13 - ws, 0, 0, 0); }
        // WeaponRandomizer: Math.Round(5 + Math.Pow(weaponNum + 1, 1.25) * 8)
        for (int n = 1; n <= 32; n++) Line("weapon", n, 1.25, 5, 8, 1);
        // DefenseRefactor: Math.Pow(((501) - def) / (double)500, 1.5 | 1.1), table value (byte)(min + ratio * (63 - min))
        for (int i = 0; i < 1000; i++)
        {
            int def = Math.Min(i, 500);
            double x = ((500 + 1) - def) / (double)500;
            Line("defense15", x, 1.5, 1, 62, 2);
            Line("defense11", x, 1.1, 4, 59, 2);
        }
        // EnemiesAtYourLevel: Math.Pow(lev, exponent), exponent 1.0 or 1.1, lev 1..99
        for (int lev = 1; lev <= 99; lev++) { Line("atlevel10", lev, 1.0, 0, 0, 0); Line("atlevel11", lev, 1.1, 0, 0, 0); }
        // EnemyChanges.writeStatTable and AncientCaveEventMaker: (int)(base + Math.Pow(i, exp) * growth) from the shipped difficulty files
        var seen = new HashSet<string>();
        foreach (string f in Directory.GetFiles(props, "difficulty*.properties"))
        {
            var kv = new Dictionary<string, double>();
            foreach (string raw in File.ReadAllLines(f))
            {
                string ln = raw.Trim().TrimStart('﻿');
                int eq = ln.IndexOf('=');
                if (eq <= 0) continue;
                double v;
                if (double.TryParse(ln.Substring(eq + 1), NumberStyles.Float, CultureInfo.InvariantCulture, out v)) kv[ln.Substring(0, eq)] = v;
            }
            foreach (var e in kv)
            {
                if (!e.Key.EndsWith("_exp")) continue;
                string stem = e.Key.Substring(0, e.Key.Length - 4);
                double bs, gr;
                if (!kv.TryGetValue(stem + "_base", out bs) || !kv.TryGetValue(stem + "_growth", out gr)) continue;
                foreach (double scale in new[] { 1.0, 0.5, 1.5 })
                {
                    string key = stem + "/" + bs + "/" + gr + "/" + e.Value + "/" + scale;
                    if (!seen.Add(key)) continue;
                    for (int i = 0; i <= 100; i++) Line("table", i, e.Value, bs, gr * scale, 2);
                }
            }
        }
        // coarse grid of exponents (custom difficulty values are free-form doubles)
        for (int k = 0; k <= 60; k++)
            for (int i = 0; i <= 100; i++) Line("grid", i, k * 0.05, 7, 12.0, 2);
        return 0;
    }

    public static int Main(string[] args)
    {
        if (args.Length >= 1 && args[0] == "rng") return Rng();
        if (args.Length >= 2 && args[0] == "pow") return Pow(args[1]);
        Console.Error.WriteLine("usage: OracleVectors rng | pow <Resources folder>");
        return 2;
    }
}
