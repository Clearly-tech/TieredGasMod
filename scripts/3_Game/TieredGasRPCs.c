//---------------------------------------------------------------------------------------------------
// scripts/3_Game/TieredGasRPCs.c
//
// File summary: Shared constants: RPC IDs + menu ID used by TieredGas.
//
// Functions: none (constants only).
//---------------------------------------------------------------------------------------------------

const int RPC_TIERED_GAS_UPDATE        = 90001; 
const int RPC_TIERED_GAS_ZONES_REQUEST = 90002; 
const int RPC_TIERED_GAS_ZONES_SYNC    = 90003; 

const int RPC_ADMIN_LIST_ZONES        = 90010;
const int RPC_ADMIN_SPAWN_ZONE        = 90011;
const int RPC_ADMIN_REMOVE_ZONE       = 90012;
const int RPC_ADMIN_RELOAD_CONFIG     = 90013;
const int RPC_ADMIN_RELOAD_ADMINS     = 90014;
const int RPC_ADMIN_MESSAGE           = 90015;
const int RPC_ADMIN_CHECK             = 90016;
const int RPC_ADMIN_CHECK_RESPONSE    = 90017;
const int RPC_ADMIN_RELOAD_ZONES      = 90018;
const int RPC_ADMIN_REMOVE_ZONE_BY_UUID = 90019;

const int MENU_TIEREDGAS_ADMIN        = 91000;

// Payload for Spawn Here cause too many Params
class TieredGasSpawnPayload : Param
{
    int tier;
    int gasType;
    float radius;

    string zoneName;
    string colorId;
    string density;

    bool cycle;
    float cycleSeconds;

    float height;
    float bottomOffset;

    bool maskRequired;
    float verticalMargin;
}