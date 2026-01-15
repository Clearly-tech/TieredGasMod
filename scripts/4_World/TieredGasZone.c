//---------------------------------------------------------------------------------------------------
// scripts/4_World/TieredGasZone.c
//
// File summary: Zone world object + advanced settings: contains zone metadata, containment checks, and client visual anchor generation.
//
// TG_AdvancedTieredGasSettingMgr
//
// string GetPath()
//      Returns path to advanced settings JSON for anchor/visual tuning.
//      Params: none
//
// void EnsureLoaded()
//      Loads advanced settings from disk once (creates defaults if missing).
//      Params: none
//
// int GetBaseMaxAnchors(float radius)
//      Base anchor count from radius before density modifiers.
//      Params:
//          radius: zone radius
//
// float GetMapFloat(ref map<string, float> m, string key, float def)
//      Map lookup helper with fallback.
//      Params:
//          m: map to read
//          key: key to find
//          def: fallback
//
// float GetAnchorSpacing(string dens)
//      Returns spacing based on density string.
//      Params:
//          dens: density ID/string
//
// float GetAnchorJitter(string dens)
//      Returns random jitter amount based on density.
//      Params:
//          dens: density ID/string
//
// int GetAnchorMax(float radius, string dens)
//      returns final max anchor count given radius + density.
//      Params:
//          radius: zone radius
//          dens: density ID/string
//
//
// TieredGasZone : BuildingBase
//
//
// void TieredGasZone()
//      Constructor; initializes zone defaults.
//      Params: none
//
// void ApplyConfig(string uuid, string name, string density, string colorId, int gasTier, int gasType, float radius, bool requiresMask, float cloudHeight, float bottomOffset, float verticalMargin, bool isDynamic)
//      Applies config fields to this zone instance.
//      Params: (each is the zone config field as named)
//
// void StartVisualTimer()
//      Starts periodic visual updates (anchor refresh / client particle sync).
//      Params: none
//
// void StopVisualTimer()
//      Stops periodic visual updates.
//      Params: none
//
// void OnVisualTick()
//      Periodic visual update callback.
//      Params: none
//
// bool IsInside(vector pos)
//      Checks if a world position is inside the zone’s volume/radius.
//      Params:
//          pos: world position
//
// int GetAnchorMax()
//      Returns computed anchor max for this zone (based on radius/density/advanced settings).
//      Params: none
//
// float GetAnchorJitter()
//      Returns computed jitter for this zone.
//      Params: none
//
// string ResolveCloudParticleKey(bool low)
//      Resolves which cloud particle to use (based on density/color + LOD).
//      Params:
//          low: whether to use low variant
//
// string ResolveLocalParticleKey()
//      Resolves which local “inside gas” particle key to use.
//      Params: none
//
// int HashString(string s)
//      Hash helper used for deterministic pseudo-random placement.
//      Params:
//          s: input string
//
// float Rand01(int seed)
//      Deterministic random 0..1 from seed.
//      Params:
//          seed: seed value
//
// array<vector> BuildCloudAnchorsFilled(vector center)
//      Builds anchor positions for cloud particles covering the zone area.
//      Params:
//          center: zone center position
//
// string GetUUID()
//      Returns zone UUID.
//      Params: none
//
// string GetZoneName()
//      Returns zone display name.
//      Params: none
//
// string GetColorId()
//      Returns zone color ID.
//      Params: none
//
// string GetDensity()
//      Returns zone density string.
//      Params: none
//
// int GetGasTier()
//      Returns gas tier.
//      Params: none
//
// int GetGasType()
//      Returns gas type.
//      Params: none
//
// float GetRadius()
//      Returns radius.
//      Params: none
//
// bool GetMaskRequired()
//      Returns mask-required flag.
//      Params: none
//---------------------------------------------------------------------------------------------------

class TG_AnchorBand
{
    float maxRadius;
    int maxAnchors;
}
class TG_AdvancedTieredGasSetting
{
    ref array<ref TG_AnchorBand> maxAnchorsByRadius;
    ref map<string, float> densityAnchorMultiplier; 
    ref map<string, float> spacingByDensity;        
    ref map<string, float> jitterByDensity;         
    int maxAnchorsHardCap;
}

class TG_AdvancedTieredGasSettingMgr
{
    static ref TG_AdvancedTieredGasSetting s_Data;
    static bool s_Loaded;

    static string GetPath()
    {
        return "$profile:TieredGas/AdvancedTieredGasSetting.json";
    }

    static void EnsureLoaded()
    {
        if (s_Loaded && s_Data) return;

        string folder = "$profile:TieredGas";
        string path = GetPath();
        if (!FileExist(folder)) { MakeDirectory(folder); }

        ref TG_AdvancedTieredGasSetting def = new TG_AdvancedTieredGasSetting();
        def.maxAnchorsByRadius = new array<ref TG_AnchorBand>();

        ref TG_AnchorBand b0 = new TG_AnchorBand(); b0.maxRadius = 50.0;  b0.maxAnchors = 100; def.maxAnchorsByRadius.Insert(b0);
        ref TG_AnchorBand b1 = new TG_AnchorBand(); b1.maxRadius = 300.0; b1.maxAnchors = 200; def.maxAnchorsByRadius.Insert(b1);
        ref TG_AnchorBand b2 = new TG_AnchorBand(); b2.maxRadius = 600.0; b2.maxAnchors = 300; def.maxAnchorsByRadius.Insert(b2);
        ref TG_AnchorBand b3 = new TG_AnchorBand(); b3.maxRadius = 900.0; b3.maxAnchors = 450; def.maxAnchorsByRadius.Insert(b3);

        def.densityAnchorMultiplier = new map<string, float>();
        def.densityAnchorMultiplier.Insert("Light", 1.00);
        def.densityAnchorMultiplier.Insert("Normal", 1.15);
        def.densityAnchorMultiplier.Insert("Dense", 1.35);

        def.spacingByDensity = new map<string, float>();
        def.spacingByDensity.Insert("Light", 70.0);
        def.spacingByDensity.Insert("Normal", 55.0);
        def.spacingByDensity.Insert("Dense", 40.0);

        def.jitterByDensity = new map<string, float>();
        def.jitterByDensity.Insert("Light", 14.0);
        def.jitterByDensity.Insert("Normal", 12.0);
        def.jitterByDensity.Insert("Dense", 10.0);

        def.maxAnchorsHardCap = 600;

        if (FileExist(path))
        {
            ref TG_AdvancedTieredGasSetting loaded = new TG_AdvancedTieredGasSetting();
            JsonFileLoader<TG_AdvancedTieredGasSetting>.JsonLoadFile(path, loaded);

            
            if (loaded && loaded.maxAnchorsByRadius && loaded.maxAnchorsByRadius.Count() > 0)
            {
                s_Data = loaded;
            }
            else
            {
                s_Data = def;
                JsonFileLoader<TG_AdvancedTieredGasSetting>.JsonSaveFile(path, def);
                Print("[TieredGas] AdvancedTieredGasSetting.json missing new schema, overwrote with defaults.");
            }
        }
        else
        {
            s_Data = def;
            JsonFileLoader<TG_AdvancedTieredGasSetting>.JsonSaveFile(path, def);
            Print("[TieredGas] Created default AdvancedTieredGasSetting.json");
        }

        if (!s_Data.densityAnchorMultiplier) s_Data.densityAnchorMultiplier = def.densityAnchorMultiplier;
        if (!s_Data.spacingByDensity)        s_Data.spacingByDensity        = def.spacingByDensity;
        if (!s_Data.jitterByDensity)         s_Data.jitterByDensity         = def.jitterByDensity;
        if (s_Data.maxAnchorsHardCap <= 0)   s_Data.maxAnchorsHardCap       = def.maxAnchorsHardCap;

        s_Loaded = true;
    }

    static int GetBaseMaxAnchors(float radius)
    {
        EnsureLoaded();
        int fallback = 200;
        if (!s_Data || !s_Data.maxAnchorsByRadius || s_Data.maxAnchorsByRadius.Count() == 0) return fallback;

        foreach (TG_AnchorBand b : s_Data.maxAnchorsByRadius)
        {
            if (!b) continue;
            if (radius <= b.maxRadius)
                return b.maxAnchors;
            fallback = b.maxAnchors; 
        }
        return fallback;
    }

    static float GetMapFloat(map<string, float> m, string key, float def)
    {
        if (m && m.Contains(key)) return m.Get(key);
        return def;
    }

    static float GetAnchorSpacing(string dens)
    {
        EnsureLoaded();
        return GetMapFloat(s_Data.spacingByDensity, dens, 55.0);
    }

    static float GetAnchorJitter(string dens)
    {
        EnsureLoaded();
        return GetMapFloat(s_Data.jitterByDensity, dens, 12.0);
    }

    static int GetAnchorMax(float radius, string dens)
    {
        EnsureLoaded();

        int baseMax = GetBaseMaxAnchors(radius);
        float mul = GetMapFloat(s_Data.densityAnchorMultiplier, dens, 1.0);
        int outMax = Math.Round(baseMax * mul);

        int cap = s_Data.maxAnchorsHardCap;
        if (cap > 0 && outMax > cap) outMax = cap;
        if (outMax < 1) outMax = 1;
        return outMax;
    }
}
class TieredGasZone : BuildingBase
{

    static const float CLOUD_VISUAL_RANGE       = 1700.0;
    static const float CLOUD_DESPAWN_RANGE      = 2000.0;
    static const float CLOUD_DESPAWN_HOLD_SECONDS = 0;
    static const float CLOUD_HI_RANGE           = 600.0;
    static const float CLOUD_HI_HYSTERESIS      = 25.0;
    static const int   CLOUD_LOD_COOLDOWN_MS    = 8000;
    static const float VISUAL_CHECK_SECONDS     = 0.25;

    static const float ANCHOR_SPACING_FALLBACK = 55.0;
    static const float ANCHOR_JITTER_FALLBACK  = 12.0;
    static const int   ANCHOR_MAX_FALLBACK     = 200;
    
    static const float CLOUD_CROSSFADE_SECONDS = 10.50;

    string m_UUID;
    string m_Name;
    string m_ColorId;
    string m_Density;
    int m_GasTier;
    int m_GasType;

    float m_Radius;
    float m_Height;
    float m_BottomOffset;
    float m_VerticalMargin;
    bool m_IsDynamic;
    bool m_MaskRequired;

    protected ref Timer m_VisualTimer;
    protected bool m_CloudActive;
    protected bool m_LastCloudLow;
    protected string m_LastCloudKey;

    protected float m_DespawnOverTimer;

    protected int m_LastLodSwitchMs;

    void TieredGasZone()
    {
        m_CloudActive = false;
        m_LastCloudLow = false;
        m_LastCloudKey = "";
        m_DespawnOverTimer = 0.0;
        m_LastLodSwitchMs = 0;
        m_MaskRequired = false;
    }

    void ~TieredGasZone()
    {
        StopVisualTimer();

        if (GetGame() && (GetGame().IsClient() || !GetGame().IsMultiplayer()))
        {
            TieredGasParticleManager.RemoveZoneCloud(m_UUID, 0.0);
            TieredGasParticleManager.ClearPlayerLocalIfOwner(this);
        }
    }

    void ApplyConfig(string uuid, string name, string colorId, string density, int tier, int gasType, float radius, bool maskRequired, float height, float bottomOffset, float verticalMargin, bool isDynamic)
    {
        m_UUID = uuid;
        m_Name = name;
        m_ColorId = colorId;
        m_Density = density;
        m_GasTier = tier;
        m_GasType = gasType;

        m_Radius = radius;
        m_MaskRequired = maskRequired; 
        m_Height = height;
        m_BottomOffset = bottomOffset;
        m_VerticalMargin = verticalMargin;
        m_IsDynamic = isDynamic;

        StartVisualTimer();
    }

    protected void StartVisualTimer()
    {
        if (!GetGame() || !(GetGame().IsClient() || !GetGame().IsMultiplayer())) { return; }

        if (!m_VisualTimer)
        {
            m_VisualTimer = new Timer(CALL_CATEGORY_GAMEPLAY);
            m_VisualTimer.Run(VISUAL_CHECK_SECONDS, this, "OnVisualTick", NULL, true);
        }
    }

    protected void StopVisualTimer()
    {
        if (m_VisualTimer)
        {
            m_VisualTimer.Stop();
            m_VisualTimer = null;
        }
    }

    void OnVisualTick()
    {
        if (!GetGame() || !(GetGame().IsClient() || !GetGame().IsMultiplayer())) { return; }

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) { return; }

        vector playerPos = player.GetPosition();
        vector zonePos = GetPosition();

        float distSq = vector.DistanceSq(playerPos, zonePos);
        float dist = Math.Sqrt(distSq);

        bool inside = IsInside(playerPos);

        float despawnSq = CLOUD_DESPAWN_RANGE * CLOUD_DESPAWN_RANGE;
        float spawnSq = CLOUD_VISUAL_RANGE * CLOUD_VISUAL_RANGE;

        bool shouldSpawnCloud = (!m_CloudActive && distSq <= spawnSq);
        bool shouldKeepCloud = (m_CloudActive && distSq <= despawnSq);

        if (m_CloudActive && !shouldKeepCloud)
        {
            m_DespawnOverTimer += VISUAL_CHECK_SECONDS;
            if (m_DespawnOverTimer >= CLOUD_DESPAWN_HOLD_SECONDS)
            {
                m_CloudActive = false;
                m_LastCloudKey = "";
                m_DespawnOverTimer = 0.0;
                TieredGasParticleManager.RemoveZoneCloud(m_UUID, CLOUD_CROSSFADE_SECONDS);
            }
        }
        else
        {
            m_DespawnOverTimer = 0.0;
        }

        if (shouldSpawnCloud || shouldKeepCloud)
        {
            bool useLow = false;

            float hiIn  = (CLOUD_HI_RANGE - CLOUD_HI_HYSTERESIS);
            float hiOut = (CLOUD_HI_RANGE + CLOUD_HI_HYSTERESIS);

            int nowMs = GetGame().GetTime();

            bool desiredLow = false;
            if (m_LastCloudLow)
            {
                if (dist > hiIn) desiredLow = true;
            }
            else
            {
                if (dist > hiOut) desiredLow = true;
            }

            useLow = m_LastCloudLow;
            if (desiredLow != m_LastCloudLow)
            {
                if ((nowMs - m_LastLodSwitchMs) >= CLOUD_LOD_COOLDOWN_MS)
                {
                    useLow = desiredLow;
                    m_LastLodSwitchMs = nowMs;
                }
            }

            string cloudKey = ResolveCloudParticleKey(useLow);

            if (m_CloudActive && m_LastCloudKey == cloudKey && m_LastCloudLow == useLow)
            {

            }
            else
            {
                ref array<vector> anchors = BuildCloudAnchorsFilled(zonePos);

                TieredGasParticleManager.UpdateZoneCloud(m_UUID, anchors, cloudKey, CLOUD_CROSSFADE_SECONDS);
                m_CloudActive = true;
                m_LastCloudLow = useLow;
                m_LastCloudKey = cloudKey;
            }
        }

        if (inside)
        {
            string localKey = ResolveLocalParticleKey();
            TieredGasParticleManager.UpdatePlayerLocalFromZone(this, m_GasTier, player, localKey);
        }
        else
        {
            TieredGasParticleManager.ClearPlayerLocalIfOwner(this);
        }
    }

    bool IsInside(vector pos)
    {
        vector z = GetPosition();
        z[1] = GetGame().SurfaceY(z[0], z[2]) - m_BottomOffset;

        float dx = pos[0] - z[0];
        float dz = pos[2] - z[2];
        float hDist = Math.Sqrt((dx * dx) + (dz * dz));

        float dy = pos[1] - z[1];

        if (hDist > m_Radius) return false;
        if (dy < 0) return false;
        if (dy > (m_Height + m_VerticalMargin)) return false;

        return true;
    }

    protected string NormalizeColor(string c)
    {
        if (!c || c == "") 
            return "default";

        c.ToLower();
        return c;
    }

    protected string NormalizeDensity(string d)
    {
        if (!d || d == "")
            return "Normal";

        d.ToLower();

        if (d == "light")  return "Light";
        if (d == "low")    return "Light";

        if (d == "normal") return "Normal";
        if (d == "medium") return "Normal";

        if (d == "dense")  return "Dense";
        if (d == "thick")  return "Dense";

        return "Normal";
    }

    protected float GetAnchorSpacing()
    {
        string dens = NormalizeDensity(m_Density);
        float s = TG_AdvancedTieredGasSettingMgr.GetAnchorSpacing(dens);
        if (s <= 0) s = ANCHOR_SPACING_FALLBACK;
        return s;
    }

    protected int GetAnchorMax()
    {
        string dens = NormalizeDensity(m_Density);
        int m = TG_AdvancedTieredGasSettingMgr.GetAnchorMax(m_Radius, dens);
        if (m <= 0) m = ANCHOR_MAX_FALLBACK;
        return m;
    }

    protected float GetAnchorJitter()
    {
        string dens = NormalizeDensity(m_Density);
        float j = TG_AdvancedTieredGasSettingMgr.GetAnchorJitter(dens);
        if (j < 0) j = 0;
        return j;
    }

    string ResolveCloudParticleKey(bool low)
    {
        string color = NormalizeColor(m_ColorId);
        string dens  = NormalizeDensity(m_Density);

        string key = "TieredGasCloud_" + color + "_" + dens;
        if (low)
        {
            key = key + "_low";
        }
        return key;
    }

    string ResolveLocalParticleKey()
    {
        string color = NormalizeColor(m_ColorId);
        string dens  = NormalizeDensity(m_Density);

        Print("LocalPlayer Key: "+"TieredGasLocal_" + color + "_" + dens);

        return "TieredGasLocal_" + color + "_" + dens;
    }

    protected int HashString(string s)
    {
        int h = s.Hash();
        if (h < 0)
        {
            h = -h;
        }
        return h;
    }

    protected float Rand01(int seed)
    {
        int x = seed;
        x = (1103515245 * x + 12345);
        int v = (x / 65536) % 32768;
        return (v / 32767.0);
    }

    protected ref array<vector> BuildCloudAnchorsFilled(vector center)
    {
        ref array<vector> anchors = new array<vector>();
        anchors.Insert(center);

        float r = m_Radius;

        float ringStep = GetAnchorSpacing();
        int anchorMax  = GetAnchorMax();
        float maxJ     = GetAnchorJitter();

        if (r <= (ringStep * 0.75))
            return anchors;
                int baseSeed = HashString(m_UUID);
        int ringIndex = 0;
        float ringR = ringStep;

        while (ringR < r && anchors.Count() < anchorMax)
        {
            float circumference = (Math.PI * 2.0) * ringR;
            int count = Math.Floor(circumference / ringStep);
            if (count < 6) count = 6;

            for (int i = 0; i < count && anchors.Count() < anchorMax; i++)
            {
                float t = (Math.PI * 2.0 * i) / count;

                float jx = (Rand01(baseSeed + 1000 + ringIndex * 97 + i * 17) - 0.5) * (maxJ * 2.0);
                float jz = (Rand01(baseSeed + 2000 + ringIndex * 89 + i * 29) - 0.5) * (maxJ * 2.0);

                float ax = center[0] + (Math.Cos(t) * ringR) + jx;
                float az = center[2] + (Math.Sin(t) * ringR) + jz;
                float ay = GetGame().SurfaceY(ax, az);

                anchors.Insert(Vector(ax, ay, az));
            }

            ringR += ringStep;
            ringIndex++;
        }

        return anchors;
    }

    string GetUUID() { return m_UUID; }
    string GetZoneName() { return m_Name; }
    string GetColorId() { return m_ColorId; }
    string GetDensity() { return m_Density; }
    int GetGasTier() { return m_GasTier; }
    int GetGasType() { return m_GasType; }
    float GetRadius() { return m_Radius; }

    bool GetMaskRequired() { return m_MaskRequired; }
};