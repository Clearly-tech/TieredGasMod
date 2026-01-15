//---------------------------------------------------------------------------------------------------
// scripts/4_World/05_TieredGasZoneSpawner.c
//
// File summary: Server zone manager: loads zone configs, creates/removes TieredGasZone, supports admin RPC actions (spawn/remove/reload).
//
// TieredGasZoneSpawner
//
// void Init()
//      Server-side init: loads zones JSON, creates defaults if missing, upgrades older formats if needed.
//      Params: none
//
// void CreateDefaultZones()
//      Creates a default set of zones in memory (used when JSON empty/missing).
//      Params: none
//
// void UpgradeZonesIfNeeded()
//      Migrates/patches zone data when config schema changes.
//      Params: none
//
// TieredGasZone SpawnZone(GasZoneConfig cfg)
//      Spawns a TieredGasZone world object from a config record.
//      Params:
//          cfg: zone config
//
// bool RemoveZone(TieredGasZone zone, float fadeSeconds = 0)
//      Removes a zone instance (optionally with fade/cleanup time).
//      Params:
//          zone: instance to remove
//          fadeSeconds: visual fade duration
//
// bool RemoveZoneByUUID(string uuid, float fadeSeconds = 0)
//      Removes zone matching UUID.
//      Params:
//          uuid: zone ID
//          fadeSeconds: fade duration
//
// GasZoneConfig FindZoneConfigByUUID(string uuid)
//      Finds the config entry matching UUID.
//      Params:
//          uuid: zone ID
//
// GasZoneConfig FindNearestZoneConfig(vector pos, float maxDist)
//      Finds nearest zone config to a position within distance.
//      Params:
//          pos: world position
//          maxDist: max search radius
//
// void ReloadConfig()
//      Reloads TieredGas settings/config from disk.
//      Params: none
//
// void ReloadZones()
//      Reloads zone list from disk and refreshes spawned zones.
//      Params: none
//
// void HandleAdminSpawnZone(PlayerIdentity sender, ParamsReadContext ctx)
//      RPC handler: admin requests spawning a zone from parameters.
//      Params:
//          sender: requesting identity
//          ctx: RPC payload reader
//
// void HandleAdminRemoveZone(PlayerIdentity sender, ParamsReadContext ctx)
//      RPC handler: admin requests removing a zone (by nearest/uuid depending on payload).
//      Params:
//          sender: requesting identity
//          ctx: RPC payload reader
//
// void HandleAdminReloadZones(PlayerIdentity sender)
//      RPC handler: admin requests reloading zones.
//      Params:
//          sender: requesting identity
//---------------------------------------------------------------------------------------------------

class TieredGasZoneSpawner
{
    static ref array<ref GasZoneConfig> m_GasZones;
    static ref map<string, TieredGasZone> m_ClientZonesByUUID;
    static ref map<string, ref GasZoneConfig> m_ClientConfigsByUUID;
    static const int ZONES_RPC_CHUNK_SIZE = 900;

    static void Init()
    {
        if (GetGame().IsServer())
        {
            if (!m_GasZones) { m_GasZones = new array<ref GasZoneConfig>; }

            if (!TieredGasJSON.LoadZonesFromJSON(m_GasZones) || m_GasZones.Count() == 0)
            {
                Print("[TieredGas] No zones found in JSON, creating default zones...");
                CreateDefaultZones();
                TieredGasJSON.SaveZonesToJSON(m_GasZones);
            }

            UpgradeZonesIfNeeded();
            return;
        }
        if (!m_ClientZonesByUUID) { m_ClientZonesByUUID = new map<string, TieredGasZone>; }
        if (!m_ClientConfigsByUUID) { m_ClientConfigsByUUID = new map<string, ref GasZoneConfig>; }
    }

    static void UpgradeZonesIfNeeded()
    {
        if (!m_GasZones) { return; }

        bool changed = false;

        foreach (GasZoneConfig cfg : m_GasZones)
        {
            if (!cfg) { continue; }

            if (cfg.uuid == "")
            {
                cfg.uuid = TieredGasJSON.GenerateZoneUUID();
                changed = true;
            }

            if (cfg.name == "")
            {
                cfg.name = "Gas Zone";
                changed = true;
            }
            if (cfg.colorId == "") { cfg.colorId = "default"; changed = true; }
            if (cfg.density == "") { cfg.density = "normal"; changed = true; }
        }

        if (changed)
        {
            TieredGasJSON.SaveZonesToJSON(m_GasZones);
        }
    }

    static vector ParsePositionString(string posStr)
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
        else
        {
            Print("[TieredGas] ERROR: Invalid position string: " + posStr);
        }

        return result;
    }

    static void SendZonesToPlayer(PlayerBase player)
    {
        if (!GetGame().IsServer() || !player || !player.GetIdentity()) return;

        UpgradeZonesIfNeeded();

        JsonSerializer js = new JsonSerializer();
        string jsonStr;
        js.WriteToString(m_GasZones, true, jsonStr);

        int len = jsonStr.Length();
        int total = Math.Ceil(len / (float)ZONES_RPC_CHUNK_SIZE);

        for (int i = 0; i < total; i++)
        {
            int start = i * ZONES_RPC_CHUNK_SIZE;
            int count = ZONES_RPC_CHUNK_SIZE;
            if (start + count > len) count = len - start;

            string chunk = jsonStr.Substring(start, count);

            Param3<int, int, string> p = new Param3<int, int, string>(i, total, chunk);
            GetGame().RPCSingleParam(player, RPC_TIERED_GAS_ZONES_SYNC, p, true, player.GetIdentity());

            Print("[TieredGas] ZONES_SYNC chunk " + i + "/" + total + " len=" + chunk.Length().ToString());
        }

        Print("[TieredGas] SendZonesToPlayer -> chunks=" + total + " bytes=" + len);
    }

    static void BroadcastZonesToAll()
    {
        if (!GetGame().IsServer()) { return; }

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        foreach (Man m : players)
        {
            PlayerBase p = PlayerBase.Cast(m);
            if (p) { SendZonesToPlayer(p); }
        }
    }

    static bool ArrayContainsString(array<string> arr, string value)
    {
        if (!arr) return false;
        for (int i = 0; i < arr.Count(); i++)
        {
            if (arr[i] == value)
                return true;
        }
        return false;
    }

    static void ApplyClientZoneSync(array<ref GasZoneConfig> zones)
    {
        if (GetGame().IsServer()) { return; }

        if (!m_ClientZonesByUUID) { m_ClientZonesByUUID = new map<string, TieredGasZone>; }
        if (!m_ClientConfigsByUUID) { m_ClientConfigsByUUID = new map<string, ref GasZoneConfig>; }

        if (!zones)
        {
            return;
        }

        ref array<string> incomingUUIDs = new array<string>();
        for (int zi = 0; zi < zones.Count(); zi++)
        {
            GasZoneConfig cfgIn = zones[zi];
            if (!cfgIn) continue;
            if (cfgIn.uuid == "") continue;

            if (!ArrayContainsString(incomingUUIDs, cfgIn.uuid))
                incomingUUIDs.Insert(cfgIn.uuid);
        }

        ref array<string> toDelete = new array<string>();
        foreach (string uuidOld, TieredGasZone zoneOld : m_ClientZonesByUUID)
        {
            if (!ArrayContainsString(incomingUUIDs, uuidOld))
            {
                toDelete.Insert(uuidOld);
            }
        }

        for (int di = 0; di < toDelete.Count(); di++)
        {
            string delUuid = toDelete[di];
            TieredGasZone delZone = m_ClientZonesByUUID.Get(delUuid);
            if (delZone)
            {
                GetGame().ObjectDelete(delZone);
            }
            m_ClientZonesByUUID.Remove(delUuid);
            if (m_ClientConfigsByUUID) m_ClientConfigsByUUID.Remove(delUuid);
        }

        for (int ui = 0; ui < zones.Count(); ui++)
        {
            GasZoneConfig cfg = zones[ui];
            if (!cfg) continue;
            if (cfg.uuid == "") continue;
            if (cfg.name == "") cfg.name = "Gas Zone";

            m_ClientConfigsByUUID.Set(cfg.uuid, cfg);

            TieredGasZone zone = null;
            if (m_ClientZonesByUUID.Contains(cfg.uuid))
            {
                zone = m_ClientZonesByUUID.Get(cfg.uuid);
            }

            vector pos = ParsePositionString(cfg.position);
            pos[1] = GetGame().SurfaceY(pos[0], pos[2]);

            if (!zone)
            {
                zone = TieredGasZone.Cast(GetGame().CreateObjectEx("TieredGasZone", pos, ECE_LOCAL));
                if (!zone) continue;
                m_ClientZonesByUUID.Set(cfg.uuid, zone);
            }

            zone.SetPosition(pos);
            zone.ApplyConfig(cfg.uuid, cfg.name, cfg.colorId, cfg.density, cfg.tier, cfg.gasType, cfg.radius, cfg.maskRequired, cfg.height, cfg.bottomOffset, cfg.verticalMargin, cfg.isDynamic);
        }
    }

    static void AddZoneAndSave(GasZoneConfig cfg)
    {
        if (!GetGame().IsServer()) { return; }
        if (!m_GasZones) { m_GasZones = new array<ref GasZoneConfig>; }

        if (cfg.uuid == "") { cfg.uuid = TieredGasJSON.GenerateZoneUUID(); }
        if (cfg.name == "") { cfg.name = "Gas Zone"; }

        m_GasZones.Insert(cfg);
        TieredGasJSON.SaveZonesToJSON(m_GasZones);
        BroadcastZonesToAll();
    }

    static bool RemoveZoneByUUID(string uuid)
    {
        if (!GetGame().IsServer()) { return false; }
        if (!m_GasZones) { return false; }

        for (int i = m_GasZones.Count() - 1; i >= 0; i--)
        {
            if (m_GasZones[i] && m_GasZones[i].uuid == uuid)
            {
                m_GasZones.Remove(i);
                TieredGasJSON.SaveZonesToJSON(m_GasZones);
                BroadcastZonesToAll();
                return true;
            }
        }

        return false;
    }

    static GasZoneConfig FindNearestZoneConfig(vector pos, float maxDist)
    {
        if (!m_GasZones) { return null; }

        float best = maxDist * maxDist;
        GasZoneConfig bestCfg;

        foreach (GasZoneConfig cfg : m_GasZones)
        {
            if (!cfg) { continue; }

            vector zpos = ParsePositionString(cfg.position);
            float d2 = vector.DistanceSq(pos, zpos);

            if (d2 < best)
            {
                best = d2;
                bestCfg = cfg;
            }
        }

        return bestCfg;
    }

    static void CreateDefaultZones()
    {
        if (!m_GasZones) { m_GasZones = new array<ref GasZoneConfig>; }

        ref GasZoneConfig z = new GasZoneConfig;
        z.uuid = TieredGasJSON.GenerateZoneUUID();
        z.name = "Default Gas Zone";
        z.colorId = "default";
        z.density = "normal";
        z.position = "100 0 100";
        z.radius = 50;
        z.tier = 2;
        z.gasType = 0;
        z.maskRequired = true;
        z.height = 10;
        z.bottomOffset = 0;
        z.verticalMargin = 2;
        z.isDynamic = false;

        z.cycle = false;
        z.cycleSeconds = 3.0;

        m_GasZones.Insert(z);
    }

    static void Cleanup()
    {
        if (GetGame().IsServer())
        {
            if (m_GasZones) m_GasZones.Clear();
            return;
        }

        if (m_ClientZonesByUUID)
        {
            foreach (string k, TieredGasZone z : m_ClientZonesByUUID)
            {
                if (z) GetGame().ObjectDelete(z);
            }
            m_ClientZonesByUUID.Clear();
        }

        if (m_ClientConfigsByUUID)
        {
            m_ClientConfigsByUUID.Clear();
        }
    }
};
