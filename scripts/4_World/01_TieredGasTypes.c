//---------------------------------------------------------------------------------------------------
// scripts/4_World/01_TieredGasTypes.c
//
// File summary: Gas type conversion helpers (enum/int ↔ string).
//
// Functions
//
// string GasTypeToString(int gasType)
//      Converts a numeric gas type ID to its string form used in configs/UI.
//      Params:
//          gasType: type identifier
//
// int GasStringToType(string s)
//      Parses a string gas type name into the numeric ID.
//      Params:
//          s: string type name
//---------------------------------------------------------------------------------------------------

enum TieredGasType
{
    TOXIC = 0,
    NERVE = 1,
    BIO   = 2
};

class TieredGasTypes
{
    static string GasTypeToString(int type)
    {
        switch (type)
        {
            case TieredGasType.NERVE: return "NERVE";
            case TieredGasType.BIO:   return "BIO";
        }
        return "TOXIC";
    }

    static int GasStringToType(string name)
    {
        name.ToUpper();

        if (name == "NERVE") { return TieredGasType.NERVE; }
        if (name == "BIO")   { return TieredGasType.BIO; }
        return TieredGasType.TOXIC;
    }
};
