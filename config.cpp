class CfgPatches
{
    class TieredGasMod
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Characters",
            "DZ_Scripts",
            "DZ_Gear_Consumables",
			"JM_CF_Scripts"
        };
    };
};
class CfgMods
{
	
    class TieredGasMod
    {
		dir = "TieredGasMod";
		inputs = "TieredGasMod/Inputs/TieredGasInputs.xml";
		name = "TieredGasMod";
        picture = "";
        action = "";
        hideName = 1;
        hidePicture = 1;
        credits = "";
        author = "";
        authorID = "";

        version = "1.0";
        extra = 0;
        type = "mod";

        dependencies[] =
        {
            "Game",
			"World",
            "Mission",
        };

        class defs
        {
			class gameScriptModule
            {
                value = "";
                files[] =
                {
                    "TieredGasMod/scripts/3_Game"
                };
            };
			
            class worldScriptModule
            {
                value = "";
                files[] =
                {
                    "TieredGasMod/scripts/4_World"
                };
            };

            class missionScriptModule
            {
                value = "";
                files[] =
                {
                    "TieredGasMod/scripts/5_Mission"
                };
            };
        };
    };
};


class UIMenus
{
    class TieredGasAdminMenu
    {
        menuID = 79821;
        menu = "TieredGasAdminMenu";
    };
};

class CfgVehicles
{
	class Clothing;
	class ContaminatedTrigger;
	class Armband_ColorBase;  // Base armband class

	// ============================================================
	// NBC Suits using Armband slot
	// ============================================================
	
	// Base class – not spawnable
	class NBCSuit_Base : Armband_ColorBase
	{
		scope = 0;
		displayName = "NBC Protection Device";
		descriptionShort = "Base NBC protection device.";
		
		// Use existing Armband slot
		inventorySlot[] = {"Armband"};
		
		// Armband properties
		itemSize[] = {2,1};
		weight = 200;
		
		// Visual - using armband model
		hiddenSelections[] = {"camoGround"};
		
		class DamageSystem
		{
			class GlobalHealth
			{
				class Health
				{
					hitpoints = 100;
					healthLevels[] =
					{
						{1.0,{"DZ\characters\armbands\data\armband_black_co.paa"}},
						{0.7,{"DZ\characters\armbands\data\armband_black_co.paa"}},
						{0.5,{"DZ\characters\armbands\data\armband_black_co.paa"}},
						{0.3,{"DZ\characters\armbands\data\armband_black_co.paa"}},
						{0.0,{"DZ\characters\armbands\data\armband_black_co.paa"}}
					};
				};
			};
		};
	};

	class NBCSuit_Tier1 : NBCSuit_Base
	{
		scope = 2;
		displayName = "NBC Protection Device (Tier 1)";
		descriptionShort = "Basic NBC protection device. Attach to arm for limited gas protection against toxic gas.";
		
		hiddenSelectionsTextures[] = 
		{
			"dz\characters\armbands\data\armband_white_co.paa"
		};
	};

	class NBCSuit_Tier2 : NBCSuit_Base
	{
		scope = 2;
		displayName = "NBC Protection Device (Tier 2)";
		descriptionShort = "Moderate NBC protection device. Effective against toxic and weak nerve gas.";
		
		hiddenSelectionsTextures[] = 
		{
			"dz\characters\armbands\data\armband_yellow_co.paa"
		};
	};

	class NBCSuit_Tier3 : NBCSuit_Base
	{
		scope = 2;
		displayName = "NBC Protection Device (Tier 3)";
		descriptionShort = "Advanced NBC protection device. Protects against most nerve and bio agents.";
		
		hiddenSelectionsTextures[] = 
		{
			"dz\characters\armbands\data\armband_orange_co.paa"
		};
	};

	class NBCSuit_Tier4 : NBCSuit_Base
	{
		scope = 2;
		displayName = "NBC Protection Device (Tier 4)";
		descriptionShort = "Legendary NBC protection device. Complete immunity to all gas types.";
		GasImmunity = 1;
		
		hiddenSelectionsTextures[] = 
		{
			"dz\characters\armbands\data\armband_red_co.paa"
		};
	};
};