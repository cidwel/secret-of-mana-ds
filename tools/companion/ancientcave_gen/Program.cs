using System;
using System.Collections.Generic;
using SoMRandomizer.config.settings;
using SoMRandomizer.processing.common;
using SoMRandomizer.processing.ancientcave;
class P {
 static System.Reflection.Assembly R(object o, ResolveEventArgs e){
  var ex=typeof(SoMRandomizer.processing.common.RomGenerator).Assembly; var n=new System.Reflection.AssemblyName(e.Name);
  using (var st=ex.GetManifestResourceStream(n.Name+".dll")){ if(st==null) return null; var b=new byte[st.Length]; st.Read(b,0,b.Length); return System.Reflection.Assembly.Load(b);} }
 static void Main(string[] a) {
  AppDomain.CurrentDomain.AssemblyResolve += R;
  var common = new CommonSettings();
  var ac = new AncientCaveSettings(common);
  common.set(CommonSettings.PROPERTYNAME_MODE, AncientCaveSettings.MODE_KEY);
  common.set(CommonSettings.PROPERTYNAME_VERSION, RomGenerator.VERSION_NUMBER);
  foreach (var f in typeof(SoMRandomizer.config.DifficultySettings).GetFields())
    if (f.IsStatic && f.FieldType==typeof(string) && f.Name!="DIFFICULTY_PROPERTY_PREFIX") {
      string k=(string)f.GetValue(null); string v = (k=="startingGold"||k=="restoreCost"||k=="continueCost")?"100":(k=="numWatts"||k=="numPhannas"||k=="numElements")?"3":"1";
      try { ac.set(SoMRandomizer.config.DifficultySettings.DIFFICULTY_PROPERTY_PREFIX+k, v); } catch(Exception e){ Console.WriteLine("skip "+k+" "+e.Message);} }
  ac.set(AncientCaveSettings.PROPERTYNAME_RANDOM_MUSIC, "false");
  var gens = new Dictionary<string, RomGenerator>{{AncientCaveSettings.MODE_KEY, new AncientCaveGenerator()}};
  var sets = new Dictionary<string, RandoSettings>{{AncientCaveSettings.MODE_KEY, ac}};
  RomGenerator.initGeneration(a[0], a[1], a[2], gens, common, sets);
  Console.WriteLine("done");
}}
