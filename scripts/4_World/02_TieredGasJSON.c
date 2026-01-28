//---------------------------------------------------------------------------------------------------
// scripts/4_World/02_TieredGasJSON.c
//
// File summary: JSON structures + load/save utilities for TieredGas config (zones + defaults + lookups).
//
// Functions
//
// string GenerateZoneUUID()
//      Generates a unique identifier for a zone entry.
//      Params: none
//
// bool Load(ref TieredGasSettings settings)
//      Loads main TieredGas settings from JSON (and/or creates defaults if missing).
//      Params:
//          settings: settings object to fill
//
// TieredGasSettings CreateDefaultSettings()
//      Builds an in-memory default settings structure.
//      Params: none
//
// GasTypeConfig GetGasTypeConfig(ref array<ref GasTypeConfig> gasTypes, int gasType)
//      Finds the config entry for a given gas type.
//      Params:
//          gasTypes: config list
//          gasType: type ID to locate
//
// int GetZoneGasTier(GasZoneConfig cfg)
//      Returns gas tier for a zone config (with safe fallback if unset/invalid).
//      Params:
//          cfg: zone config
//
// int GetZoneGasType(GasZoneConfig cfg)
//      Returns gas type for a zone config (with safe fallback).
//      Params:
//          cfg: zone config
//
// bool GetZoneRequiresMask(GasZoneConfig cfg)
//      Returns whether this zone requires a mask.
//      Params:
//          cfg: zone config
//
// bool LoadZonesFromJSON(out array<ref GasZoneConfig> zones)
//      Loads the zones list from the zones JSON file.
//      Params (out):
//          zones: filled with loaded zone configs
//
// bool SaveZonesToJSON(array<ref GasZoneConfig> zones)
//      Saves zones list back to JSON.
//      Params:
//          zones: list to write
//---------------------------------------------------------------------------------------------------

class GasZoneConfig
{
    
    string uuid;
    string position;
    float radius;
    int tier;
    int gasType;
    bool maskRequired;
    float height;
    float bottomOffset;
    float verticalMargin;
    bool isDynamic;
    string name;
    string colorId;      
    string density;      
    bool cycle;
    float cycleSeconds;

}

class GasTypeData
{

    float healthDamage;
    float bloodDamage;
    float shockDamage;
    float filterDrain;
    bool blur;
    bool cough;
    ref array<float> color;

}

class GasTierData
{

    float damageMultiplier;
    float filterMultiplier;

}



class TieredGasNerveExposureConfig
{

    float threshold;
    int instantTier;
    ref map<int, float> rateMultByTier;

}

class TieredGasFXTierConfig
{

    float gasBlur;
    float gasVignette;
    float nerveBlurMin;
    float nerveBlurSpikeMin;
    float nerveVignetteBase;

}

class TieredGasEffectRule
{
    bool enabled;
    int  minTier;

    void TieredGasEffectRule(bool e = true, int t = 1)
    {
        enabled = e;
        minTier = t;
    }

    bool AllowsTier(int tier)
    {
        if (!enabled) return false;
        if (minTier < 1) return true;
        return tier >= minTier;
    }
}

class TieredGasJSON_Instance
{
    ref TieredGasNerveExposureConfig NerveExposure;

    ref map<int, ref TieredGasFXTierConfig> FXByTier;

    ref map<string, ref GasTypeData> GasTypes;
    ref map<int, ref GasTierData> Tiers;

    float protectionLeakThreshold; 
    float protectionMinHealthCap;  


    // Chance tables (by tier)
    ref map<int, float> toxicBleedChanceByTier;
    float toxicBleedChanceCap;

    ref map<int, float> bioInfectionChanceByTier;
    float bioInfectionChanceCap;
    ref map<string, ref TieredGasEffectRule> PermanentEffects;
    ref map<string, ref TieredGasEffectRule> TierEffects;

    string protectionSlot;

    ref map<int, string> protectionClassItemsByTier;
}

class TieredGasJSON
{
    // --------------------------------------------------------------------
    // Common paths
    // --------------------------------------------------------------------
    static string GetConfigFolder()
    {
        return "$profile:TieredGas";
    }

    static string GetGasSettingsPath()
    {
        return GetConfigFolder() + "/GasSettings.json";
    }

    static string GetGasZonesPath()
    {
        return GetConfigFolder() + "/GasZones.json";
    }

    static string GetAdminListPath()
    {
        return GetConfigFolder() + "/AdminList.json";
    }

    static string GetAdvancedSettingsPath()
    {
        return GetConfigFolder() + "/AdvancedTieredGasSetting.json";
    }

    static string GenerateZoneUUID()
    {
        int t = GetGame().GetTime();
        int r = Math.RandomInt(100000, 999999);
        return "TGZ-" + t.ToString() + "-" + r.ToString();
    }

    static ref map<string, ref GasTypeData> s_GasTypes;
    static ref map<int, ref GasTierData> s_Tiers;

    static ref TieredGasNerveExposureConfig s_NerveExposure;

    static ref map<int, ref TieredGasFXTierConfig> s_FXByTier;

    static float s_ProtectionLeakThreshold = 0.30;
    static float s_ProtectionMinHealthCap  = 0.20;


    static ref map<int, float> s_ToxicBleedChanceByTier;
    static float s_ToxicBleedChanceCap = 0.50;

    static ref map<int, float> s_BioInfectionChanceByTier;
    static float s_BioInfectionChanceCap = 0.20;
    static ref map<string, ref TieredGasEffectRule> s_PermanentEffects;
    static ref map<string, ref TieredGasEffectRule> s_TierEffects;

    static string s_ProtectionSlot = "Armband";
    static ref map<int, string> s_ProtectionClassItemsByTier;

    static bool m_Loaded = false;

    static void Load(bool forceReload = false)
    {
        if (m_Loaded && !forceReload) { return; }

        s_GasTypes = new map<string, ref GasTypeData>;
        s_Tiers    = new map<int, ref GasTierData>;
        s_NerveExposure = new TieredGasNerveExposureConfig();
        s_FXByTier      = new map<int, ref TieredGasFXTierConfig>;
        s_PermanentEffects = new map<string, ref TieredGasEffectRule>;
        s_TierEffects      = new map<string, ref TieredGasEffectRule>;


        s_ToxicBleedChanceByTier   = new map<int, float>;
        s_BioInfectionChanceByTier  = new map<int, float>;
        s_ProtectionClassItemsByTier = new map<int, string>;
        string folder = GetConfigFolder();
        string path   = GetGasSettingsPath();

        if (!FileExist(folder)) { MakeDirectory(folder); }

        ref TieredGasJSON_Instance defaults = CreateDefaultSettings();
        bool needsSave = false;

        if (FileExist(path))
        {
            ref TieredGasJSON_Instance loaded = new TieredGasJSON_Instance();
            JsonFileLoader<TieredGasJSON_Instance>.JsonLoadFile(path, loaded);

            if (loaded && loaded.GasTypes && loaded.Tiers)
            {
                s_GasTypes = loaded.GasTypes;
                s_Tiers = loaded.Tiers;

                if (loaded.PermanentEffects) s_PermanentEffects = loaded.PermanentEffects;
                if (loaded.TierEffects)      s_TierEffects      = loaded.TierEffects;

                if (loaded.NerveExposure) s_NerveExposure = loaded.NerveExposure;
                if (loaded.FXByTier)      s_FXByTier      = loaded.FXByTier;

                if (loaded.protectionLeakThreshold > 0) s_ProtectionLeakThreshold = loaded.protectionLeakThreshold; else { s_ProtectionLeakThreshold = defaults.protectionLeakThreshold; needsSave = true; }
                if (loaded.protectionMinHealthCap  > 0) s_ProtectionMinHealthCap  = loaded.protectionMinHealthCap;  else { s_ProtectionMinHealthCap  = defaults.protectionMinHealthCap;  needsSave = true; }

                if (loaded.toxicBleedChanceByTier) s_ToxicBleedChanceByTier = loaded.toxicBleedChanceByTier; else { s_ToxicBleedChanceByTier = defaults.toxicBleedChanceByTier; needsSave = true; }
                if (loaded.toxicBleedChanceCap > 0) s_ToxicBleedChanceCap = loaded.toxicBleedChanceCap; else { s_ToxicBleedChanceCap = defaults.toxicBleedChanceCap; needsSave = true; }

                if (loaded.bioInfectionChanceByTier) s_BioInfectionChanceByTier = loaded.bioInfectionChanceByTier; else { s_BioInfectionChanceByTier = defaults.bioInfectionChanceByTier; needsSave = true; }
                if (loaded.bioInfectionChanceCap > 0) s_BioInfectionChanceCap = loaded.bioInfectionChanceCap; else { s_BioInfectionChanceCap = defaults.bioInfectionChanceCap; needsSave = true; }

                if (loaded.protectionSlot && loaded.protectionSlot.Length() > 0)
                    s_ProtectionSlot = loaded.protectionSlot;
                else { s_ProtectionSlot = defaults.protectionSlot; needsSave = true; }

                if (loaded.protectionClassItemsByTier)
                    s_ProtectionClassItemsByTier = loaded.protectionClassItemsByTier;
                else { s_ProtectionClassItemsByTier = defaults.protectionClassItemsByTier; needsSave = true; }

                Print("[TieredGas] Settings loaded from JSON.");
            }
            else
            {
                s_GasTypes = defaults.GasTypes;
                s_Tiers = defaults.Tiers;
                s_PermanentEffects = defaults.PermanentEffects;
                s_TierEffects      = defaults.TierEffects;
                s_ProtectionLeakThreshold = defaults.protectionLeakThreshold;
                s_ProtectionMinHealthCap  = defaults.protectionMinHealthCap;
                s_ToxicBleedChanceByTier  = defaults.toxicBleedChanceByTier;
                s_ToxicBleedChanceCap     = defaults.toxicBleedChanceCap;
                s_BioInfectionChanceByTier = defaults.bioInfectionChanceByTier;
                s_BioInfectionChanceCap    = defaults.bioInfectionChanceCap;
                s_ProtectionSlot           = defaults.protectionSlot;
                s_ProtectionClassItemsByTier = defaults.protectionClassItemsByTier;
                s_ToxicBleedChanceByTier  = defaults.toxicBleedChanceByTier;
                s_ToxicBleedChanceCap     = defaults.toxicBleedChanceCap;
                s_BioInfectionChanceByTier = defaults.bioInfectionChanceByTier;
                s_BioInfectionChanceCap    = defaults.bioInfectionChanceCap;
                Print("[TieredGas] Failed to load JSON, using defaults.");
                needsSave = true;
            }

            if (needsSave)
            {
                ref TieredGasJSON_Instance merged = loaded;
                if (!merged) merged = new TieredGasJSON_Instance();
                if (!merged.GasTypes) merged.GasTypes = s_GasTypes;
                if (!merged.Tiers)    merged.Tiers    = s_Tiers;
                if (!merged.PermanentEffects) merged.PermanentEffects = s_PermanentEffects;
                if (!merged.TierEffects)      merged.TierEffects      = s_TierEffects;
                if (!merged.NerveExposure) merged.NerveExposure = s_NerveExposure;
                if (!merged.FXByTier)      merged.FXByTier      = s_FXByTier;
                merged.protectionLeakThreshold = s_ProtectionLeakThreshold;
                merged.protectionMinHealthCap  = s_ProtectionMinHealthCap;
                merged.toxicBleedChanceByTier = s_ToxicBleedChanceByTier;
                merged.toxicBleedChanceCap    = s_ToxicBleedChanceCap;
                merged.bioInfectionChanceByTier = s_BioInfectionChanceByTier;
                merged.bioInfectionChanceCap    = s_BioInfectionChanceCap;

                merged.protectionSlot = s_ProtectionSlot;
                merged.protectionClassItemsByTier = s_ProtectionClassItemsByTier;

                JsonFileLoader<TieredGasJSON_Instance>.JsonSaveFile(path, merged);
                Print("[TieredGas] Migrated GasSettings.json with new protection fields.");
            }
        }
        else
        {
            s_GasTypes = defaults.GasTypes;
            s_Tiers = defaults.Tiers;
            s_PermanentEffects = defaults.PermanentEffects;
            s_TierEffects      = defaults.TierEffects;
            s_ProtectionLeakThreshold = defaults.protectionLeakThreshold;
            s_ProtectionMinHealthCap  = defaults.protectionMinHealthCap;
            s_NerveExposure = defaults.NerveExposure;
            s_FXByTier      = defaults.FXByTier;
            s_ProtectionSlot = defaults.protectionSlot;
            s_ProtectionClassItemsByTier = defaults.protectionClassItemsByTier;
            JsonFileLoader<TieredGasJSON_Instance>.JsonSaveFile(path, defaults);
            Print("[TieredGas] Created default GasSettings.json");
        }

        EnsureEffectDefaults();

        m_Loaded = true;
        Print("[TieredGas] Settings ready.");
    }

    static ref TieredGasJSON_Instance CreateDefaultSettings()
    {
        ref TieredGasJSON_Instance inst = new TieredGasJSON_Instance();
        inst.GasTypes = new map<string, ref GasTypeData>;
        inst.Tiers    = new map<int, ref GasTierData>;

        inst.PermanentEffects = new map<string, ref TieredGasEffectRule>;
        inst.TierEffects      = new map<string, ref TieredGasEffectRule>;

        inst.protectionLeakThreshold = 0.30;
        inst.protectionMinHealthCap  = 0.20;
        inst.toxicBleedChanceByTier = new map<int, float>;
        inst.toxicBleedChanceByTier.Insert(1, 0.15);
        inst.toxicBleedChanceByTier.Insert(2, 0.25);
        inst.toxicBleedChanceByTier.Insert(3, 0.50);
        inst.toxicBleedChanceByTier.Insert(4, 0.75);
        inst.toxicBleedChanceCap = 0.50;

        inst.bioInfectionChanceByTier = new map<int, float>;
        inst.bioInfectionChanceByTier.Insert(1, 0.05);
        inst.bioInfectionChanceByTier.Insert(2, 0.10);
        inst.bioInfectionChanceByTier.Insert(3, 0.15);
        inst.bioInfectionChanceByTier.Insert(4, 0.20);
        inst.bioInfectionChanceCap = 0.20;

        inst.protectionSlot = "Armband";
        inst.protectionClassItemsByTier = new map<int, string>;
        inst.protectionClassItemsByTier.Insert(1, "NBCSuit_Tier1");
        inst.protectionClassItemsByTier.Insert(2, "NBCSuit_Tier2");
        inst.protectionClassItemsByTier.Insert(3, "NBCSuit_Tier3");
        inst.protectionClassItemsByTier.Insert(4, "NBCSuit_Tier4");


        inst.NerveExposure = new TieredGasNerveExposureConfig();
        inst.NerveExposure.threshold = 180.0;
        inst.NerveExposure.instantTier = 4;
        inst.NerveExposure.rateMultByTier = new map<int, float>;
        inst.NerveExposure.rateMultByTier.Insert(1, 1.00);
        inst.NerveExposure.rateMultByTier.Insert(2, 1.25);
        inst.NerveExposure.rateMultByTier.Insert(3, 1.60);
        inst.NerveExposure.rateMultByTier.Insert(4, 2.20);

        inst.FXByTier = new map<int, ref TieredGasFXTierConfig>;
        for (int fxT = 1; fxT <= 4; fxT++)
        {
            ref TieredGasFXTierConfig fx = new TieredGasFXTierConfig();
            switch (fxT)
            {
                case 1: fx.gasBlur = 0.15; fx.gasVignette = 0.00; fx.nerveBlurMin = 0.22; fx.nerveBlurSpikeMin = 0.30; fx.nerveVignetteBase = 0.12; break;
                case 2: fx.gasBlur = 0.25; fx.gasVignette = 0.00; fx.nerveBlurMin = 0.28; fx.nerveBlurSpikeMin = 0.38; fx.nerveVignetteBase = 0.18; break;
                case 3: fx.gasBlur = 0.35; fx.gasVignette = 0.00; fx.nerveBlurMin = 0.34; fx.nerveBlurSpikeMin = 0.46; fx.nerveVignetteBase = 0.24; break;
                default: fx.gasBlur = 0.45; fx.gasVignette = 0.00; fx.nerveBlurMin = 0.40; fx.nerveBlurSpikeMin = 0.55; fx.nerveVignetteBase = 0.30; break;
            }
            inst.FXByTier.Insert(fxT, fx);
        }

        inst.PermanentEffects.Insert("NERVE_PERMANENT", new TieredGasEffectRule(true, 3));
        inst.PermanentEffects.Insert("BIO_INFECTION",   new TieredGasEffectRule(true, 2));
        inst.PermanentEffects.Insert("TOXIC_WOUND",     new TieredGasEffectRule(true, 2));

        inst.TierEffects.Insert("BLUR",  new TieredGasEffectRule(true, 1));
        inst.TierEffects.Insert("COUGH", new TieredGasEffectRule(true, 1));


        ref GasTypeData toxic = new GasTypeData();
        toxic.healthDamage = 6; toxic.bloodDamage = 0; toxic.shockDamage = 0;
        toxic.filterDrain = 1.0; toxic.blur = true; toxic.cough = true;
        toxic.color = {0.6,1.0,0.6,0.3};
        inst.GasTypes.Insert("TOXIC", toxic);

        ref GasTypeData nerve = new GasTypeData();
        nerve.healthDamage = 4; nerve.bloodDamage = 2.5; nerve.shockDamage = 2;
        nerve.filterDrain = 1.2; nerve.blur = true; nerve.cough = false;
        nerve.color = {1.0,0.6,0.6,0.4};
        inst.GasTypes.Insert("NERVE", nerve);

        ref GasTypeData bio = new GasTypeData();
        bio.healthDamage = 2; bio.bloodDamage = 4; bio.shockDamage = 0;
        bio.filterDrain = 0.8; bio.blur = false; bio.cough = true;
        bio.color = {0.6,0.6,1.0,0.3};
        inst.GasTypes.Insert("BIO", bio);

        for (int i = 1; i <= 4; i++)
        {
            ref GasTierData tier = new GasTierData();
            tier.damageMultiplier = 0.5 + (i * 0.5);
            tier.filterMultiplier = 1.0 + (i * 0.25);
            inst.Tiers.Insert(i, tier);
        }

        return inst;
    }

    static float GetProtectionLeakThreshold()
    {
        if (!m_Loaded) { Load(); }
        if (s_ProtectionLeakThreshold <= 0) s_ProtectionLeakThreshold = 0.30;
        if (s_ProtectionLeakThreshold > 1)  s_ProtectionLeakThreshold = 1.0;
        return s_ProtectionLeakThreshold;
    }

    static float GetProtectionMinHealthCap()
    {
        if (!m_Loaded) { Load(); }
        if (s_ProtectionMinHealthCap < 0)  s_ProtectionMinHealthCap = 0.0;
        if (s_ProtectionMinHealthCap > 1)  s_ProtectionMinHealthCap = 1.0;
        return s_ProtectionMinHealthCap;
    }

    static string GetProtectionSlot()
    {
        if (!m_Loaded) { Load(); }
        if (!s_ProtectionSlot || s_ProtectionSlot.Length() == 0)
            s_ProtectionSlot = "Armband";
        return s_ProtectionSlot;
    }

    static map<int, string> GetProtectionClassItemsByTier()
    {
        if (!m_Loaded) { Load(); }
        if (!s_ProtectionClassItemsByTier)
            s_ProtectionClassItemsByTier = CreateDefaultSettings().protectionClassItemsByTier;
        return s_ProtectionClassItemsByTier;
    }

    static int GetConfiguredProtectionTierForItem(ItemBase item)
    {
        if (!item) return 0;
        map<int, string> m = GetProtectionClassItemsByTier();
        if (!m) return 0;

        string t = item.GetType();
        for (int tier = 1; tier <= 4; tier++)
        {
            if (m.Contains(tier))
            {
                string cfg = m.Get(tier);
                if (cfg && cfg.Length() > 0 && cfg == t)
                    return tier;
            }
        }
        return 0;
    }

    static float GetNerveExposureThreshold()
    {
        if (!m_Loaded) { Load(); }
        if (!s_NerveExposure) { s_NerveExposure = CreateDefaultSettings().NerveExposure; }
        if (s_NerveExposure.threshold <= 0) s_NerveExposure.threshold = 180.0;
        return s_NerveExposure.threshold;
    }

    static int GetNerveInstantTier()
    {
        if (!m_Loaded) { Load(); }
        if (!s_NerveExposure) { s_NerveExposure = CreateDefaultSettings().NerveExposure; }
        if (s_NerveExposure.instantTier < 1) s_NerveExposure.instantTier = 4;
        return s_NerveExposure.instantTier;
    }

    static float GetNerveExposureRateMult(int tier)
    {
        if (!m_Loaded) { Load(); }
        if (!s_NerveExposure) { s_NerveExposure = CreateDefaultSettings().NerveExposure; }

        if (!s_NerveExposure.rateMultByTier)
            s_NerveExposure.rateMultByTier = CreateDefaultSettings().NerveExposure.rateMultByTier;

        float m = 1.0;
        if (s_NerveExposure.rateMultByTier.Contains(tier))
            m = s_NerveExposure.rateMultByTier.Get(tier);

        if (m < 0) m = 0;
        if (m > 10) m = 10;
        return m;
    }

    static TieredGasFXTierConfig GetFXTier(int tier)
    {
        if (!m_Loaded) { Load(); }

        if (!s_FXByTier)
        {
            TieredGasJSON_Instance def = CreateDefaultSettings();
            if (def) s_FXByTier = def.FXByTier;
        }

        if (!s_FXByTier) return null;

        if (s_FXByTier.Contains(tier))
            return s_FXByTier.Get(tier);

        if (s_FXByTier.Contains(1))
            return s_FXByTier.Get(1);

        return null;
    }

    static float GetGasBlurForTier(int tier)
    {
        TieredGasFXTierConfig fx = GetFXTier(tier);
        if (fx) return fx.gasBlur;
        return 0.0;
    }

    static float GetGasVignetteForTier(int tier)
    {
        TieredGasFXTierConfig fx = GetFXTier(tier);
        if (fx) return fx.gasVignette;
        return 0.0;
    }

    static float GetNerveBlurMinForTier(int tier)
    {
        TieredGasFXTierConfig fx = GetFXTier(tier);
        if (fx) return fx.nerveBlurMin;
        return 0.25;
    }

    static float GetNerveBlurSpikeMinForTier(int tier)
    {
        TieredGasFXTierConfig fx = GetFXTier(tier);
        if (fx) return fx.nerveBlurSpikeMin;
        return 0.35;
    }

    static float GetNerveVignetteBaseForTier(int tier)
    {
        TieredGasFXTierConfig fx = GetFXTier(tier);
        if (fx) return fx.nerveVignetteBase;
        return 0.15;
    }
    static GasTypeData GetGasType(string name)
    {
        if (!s_GasTypes) { Load(); }
        if (s_GasTypes.Contains(name)) { return s_GasTypes.Get(name); }
        Print("[TieredGas] Warning: GasTypeData not found for: " + name);
        return null;
    }

    static GasTierData GetTier(int tier)
    {
        if (!s_Tiers) { Load(); }
        if (s_Tiers.Contains(tier)) { return s_Tiers.Get(tier); }
        Print("[TieredGas] Warning: GasTierData not found for tier: " + tier);
        return null;
    }

    static void EnsureEffectDefaults()
    {
        if (!s_PermanentEffects) { s_PermanentEffects = new map<string, ref TieredGasEffectRule>; }
        if (!s_TierEffects)      { s_TierEffects      = new map<string, ref TieredGasEffectRule>; }

        if (!s_PermanentEffects.Contains("NERVE_PERMANENT"))
            s_PermanentEffects.Insert("NERVE_PERMANENT", new TieredGasEffectRule(true, 3));
        if (!s_PermanentEffects.Contains("BIO_INFECTION"))
            s_PermanentEffects.Insert("BIO_INFECTION", new TieredGasEffectRule(true, 2));
        if (!s_PermanentEffects.Contains("TOXIC_WOUND"))
            s_PermanentEffects.Insert("TOXIC_WOUND", new TieredGasEffectRule(true, 2));

        if (!s_TierEffects.Contains("BLUR"))
            s_TierEffects.Insert("BLUR", new TieredGasEffectRule(true, 1));
        if (!s_TierEffects.Contains("COUGH"))
            s_TierEffects.Insert("COUGH", new TieredGasEffectRule(true, 1));
    }

    static bool AllowsPermanentEffect(string key, int gasTier)
    {
        if (!s_PermanentEffects) { EnsureEffectDefaults(); }
        if (s_PermanentEffects.Contains(key))
            return s_PermanentEffects.Get(key).AllowsTier(gasTier);
        return false;
    }

    static bool AllowsTierEffect(string key, int gasTier)
    {
        if (!s_TierEffects) { EnsureEffectDefaults(); }
        if (s_TierEffects.Contains(key))
            return s_TierEffects.Get(key).AllowsTier(gasTier);
        return false;
    }

    static float GetToxicBleedChanceForTier(int tier)
    {
        if (!m_Loaded) { Load(); }
        if (!s_ToxicBleedChanceByTier)
            s_ToxicBleedChanceByTier = CreateDefaultSettings().toxicBleedChanceByTier;

        float v = 0.0;
        if (s_ToxicBleedChanceByTier && s_ToxicBleedChanceByTier.Contains(tier))
            v = s_ToxicBleedChanceByTier.Get(tier);
        else if (s_ToxicBleedChanceByTier && s_ToxicBleedChanceByTier.Contains(1))
            v = s_ToxicBleedChanceByTier.Get(1);

        if (v < 0) v = 0;
        if (v > 1) v = 1;
        return v;
    }

    static float GetToxicBleedChanceCap()
    {
        if (!m_Loaded) { Load(); }
        if (s_ToxicBleedChanceCap < 0) s_ToxicBleedChanceCap = 0.0;
        if (s_ToxicBleedChanceCap > 1) s_ToxicBleedChanceCap = 1.0;
        return s_ToxicBleedChanceCap;
    }

    static float GetBioInfectionChanceForTier(int tier)
    {
        if (!m_Loaded) { Load(); }
        if (!s_BioInfectionChanceByTier)
            s_BioInfectionChanceByTier = CreateDefaultSettings().bioInfectionChanceByTier;

        float v = 0.0;
        if (s_BioInfectionChanceByTier && s_BioInfectionChanceByTier.Contains(tier))
            v = s_BioInfectionChanceByTier.Get(tier);
        else if (s_BioInfectionChanceByTier && s_BioInfectionChanceByTier.Contains(1))
            v = s_BioInfectionChanceByTier.Get(1);

        if (v < 0) v = 0;
        if (v > 1) v = 1;
        return v;
    }

    static float GetBioInfectionChanceCap()
    {
        if (!m_Loaded) { Load(); }
        if (s_BioInfectionChanceCap < 0) s_BioInfectionChanceCap = 0.0;
        if (s_BioInfectionChanceCap > 1) s_BioInfectionChanceCap = 1.0;
        return s_BioInfectionChanceCap;
    }

    static float GetFilterDrain(int gasType)
    {
        GasTypeData d = GetGasType(TieredGasTypes.GasTypeToString(gasType));
        if (d) { return d.filterDrain; }
        return 1.0;
    }

    static bool GetZoneRequiresMask(Object zoneObj)
    {
        if (!zoneObj) { return false; }

        TieredGasZone tgz = TieredGasZone.Cast(zoneObj);
        if (!tgz) { return false; }

        return tgz.GetMaskRequired();
    }

    static bool LoadZonesFromJSON(out array<ref GasZoneConfig> zones)
    {
        if (!zones) zones = new array<ref GasZoneConfig>();

        string folder = GetConfigFolder();
        string path = GetGasZonesPath();

        if (!FileExist(path)) { return false; }

        JsonFileLoader<ref array<ref GasZoneConfig>>.JsonLoadFile(path, zones);

        foreach (GasZoneConfig z : zones)
        {
            if (!z) continue;

            if (z.colorId == "") z.colorId = "default";

            string d = z.density;
            d.Trim();
            d.ToLower();

            if (d == "" || d == "med" || d == "medium") d = "normal";
            if (d == "light") d = "low";
            if (d == "lo") d = "low";

            if (d != "low" && d != "normal" && d != "dense") d = "normal";
            z.density = d;
        }

        return zones.Count() > 0;
    }

    static void SaveZonesToJSON(array<ref GasZoneConfig> zones)
    {
        string folder = GetConfigFolder();
        string path   = GetGasZonesPath();

        if (!FileExist(folder)) { MakeDirectory(folder); }
        JsonFileLoader<array<ref GasZoneConfig>>.JsonSaveFile(path, zones);
        Print("[TieredGas] Saved " + zones.Count().ToString() + " gas zones to JSON");
    }

    static void LoadAdminUIDs(out array<string> uids)
    {
        if (!uids) uids = new array<string>;

        string folder = GetConfigFolder();
        string path   = GetAdminListPath();

        if (!FileExist(folder)) { MakeDirectory(folder); }

        if (!FileExist(path))
        {
            uids.Insert("YOUR_UUID_HERE");
            JsonFileLoader<ref array<string>>.JsonSaveFile(path, uids);
            return;
        }

        JsonFileLoader<ref array<string>>.JsonLoadFile(path, uids);
    }

    static void SaveAdminUIDs(array<string> uids)
    {
        string folder = GetConfigFolder();
        string path   = GetAdminListPath();
        if (!FileExist(folder)) { MakeDirectory(folder); }
        JsonFileLoader<ref array<string>>.JsonSaveFile(path, uids);
    }

    static TG_AdvancedTieredGasSetting LoadAdvancedSettings(ref TG_AdvancedTieredGasSetting defaults)
    {
        string folder = GetConfigFolder();
        string path   = GetAdvancedSettingsPath();

        if (!FileExist(folder)) { MakeDirectory(folder); }

        if (!defaults)
        {
            defaults = new TG_AdvancedTieredGasSetting();
            defaults.maxAnchorsByRadius = new array<ref TG_AnchorBand>();
            defaults.densityAnchorMultiplier = new map<string, float>();
            defaults.spacingByDensity        = new map<string, float>();
            defaults.jitterByDensity         = new map<string, float>();
            defaults.maxAnchorsHardCap = 600;
        }

        ref TG_AdvancedTieredGasSetting result;
        bool needsSave = false;

        if (FileExist(path))
        {
            ref TG_AdvancedTieredGasSetting loaded = new TG_AdvancedTieredGasSetting();
            JsonFileLoader<TG_AdvancedTieredGasSetting>.JsonLoadFile(path, loaded);

            if (loaded && loaded.maxAnchorsByRadius && loaded.maxAnchorsByRadius.Count() > 0)
            {
                result = loaded;
            }
            else
            {
                result = defaults;
                needsSave = true;
                Print("[TieredGas] AdvancedTieredGasSetting.json missing schema, overwriting with defaults.");
            }
        }
        else
        {
            result = defaults;
            needsSave = true;
            Print("[TieredGas] Created default AdvancedTieredGasSetting.json");
        }

        if (!result.maxAnchorsByRadius)      { result.maxAnchorsByRadius = defaults.maxAnchorsByRadius; needsSave = true; }
        if (!result.densityAnchorMultiplier){ result.densityAnchorMultiplier = defaults.densityAnchorMultiplier; needsSave = true; }
        if (!result.spacingByDensity)       { result.spacingByDensity        = defaults.spacingByDensity; needsSave = true; }
        if (!result.jitterByDensity)        { result.jitterByDensity         = defaults.jitterByDensity; needsSave = true; }
        if (result.maxAnchorsHardCap <= 0)  { result.maxAnchorsHardCap       = defaults.maxAnchorsHardCap; needsSave = true; }

        if (needsSave)
        {
            JsonFileLoader<TG_AdvancedTieredGasSetting>.JsonSaveFile(path, result);
        }

        return result;
    }

    static void ZonesToJsonString(array<ref GasZoneConfig> zones, out string jsonStr, bool pretty = true)
    {
        JsonSerializer js = new JsonSerializer();
        js.WriteToString(zones, pretty, jsonStr);
    }

    static bool ZonesFromJsonString(string jsonStr, out array<ref GasZoneConfig> zones, out string err)
    {
        zones = new array<ref GasZoneConfig>();
        err = "";

        JsonSerializer js = new JsonSerializer();
        return js.ReadFromString(zones, jsonStr, err);
    }

    static void ZonesToChunks(array<ref GasZoneConfig> zones, int chunkSize, out array<string> chunks, out string fullJson)
    {
        ZonesToJsonString(zones, fullJson, true);

        int len = fullJson.Length();
        int total = Math.Ceil(len / (float)chunkSize);

        chunks = new array<string>();
        chunks.Reserve(total);

        for (int i = 0; i < total; i++)
        {
            int start = i * chunkSize;
            int count = chunkSize;
            if (start + count > len) count = len - start;

            chunks.Insert(fullJson.Substring(start, count));
        }
    }

    static bool ZonesFromChunks(array<string> chunks, out array<ref GasZoneConfig> zones, out string err)
    {
        zones = new array<ref GasZoneConfig>();
        err = "";

        if (!chunks || chunks.Count() == 0)
        {
            err = "No chunks";
            return false;
        }

        string jsonStr = "";
        for (int i = 0; i < chunks.Count(); i++)
            jsonStr += chunks[i];

        return ZonesFromJsonString(jsonStr, zones, err);
    }
}
