//---------------------------------------------------------------------------------------------------
// scripts/5_Mission/TieredGasServer.c
//
// File summary: Server mission hooks: ensures $profile:TieredGas folder exists and initializes server systems.
//
// MissionServer (modded)
//
// void OnInit()
//      Server init: creates profile folder if missing; triggers zone spawner init.
//      Params: none
//
// void OnMissionFinish()
//      Cleanup when mission ends (stop timers/cleanup server state).
//      Params: none
//---------------------------------------------------------------------------------------------------

modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();

        Print("==============================================");
        Print("[TieredGasMod] Server Initialization Starting");
        Print("==============================================");

        string folder = "$profile:TieredGas";
        if (!FileExist(folder))
        {
            MakeDirectory(folder);
            Print("[TieredGasMod] Created config folder: " + folder);
        }

        TieredGasAdminList.Load();
        TieredGasAdminMenuSettings.Load();
        TieredGasJSON.Load();
        TieredGasZoneSpawner.Init();

        Print("==============================================");
        Print("[TieredGasMod] Initialization Complete");
        Print("==============================================");
    }

    override void OnMissionFinish()
    {
        Print("[TieredGasMod] Server shutting down...");
        TieredGasZoneSpawner.Cleanup();
        Print("[TieredGasMod] Cleanup complete");
        super.OnMissionFinish();
    }
}
