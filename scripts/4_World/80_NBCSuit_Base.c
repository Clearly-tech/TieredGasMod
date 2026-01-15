//---------------------------------------------------------------------------------------------------
// scripts/4_World/80_NBCSuit_Base.c
//
// File summary: NBC suit base behavior integration (ties suit parts into tiered protection/filter logic).
//
// NBCSuit_Base (modded)
//
// void EEInit()
//      Item lifecycle init hook; used to set up suit behavior.
//      Params: none
//
// void OnWasAttached(EntityAI parent, int slot_id)
//      Called when attached to a parent (typically player); can apply effects/links.
//      Params:
//          parent: entity it attached to
//          slot_id: inventory slot
//
// void OnWasDetached(EntityAI parent, int slot_id)
//      Called when detached; removes effects/links.
//      Params:
//          parent: entity detached from
//          slot_id: inventory slot
//
// bool CanPutInCargo(EntityAI parent)
//      Restricts cargo interactions if needed (suit rules).
//      Params:
//          parent: container parent
//
// bool CanPutIntoHands(EntityAI parent)
//      Restricts hands interactions if needed.
//      Params:
//          parent: entity attempting
//
// bool CanRemoveFromCargo(EntityAI parent)
//      Restricts removal rules if needed.
//      Params:
//          parent: container parent
//---------------------------------------------------------------------------------------------------

class NBCSuit_Base : Clothing
{
    protected int m_ProtectionTier = 0;
    void NBCSuit_Base()
    {
        Print("[TieredGasMod] NBC Suit Loaded");
    }

    override void OnWasAttached(EntityAI parent, int slot_id)
    {
        super.OnWasAttached(parent, slot_id);
        InitializeTier();
    }

    void InitializeTier()
    {
        string className = GetType();
        if (className.Contains("Tier1")) { m_ProtectionTier = 1; }
        else if (className.Contains("Tier2")) { m_ProtectionTier = 2; }
        else if (className.Contains("Tier3")) { m_ProtectionTier = 3; }
        else if (className.Contains("Tier4")) { m_ProtectionTier = 4; }
        else { m_ProtectionTier = 0; } 

        Print("[NBCSuit] Class: " + className + " | ProtectionTier: " + m_ProtectionTier);
    }

    int GetProtectionTier()
    {
        if (m_ProtectionTier == 0) { InitializeTier(); }
        Print("[NBCSuit] GetProtectionTier(): " + m_ProtectionTier);
        return m_ProtectionTier;
    }

    void SetProtectionTier(int tier)
    {
        m_ProtectionTier = tier;
    }

    bool IsNBCSuit()
    {
        return true;
    }
}