// Headless command line front end for the Moppu/SecretOfManaRandomizer sources (LGPL-2.1, v1.49).
// Compiled together with the upstream sources by somrando-headless.csproj; this file is original work of this repository.
using System;
using System.Collections.Generic;
using System.IO;

public static class Headless
{
    static int Usage()
    {
        Console.Error.WriteLine("usage: somrando --mode ancientcave|open|rando|bossrush|chaos --seed STR --src ROM --dst OUT");
        Console.Error.WriteLine("                [--difficulty casual|hard|reallyhard] [--set key=value]... [--workdir DIR] [--list-options] [--flat-difficulty]");
        return 2;
    }

    public static int Main(string[] args)
    {
        string mode = "ancientcave", seed = "1", src = null, dst = null, diff = "casual", work = null;
        bool list = false, flat = false;
        var sets = new Dictionary<string, string>();
        for (int i = 0; i < args.Length; i++)
        {
            string a = args[i];
            string Next() { if (i + 1 >= args.Length) throw new Exception("missing value for " + a); return args[++i]; }
            switch (a)
            {
                case "--mode": mode = Next(); break;
                case "--seed": seed = Next(); break;
                case "--src": src = Next(); break;
                case "--dst": dst = Next(); break;
                case "--difficulty": diff = Next(); break;
                case "--workdir": work = Next(); break;
                case "--list-options": list = true; break;
                case "--flat-difficulty": flat = true; break;
                case "--set":
                    {
                        string kv = Next(); int eq = kv.IndexOf('=');
                        if (eq <= 0) return Usage();
                        sets[kv.Substring(0, eq)] = kv.Substring(eq + 1); break;
                    }
                default: return Usage();
            }
        }
        if (list) { SomRandoApi.ListOptions(mode, Console.Out); return 0; }
        if (src == null || dst == null) return Usage();
        src = Path.GetFullPath(src); dst = Path.GetFullPath(dst);
        if (work != null) { Directory.CreateDirectory(work); Directory.SetCurrentDirectory(work); }
        string err = SomRandoApi.Generate(mode, seed, src, dst, diff, sets, flat);
        if (err != null) { Console.Error.WriteLine("error: " + err); return 1; }
        Console.WriteLine("done " + dst);
        return 0;
    }
}
