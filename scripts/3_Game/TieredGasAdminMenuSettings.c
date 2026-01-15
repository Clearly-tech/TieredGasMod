//---------------------------------------------------------------------------------------------------
// scripts/3_Game/TieredGasAdminMenuSettings.c
//
// File summary: Loads/saves admin-menu client settings (currently Enabled) in $profile:TieredGas/AdminMenuSettings.json.
//
// TieredGasAdminMenuSettings
//
// string GetFolder()
//      Returns the settings folder path ($profile:TieredGas + subfolder used by the admin menu settings).
//      Params: none
//
// string GetPath()
//      Returns the full JSON file path for admin menu settings.
//      Params: none
//
// void Load(bool forceReload = false)
//      Loads settings JSON into cached s_Data. Creates folder/file defaults if missing.
//      Params:
//          forceReload: if true, re-reads from disk even if already cached
//
// TieredGasAdminMenuSettingsData Get()
//      Returns cached settings data (loads first if needed).
//      Params: none
//
// bool IsEnabled()
//      Convenience getter for Enabled (loads first if needed).
//      Params: none
//---------------------------------------------------------------------------------------------------

class TieredGasAdminMenuSettingsData
{
    bool Enabled = false;
}

class TieredGasAdminMenuSettings
{
    private static ref TieredGasAdminMenuSettingsData s_Data;
    private static bool s_Loaded;

    static string GetFolder()
    {
        return "$profile:TieredGas";
    }

    static string GetPath()
    {
        return GetFolder() + "/AdminMenuSettings.json";
    }

    static void Load(bool forceReload = false)
    {
        if (s_Loaded && !forceReload)
            return;

        string folder = GetFolder();
        if (!FileExist(folder))
            MakeDirectory(folder);

        string path = GetPath();

        s_Data = new TieredGasAdminMenuSettingsData();

        if (FileExist(path))
        {
            JsonFileLoader<TieredGasAdminMenuSettingsData>.JsonLoadFile(path, s_Data);
        }
        else
        {
            JsonFileLoader<TieredGasAdminMenuSettingsData>.JsonSaveFile(path, s_Data);
        }

        s_Loaded = true;
    }

    static TieredGasAdminMenuSettingsData Get()
    {
        Load();
        return s_Data;
    }

    static bool IsEnabled()
    {
        Load();
        return s_Data && s_Data.Enabled;
    }
}
