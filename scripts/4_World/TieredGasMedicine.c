//---------------------------------------------------------------------------------------------------
// scripts/4_World/TieredGasMedicine.c
//
// Hooks vanilla medical items to TieredGas persistent effects.
//
// - Epinephrine (Auto-Injector): temporarily suppresses NERVE permanent debuff
// - AntiChemInjector (PO-X Antidote): cures BIO infection
// - Disinfectants: handled by vanilla wound infection system (no extra hooks)
//---------------------------------------------------------------------------------------------------

modded class Epinephrine
{
    override void OnApply(PlayerBase player)
    {
        super.OnApply(player);
        if (!GetGame().IsServer()) return;

        PlayerBase pb = PlayerBase.Cast(player);
        if (!pb) return;

        pb.TG_SuppressNervePermanent(600000);
    }
}

modded class AntiChemInjector
{
    override void OnApply(PlayerBase player)
    {
        super.OnApply(player);
        if (!GetGame().IsServer()) return;

        PlayerBase pb = PlayerBase.Cast(player);
        if (!pb) return;

        pb.TG_ClearBioInfection();
    }
}
