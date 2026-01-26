//---------------------------------------------------------------------------------------------------
// scripts/4_World/TieredGasPlayerBase.c
//
// File summary: Player gas gameplay: tracks whether player is in a zone, current tier/type, applies damage/effects, RPC hooks, reload helpers.
//
// PlayerBase (modded)
//
// bool IsInGasZone()
//      Returns whether player is currently considered inside any gas zone.
//      Params: none
//
// int GetCurrentGasTier()
//      Current gas tier affecting the player.
//      Params: none
//
// int GetCurrentGasType()
//      Current gas type affecting the player.
//      Params: none
//
// void SetGasState(bool inZone, int tier, int type, bool requiresMask, string zoneUUID)
//      Updates player’s current gas state (usually from server evaluation).
//      Params:
//          inZone: inside zone flag
//          tier: gas tier
//          type: gas type
//          requiresMask: whether mask is required in this zone
//          zoneUUID: zone identifier source
//
// void TieredGas_OnTick(float deltaTime)
//      Per-tick update that applies gas logic (damage, filter drain, wear, HUD updates).
//      Params:
//          deltaTime: frame delta time
//
// void TieredGas_ClearGasState()
//      Clears gas state when leaving zones / reset.
//      Params: none
//
// bool TieredGas_ParseBool(string s, bool def = false)
//      Helper parse boolean-like strings from configs/RPC payloads.
//      Params:
//          s: input string
//          def: fallback default
//
// vector TieredGas_ParsePositionString(string posStr)
//      Helper parse a vector from a string format.
//      Params:
//          posStr: position string
//
// void TieredGas_RequestAdminCheck()
//      Client requests admin check (RPC) to see if menu can be used.
//      Params: none
//
// void TieredGas_ReloadConfig_Server()
//      Server-side reload config action (usually triggered via admin RPC).
//      Params: none
//
// void TieredGas_ReloadAdmins_Server()
//      Server-side reload admin list action.
//      Params: none
//
// void TieredGas_ReloadZones_Server()
//      Server-side reload zones action.
//      Params: none
//
// void SendAdminMessage(PlayerIdentity ident, string msg, bool isError)
//      Server sends an admin feedback message to a client.
//      Params:
//          ident: who to send to
//          msg: message
//          isError: error flag
//
// void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
//      andles TieredGas-related RPC calls on the player entity.
//      Params:
//          sender: identity that sent RPC
//          rpc_type: RPC ID
//          ctx: payload reader
//
// void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
//      Hook for applying gas-related extra effects when damage hits (if used by your logic).
//      Params: engine-provided hit context (as named)
//
// void OnStoreSave(ParamsWriteContext ctx)
//      Saves TieredGas player state (if persisted).
//      Params:
//          ctx: save context
//
// bool OnStoreLoad(ParamsReadContext ctx, int version)
//      Loads TieredGas player state.
//      Params:
//          ctx: load context
//          version: save version
//
// void Init()
//      Player init hook for TieredGas variables.
//      Params: none
//
// void OnConnect()
//      Called on player connect; commonly used to sync admin status / initial state.
//      Params: none
//
// void OnDisconnect()
//      Cleanup when disconnecting.
//      Params: none
//---------------------------------------------------------------------------------------------------

modded class PlayerBase
{
    private const bool TG_STORE_MARKER = true;
    private int m_TieredGas_LastAdminRPCms;
    private const int TIEREDGAS_ADMIN_RPC_COOLDOWN_MS = 250;

    ref array<string> m_TG_ZonesChunks;
    int m_TG_ZonesExpected = 0;
    int m_TG_ZonesReceived = 0;

    private bool m_ClientInGas;
    private int m_ClientTier;
    private int m_ClientType;

    bool m_TG_ClientNerveActive = false;

    bool m_TG_LastSentNerveActive = false;
    int  m_TG_LastGasSyncMS = 0;

    private float m_GasCheckTimer;
    private const float GAS_CHECK_INTERVAL = 1.0;

    int m_TG_NextBleedRollMS = 0;
    int m_TG_NextBioRollMS = 0;

    int m_TG_NextCoughMS = 0;
    int m_TG_NextSneezeMS = 0;

    int m_TG_PermBlurUntilMS = 0;
    int m_TG_NextPermBlurMS  = 0;

    float m_TG_BlurCurrent = 0.0;
    float m_TG_BlurTarget  = 0.0;

    float m_TG_VignetteCurrent = 0.0;
    float m_TG_VignetteTarget  = 0.0;

    float m_TG_NerveExposure = 0.0;
    bool  m_TG_NervePermanent = false;
    int   m_TG_NerveSuppressedUntilMS = 0;

    float m_TG_BioExposure = 0.0;
    bool  m_TG_BioInfected = false;
    int   m_TG_BioNextSymptomMS = 0;

    bool IsInGasZone() { return m_ClientInGas; }
    int GetCurrentGasTier() { return m_ClientTier; }
    string GetCurrentGasType() { return TieredGasTypes.GasTypeToString(m_ClientType); }

    void SetGasHUD(bool inGas, int tier, int gasType)
    {
        m_ClientInGas = inGas;
        m_ClientTier = tier;
        m_ClientType = gasType;
    }
    void TG_ClientGasFX(float deltaTime)
    {
        if (GetGame().IsServer()) return;

        TieredGasEffects.ClientGasFX(this, deltaTime, m_ClientInGas, m_ClientTier, m_ClientType, m_TG_ClientNerveActive);
    }



    bool TieredGas_HandleAdminRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        if (!GetGame().IsServer()) { return false; }
        TieredGasAdminMenuSettings.Load();
        if (!TieredGasAdminMenuSettings.IsEnabled())
        {
            if (rpc_type == RPC_ADMIN_CHECK)
            {
                Param1<bool> responseDisabled = new Param1<bool>(false);
                GetGame().RPCSingleParam(this, RPC_ADMIN_CHECK_RESPONSE, responseDisabled, true, sender);
            }
            return true;
        }

        int now = GetGame().GetTime();
        if (now - m_TieredGas_LastAdminRPCms < TIEREDGAS_ADMIN_RPC_COOLDOWN_MS) { return true; }
        m_TieredGas_LastAdminRPCms = now;

        switch (rpc_type)
        {
            case RPC_ADMIN_CHECK:
            {
                bool isAdmin = TieredGasAdminList.IsAdmin(this);
                Param1<bool> response = new Param1<bool>(isAdmin);
                GetGame().RPCSingleParam(this, RPC_ADMIN_CHECK_RESPONSE, response, true, sender);
                return true;
            }

            case RPC_ADMIN_LIST_ZONES:
            case RPC_ADMIN_SPAWN_ZONE:
            case RPC_ADMIN_REMOVE_ZONE:
            case RPC_ADMIN_REMOVE_ZONE_BY_UUID:
            case RPC_ADMIN_RELOAD_CONFIG:
            case RPC_ADMIN_RELOAD_ADMINS:
            case RPC_ADMIN_RELOAD_ZONES:
            {
                if (!TieredGasAdminList.IsAdmin(this))
                {
                    SendAdminMessage("[TieredGas] Access denied - Admin only", true);
                    return true;
                }

                TieredGas_HandleAdminCommand(rpc_type, ctx);
                return true;
            }
        }

        return false;
    }

    void TieredGas_HandleAdminCommand(int rpc_type, ParamsReadContext ctx)
    {
        switch (rpc_type)
        {
            case RPC_ADMIN_LIST_ZONES:
                TieredGas_ListZones_Server();
                return;

            case RPC_ADMIN_REMOVE_ZONE:
                TieredGas_RemoveNearestZone_Server();
                return;

            case RPC_ADMIN_REMOVE_ZONE_BY_UUID:
            {
                Param1<string> pUuid;
                if (ctx.Read(pUuid))
                {
                    TieredGas_RemoveZoneByUUID_Server(pUuid.param1);
                }
                else
                {
                    SendAdminMessage("[TieredGas] Remove-by-UUID failed: bad params", true);
                }
                return;
            }

            case RPC_ADMIN_RELOAD_CONFIG:
                TieredGas_ReloadConfig_Server();
                return;

            case RPC_ADMIN_RELOAD_ADMINS:
                TieredGas_ReloadAdmins_Server();
                return;

            case RPC_ADMIN_RELOAD_ZONES:
                TieredGas_ReloadZones_Server();
                return;

            case RPC_ADMIN_SPAWN_ZONE:
            {
                ref TieredGasSpawnPayload p;
                if (ctx.Read(p))
                {
                    TieredGas_SpawnZoneHere_Server( p.tier, p.gasType, p.radius,p.zoneName, p.colorId, p.density , p.cycle, p.cycleSeconds , p.height, p.bottomOffset , p.maskRequired , p.verticalMargin );
                    return;
                }
                SendAdminMessage("[TieredGas] ERROR: Spawn RPC payload invalid", true);
                return;
            }
        }
    }

    string TieredGas_NormalizeColor(string colorId)
    {
        colorId = colorId.Trim();
        colorId.ToLower();
        if (colorId == "") return "default";
        return colorId;
    }

    string TieredGas_NormalizeDensity(string density)
    {
        density = density.Trim();
        density.ToLower();

        if (density == "") return "normal";

        if (density == "light")  return "low";
        if (density == "medium") return "normal";

        if (density == "low") return "low";
        if (density == "normal") return "normal";
        if (density == "dense") return "dense";

        return "normal";
    }

    void TieredGas_ListZones_Server()
    {
        if (!GetGame().IsServer()){ return; }

        array<ref GasZoneConfig> zones = TieredGasZoneSpawner.m_GasZones;
        if (!zones || zones.Count() == 0)
        {
            SendAdminMessage("[TieredGas] No zones in config.", false);
            return;
        }

        SendAdminMessage("[TieredGas] Zones (" + zones.Count().ToString() + "):", false);

        foreach (GasZoneConfig cfg : zones)
        {
            string line = "- " + cfg.uuid + " | " + cfg.name + " | Tier " + cfg.tier.ToString() + " | R " + cfg.radius.ToString() + " | " + cfg.position;
            if (cfg.colorId != "") { line = line + " | Color " + cfg.colorId; }
            if (cfg.density != "") { line = line + " | Density " + cfg.density; }
            SendAdminMessage(line, false);
        }
    }

    void TieredGas_SpawnZoneHere_Server(int tier, int gasType, float radius, string zoneName, string colorId, string density, bool cycle, float cycleSeconds, float height, float bottomOffset, bool maskRequired, float verticalMargin)
    {
        if (!GetGame().IsServer()) { return; }

        if (!TieredGasZoneSpawner.m_GasZones) { TieredGasZoneSpawner.m_GasZones = new array<ref GasZoneConfig>; }

        GasZoneConfig cfg = new GasZoneConfig();
            cfg.uuid = TieredGasJSON.GenerateZoneUUID();
            cfg.tier = tier;
            cfg.gasType = gasType;
            cfg.radius = radius;

            cfg.name = zoneName;
            cfg.colorId = TieredGas_NormalizeColor(colorId);
            cfg.density = TieredGas_NormalizeDensity(density);

            cfg.cycle = cycle;
            cfg.cycleSeconds = cycleSeconds;

            cfg.maskRequired = maskRequired;
            cfg.height = height;
            cfg.bottomOffset = bottomOffset;
            cfg.verticalMargin = verticalMargin;
            cfg.isDynamic = false;

        vector pos = GetPosition();
        pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
        cfg.position = pos[0].ToString() + "," + pos[1].ToString() + "," + pos[2].ToString();

        TieredGasZoneSpawner.m_GasZones.Insert(cfg);
        TieredGasJSON.SaveZonesToJSON(TieredGasZoneSpawner.m_GasZones);

        TieredGasZoneSpawner.BroadcastZonesToAll();
        SendAdminMessage("[TieredGas] Added zone: " + cfg.uuid + " (" + cfg.name + ")", false);
    }

    void PersistZoneToJSON(vector pos, int tier, int gasType, float radius, bool maskRequired, float height, float bottomOffset, float verticalMargin, string particleName, bool cycle, float cycleSeconds)
    {
        ref array<ref GasZoneConfig> zones = new array<ref GasZoneConfig>();
        TieredGasJSON.LoadZonesFromJSON(zones);

        ref GasZoneConfig z = new GasZoneConfig();
        z.uuid = TieredGasJSON.GenerateZoneUUID();
        z.name = "Gas Zone";
        z.colorId = "default";
        z.density = "normal";
        z.position = pos[0].ToString() + " 0 " + pos[2].ToString();
        z.radius = radius;
        z.tier = tier;
        z.gasType = gasType;
        z.maskRequired = maskRequired;
        z.height = height;
        z.bottomOffset = bottomOffset;
        z.verticalMargin = verticalMargin;
        z.isDynamic = false;

        z.cycle = cycle;
        z.cycleSeconds = cycleSeconds;

        zones.Insert(z);
        TieredGasJSON.SaveZonesToJSON(zones);
    }

    void TieredGas_RemoveNearestZone_Server()
    {
        if (!GetGame().IsServer()) { return; }

        array<ref GasZoneConfig> zones = TieredGasZoneSpawner.m_GasZones;
        if (!zones || zones.Count() == 0)
        {
            SendAdminMessage("[TieredGas] No zones to remove.", true);
            return;
        }

        vector pPos = GetPosition();

        int bestIdx = -1;
        float bestDistSq = 999999999.0;

        for (int i = 0; i < zones.Count(); i++)
        {
            GasZoneConfig cfg = zones[i];
            vector zPos = TieredGasZoneSpawner.ParsePositionString(cfg.position);
            zPos[1] = GetGame().SurfaceY(zPos[0], zPos[2]) - cfg.bottomOffset;

            float d = vector.DistanceSq(pPos, zPos);
            if (d < bestDistSq)
            {
                bestDistSq = d;
                bestIdx = i;
            }
        }

        if (bestIdx < 0)
        {
            SendAdminMessage("[TieredGas] No zone found.", true);
            return;
        }

        string uuid = zones[bestIdx].uuid;
        string name = zones[bestIdx].name;

        zones.Remove(bestIdx);
        TieredGasJSON.SaveZonesToJSON(zones);

        TieredGasZoneSpawner.BroadcastZonesToAll();
        SendAdminMessage("[TieredGas] Removed zone: " + uuid + " (" + name + ")", false);
    }

    void TieredGas_RemoveZoneByUUID_Server(string uuid)
    {
        if (!GetGame().IsServer()) { return; }

        if (uuid == "")
        {
            SendAdminMessage("[TieredGas] Remove failed: empty UUID", true);
            return;
        }

        bool removed = TieredGasZoneSpawner.RemoveZoneByUUID(uuid);

        if (removed)
        {
            SendAdminMessage("[TieredGas] Removed zone: " + uuid, false);
        }
        else
        {
            SendAdminMessage("[TieredGas] Remove failed: UUID not found: " + uuid, true);
        }
    }

    bool RemoveZoneFromJSON_ByMatch(vector worldPos, float matchRadius, int tier, int gasType, float radius, float height, float vmargin, string particleName, bool cycle, float cycleSeconds)
    {
        ref array<ref GasZoneConfig> zones = new array<ref GasZoneConfig>();
        if (!TieredGasJSON.LoadZonesFromJSON(zones) || zones.Count() == 0) return false;

        const float EPS_RADIUS = 0.75;
        const float EPS_HEIGHT = 0.75;
        const float EPS_VMARGIN = 0.75;
        const float EPS_CYCLE = 0.15;

        int bestIdx = -1;
        float bestDist = 999999;

        for (int i = 0; i < zones.Count(); i++)
        {
            GasZoneConfig z = zones.Get(i);
            if (!z) { continue; }

            if (z.tier != tier) { continue; }
            if (z.gasType != gasType) { continue; }

            if (Math.AbsFloat(z.radius - radius) > EPS_RADIUS) { continue; }
            if (Math.AbsFloat(z.height - height) > EPS_HEIGHT) { continue; }
            if (Math.AbsFloat(z.verticalMargin - vmargin) > EPS_VMARGIN) { continue; }

            if (z.cycle != cycle) { continue; }
            if (cycle && Math.AbsFloat(z.cycleSeconds - cycleSeconds) > EPS_CYCLE) { continue; }

            vector zpos = TieredGas_ParsePositionString(z.position);
            zpos[1] = worldPos[1];

            float d = vector.Distance(worldPos, zpos);
            if (d < bestDist)
            {
                bestDist = d;
                bestIdx = i;
            }
        }

        if (bestIdx < 0 || bestDist > matchRadius) { return false; }

        zones.Remove(bestIdx);
        TieredGasJSON.SaveZonesToJSON(zones);
        return true;
    }

    vector TieredGas_ParsePositionString(string posStr)
    {
        vector result = "0 0 0";

        posStr.Replace(",", " ");

        posStr = posStr.Trim();
        while (posStr.IndexOf("  ") >= 0)
        {
            posStr.Replace("  ", " ");
        }
        posStr = posStr.Trim();

        array<string> parts = new array<string>;
        posStr.Split(" ", parts);

        if (parts.Count() >= 3)
        {
            result[0] = parts[0].ToFloat();
            result[1] = parts[1].ToFloat();
            result[2] = parts[2].ToFloat();
        }

        return result;
    }

    void TieredGas_ReloadAdmins_Server()
    {
        TieredGasAdminList.m_AdminUIDs.Clear();
        TieredGasAdminList.Load();
        SendAdminMessage("[TieredGas] Admin list reloaded (" + TieredGasAdminList.m_AdminUIDs.Count() + ")", false);
    }

    void TieredGas_ReloadConfig_Server()
    {
        TieredGasJSON.Load(true);
        SendAdminMessage("[TieredGas] Config reloaded", false);
    }

    void TieredGas_ReloadZones_Server()
    {
        if (!GetGame().IsServer()) { return; }

        if (!TieredGasZoneSpawner.m_GasZones)
            TieredGasZoneSpawner.m_GasZones = new array<ref GasZoneConfig>;

        TieredGasZoneSpawner.m_GasZones.Clear();

        if (!TieredGasJSON.LoadZonesFromJSON(TieredGasZoneSpawner.m_GasZones) || TieredGasZoneSpawner.m_GasZones.Count() == 0)
        {
            SendAdminMessage("[TieredGas] No zones found in JSON - creating defaults", true);
            TieredGasZoneSpawner.CreateDefaultZones();
            TieredGasJSON.SaveZonesToJSON(TieredGasZoneSpawner.m_GasZones);
        }

        TieredGasZoneSpawner.UpgradeZonesIfNeeded();
        TieredGasZoneSpawner.BroadcastZonesToAll();
        SendAdminMessage("[TieredGas] Zones reloaded", false);
    }

    void SendAdminMessage(string msg, bool isError)
    {
        if (!GetIdentity()) { return; }
        Param2<string, bool> p = new Param2<string, bool>(msg, isError);
        GetGame().RPCSingleParam(this, RPC_ADMIN_MESSAGE, p, true, GetIdentity());
    }

    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);

        if (GetGame().IsServer())
        {
            if (TieredGas_HandleAdminRPC(sender, rpc_type, ctx)) { return; }
        }

        if (TieredGasClientRPC.HandleClientAdminRPC(this, sender, rpc_type, ctx)) { return; }

        if (rpc_type == RPC_TIERED_GAS_ZONES_REQUEST)
        {
            if (GetGame().IsServer())
            {
                TieredGasZoneSpawner.SendZonesToPlayer(this);
            }
            return;
        }

        if (rpc_type == RPC_TIERED_GAS_ZONES_SYNC)
        {
            if (GetGame().IsServer()) return;

            Param3<int, int, string> p3;
            if (ctx.Read(p3))
            {
                int idx = p3.param1;
                int total = p3.param2;
                string chunk = p3.param3;

                Print("[TieredGas] ZONES_SYNC recv chunk idx=" + idx + " total=" + total + " len=" + chunk.Length().ToString());

                if (!m_TG_ZonesChunks || m_TG_ZonesExpected != total)
                {
                    m_TG_ZonesChunks = new array<string>;
                    m_TG_ZonesChunks.Resize(total);
                    m_TG_ZonesExpected = total;
                    m_TG_ZonesReceived = 0;
                }

                if (idx >= 0 && idx < m_TG_ZonesChunks.Count())
                {
                    if (m_TG_ZonesChunks[idx] == "") m_TG_ZonesReceived++;
                    m_TG_ZonesChunks[idx] = chunk;
                }

                if (m_TG_ZonesReceived >= m_TG_ZonesExpected)
                {
                    array<string> chunks = m_TG_ZonesChunks;

                    m_TG_ZonesChunks = null;
                    m_TG_ZonesExpected = 0;
                    m_TG_ZonesReceived = 0;

                    ref array<ref GasZoneConfig> zones = new array<ref GasZoneConfig>;
                    string err = "";

                    if (!TieredGasJSON.ZonesFromChunks(chunks, zones, err))
                    {
                        Print("[TieredGas] ZONES_SYNC JSON parse failed: " + err);
                        return;
                    }

                    Print("[TieredGas] ZONES_SYNC OK (" + zones.Count() + " zones) [chunked]");
                    TieredGasZoneSpawner.ApplyClientZoneSync(zones);
                }
                return;
            }

            Param1<string> p1;
            if (ctx.Read(p1))
            {
                string jsonStrLegacy = p1.param1;

                ref array<ref GasZoneConfig> zones2 = new array<ref GasZoneConfig>;
                string err2 = "";

                if (!TieredGasJSON.ZonesFromJsonString(jsonStrLegacy, zones2, err2))
                {
                    Print("[TieredGas] ZONES_SYNC legacy JSON parse failed: " + err2);
                    return;
                }

                Print("[TieredGas] ZONES_SYNC OK (" + zones2.Count() + " zones) [legacy]");
                TieredGasZoneSpawner.ApplyClientZoneSync(zones2);
                return;
            }

            Print("[TieredGas] ZONES_SYNC read failed (wrong rpc payload/type)");
            return;
        }
        if (rpc_type == RPC_TIERED_GAS_UPDATE)
        {
            Param4<bool, int, int, bool> data4;
            if (ctx.Read(data4))
            {
                SetGasHUD(data4.param1, data4.param2, data4.param3);
                m_TG_ClientNerveActive = data4.param4;
                return;
            }

            Param3<bool, int, int> data3;
            if (!ctx.Read(data3)) { return; }

            SetGasHUD(data3.param1, data3.param2, data3.param3);
            return;
        }

        if (TieredGas_HandleAdminRPC(sender, rpc_type, ctx)) { return; }
    }

    override void OnScheduledTick(float deltaTime)
    {
        super.OnScheduledTick(deltaTime);

        if (!GetGame().IsDedicatedServer())
        {
            TG_ClientGasFX(deltaTime);
        }
        if (!GetGame().IsServer()) { return; }

        m_GasCheckTimer += deltaTime;
        if (m_GasCheckTimer < GAS_CHECK_INTERVAL) { return; }

        float tick = m_GasCheckTimer;
        m_GasCheckTimer = 0;

        ProcessTieredGasZones(tick);
        TG_ApplyPersistentEffects(tick);
    }

    override void EEInit()
    {
        super.EEInit();

        if (GetGame().IsServer())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TG_RestorePersistentState, 1000, false);
        }
    }
void TG_RestorePersistentState()
{
    TieredGasEffects.RestorePersistentState(this);
}


    void ProcessTieredGasZones(float tickDelta)
    {
        int bestTier = 0;
        int bestType = -1;
        bool bestMaskRequired = false;

        vector p = GetPosition();

        array<ref GasZoneConfig> zones = TieredGasZoneSpawner.m_GasZones;
        if (zones)
        {
            foreach (GasZoneConfig cfg : zones)
            {
                if (!cfg) { continue; }

                vector zp = TieredGasZoneSpawner.ParsePositionString(cfg.position);
                zp[1] = GetGame().SurfaceY(zp[0], zp[2]) - cfg.bottomOffset;

                float dx = p[0] - zp[0];
                float dz = p[2] - zp[2];
                float hSq = (dx * dx) + (dz * dz);

                float r = cfg.radius;
                if (hSq > (r * r)) { continue; }

                float dy = p[1] - zp[1];
                if (dy < 0) { continue; }
                if (dy > (cfg.height + cfg.verticalMargin)) { continue; }

                if (cfg.tier > bestTier)
                {
                    bestTier = cfg.tier;
                    bestType = cfg.gasType;
                    bestMaskRequired = cfg.maskRequired;
                }
            }
        }

        bool inGas = (bestTier > 0);
        bool nerveActiveNow = (m_TG_NervePermanent && !TG_IsNerveSuppressed());
        if (inGas && bestType < 0) { bestType = 0; }

                int nowMS = GetGame().GetTime();
        bool needSync = false;

        if (inGas != m_ClientInGas || bestTier != m_ClientTier || bestType != m_ClientType)
        {
            needSync = true;
        }

        if (nerveActiveNow != m_TG_LastSentNerveActive)
        {
            needSync = true;
        }

        if (!needSync && (nowMS - m_TG_LastGasSyncMS) >= 5000)
        {
            needSync = true;
        }

        if (needSync)
        {
            SetGasHUD(inGas, bestTier, bestType);

            if (GetIdentity())
            {
                GetGame().RPCSingleParam(this, RPC_TIERED_GAS_UPDATE, new Param4<bool,int,int,bool>(inGas, bestTier, bestType, nerveActiveNow), true, GetIdentity());
                m_TG_LastSentNerveActive = nerveActiveNow;
                m_TG_LastGasSyncMS = nowMS;
            }
        }

        if (inGas)
        {
            ApplyTieredGasDamage(this, tickDelta, bestTier, bestType, bestMaskRequired);
        }
    }
    bool TG_CanRollBleedNow()
    {
        return TieredGasEffects.CanRollBleedNow(this);
    }
    bool TG_CanRollBioNow()
    {
        return TieredGasEffects.CanRollBioNow(this);
    }
    void TG_TryCough(int gasTier, float leak)
    {
        TieredGasEffects.TryCough(this, gasTier, leak);
    }
    bool TG_TryAddBleedCut(float chance01)
    {
        return TieredGasEffects.TryAddBleedCut(this, chance01);
    }
    void TG_TryInfectToxicWound(int gasTier, float leak)
    {
        TieredGasEffects.TryInfectToxicWound(this, gasTier, leak);
    }


    StaminaHandler TG_GetStaminaHandlerSafe()
    {
        return GetStaminaHandler();
    }

    void TG_DrainStamina(float amount)
    {
        if (amount <= 0) return;
        StaminaHandler sh = TG_GetStaminaHandlerSafe();
        if (!sh) return;
        sh.DepleteStamina(amount);
    }

    void TG_ClampStaminaCap(float capMult)
    {
        if (capMult <= 0) return;
        if (capMult > 1.0) capMult = 1.0;

        StaminaHandler sh = TG_GetStaminaHandlerSafe();
        if (!sh) return;

        float maxStam = sh.GetStaminaMax();
        float cap = maxStam * capMult;

        float cur = sh.GetStamina();
        if (cur > cap)
            sh.SetStamina(cap);
    }

    void TG_SuppressNervePermanent(int durationMS)
    {
        TieredGasEffects.SuppressNervePermanent(this, durationMS);
    }

    bool TG_IsNerveSuppressed()
    {
        return TieredGasEffects.IsNerveSuppressed(this);
    }
    void TG_AddNerveExposure(float exposure)
    {
        TieredGasEffects.AddNerveExposure(this, exposure);
    }
    bool TG_HasPermanentNerveDamage()
    {
        return TieredGasEffects.HasPermanentNerveDamage(this);
    }
    void TG_SetBioInfected()
    {
        TieredGasEffects.SetBioInfected(this);
    }
    void TG_ClearBioInfection()
    {
        TieredGasEffects.ClearBioInfection(this);
    }
    int TG_GetPersistentSickStage()
    {
        return TieredGasEffects.GetPersistentSickStage(this);
    }
    bool TG_IsBioInfected()
    {
        return TieredGasEffects.IsBioInfected(this);
    }
    void TG_AddBioExposure(float exposure)
    {
        TieredGasEffects.AddBioExposure(this, exposure);
    }
    void TG_UpdateVanillaSickAgentStage(int stage)
    {
        TieredGasEffects.UpdateVanillaSickAgentStage(this, stage);
    }
    void TG_TryPermanentRespSymptoms()
    {
        TieredGasEffects.TryPermanentRespSymptoms(this);
    }
    void TG_ApplyPersistentEffects(float deltaTime)
    {
        TieredGasEffects.ApplyPersistentEffects(this, deltaTime);
    }


    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(TG_STORE_MARKER);

        ctx.Write(m_TG_NerveExposure);
        ctx.Write(m_TG_NervePermanent);
        ctx.Write(m_TG_NerveSuppressedUntilMS);

        ctx.Write(m_TG_BioExposure);
        ctx.Write(m_TG_BioInfected);
        ctx.Write(m_TG_BioNextSymptomMS);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        bool marker = false;
        if (!ctx.Read(marker))
        {
            return true;
        }

        if (!marker)
            return true;

        if (!ctx.Read(m_TG_NerveExposure)) return true;
        if (!ctx.Read(m_TG_NervePermanent)) return true;
        if (!ctx.Read(m_TG_NerveSuppressedUntilMS)) return true;

        if (!ctx.Read(m_TG_BioExposure)) return true;
        if (!ctx.Read(m_TG_BioInfected)) return true;
        if (!ctx.Read(m_TG_BioNextSymptomMS)) return true;

        if (GetGame().IsServer())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TG_RestorePersistentState, 1000, false);
        }

        return true;
    }

}