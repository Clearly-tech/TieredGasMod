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

    static void Load()
    {
        string folder = "$profile:TieredGas";
        string path = folder + "/AdminList.json";

        if (!FileExist(folder)) MakeDirectory(folder);

        if (!FileExist(path))
        {
            m_AdminUIDs.Insert("YOUR_UUID_HERE");
            JsonFileLoader<ref array<string>>.JsonSaveFile(path, m_AdminUIDs);
            return;
        }

        JsonFileLoader<ref array<string>>.JsonLoadFile(path, m_AdminUIDs);
    }

    static bool IsAdmin(PlayerBase player)
    {
        if (!player) { return false; }
        PlayerIdentity id = player.GetIdentity();
        if (!id) { return false; }
        return m_AdminUIDs.Find(id.GetId()) != -1;
    }
}
