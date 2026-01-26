//---------------------------------------------------------------------------------------------------
// scripts/4_World/TieredGasAdminList.c
//
// File summary: Loads and checks admin whitelist from $profile:TieredGas/Admins.json.
//
// TieredGasAdminList
//
// void Load(bool forceReload = false)
//      Loads admin list JSON (creates defaults if missing).
//      Params:
//          forceReload: reload even if already cached
//
// bool IsAdmin(PlayerIdentity identity)
//      Checks whether an identity is listed as admin.
//      Params:
//          identity: player identity to check
//---------------------------------------------------------------------------------------------------

class TieredGasAdminList
{
    static ref array<string> m_AdminUIDs = new array<string>;

    static bool m_Loaded;

    static void Load()
    {
        TieredGasJSON.LoadAdminUIDs(m_AdminUIDs);
        m_Loaded = true;
    }

    static bool IsAdmin(PlayerBase player)
    {
        if (!player) { return false; }
        PlayerIdentity id = player.GetIdentity();
        if (!id) { return false; }
        if (!m_Loaded) { Load(); }
        return m_AdminUIDs.Find(id.GetId()) != -1;
    }
}
