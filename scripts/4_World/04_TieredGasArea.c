//---------------------------------------------------------------------------------------------------
// scripts/4_World/04_TieredGasArea.c
//
// File summary: Gas damage/effects application (server-side). Called from PlayerBase tick.
//
// Key behavior:
// - "NBC suit" is a single armband item (NBCSuit_Base) providing a protection tier.
// - If suitTier >= gasTier AND (mask not required OR player has valid mask) => immune.
// - If player enters higher tier than suit => suit takes durability damage.
// - Player effects only begin once the suit is actually damaged (leak model).
// - Blood damage is replaced by bleeding cuts (chance roll every 5 seconds).
// - Mask requirement is controlled PER-ZONE via GasZones.json (cfg.maskRequired).
//---------------------------------------------------------------------------------------------------

void ApplyTieredGasDamage(PlayerBase player, float deltaTime, int gasTier, int gasType, bool maskRequired)
{
    if (!player || !player.IsAlive()) { return; }
    if (!GetGame().IsServer()) { return; }
    if (TieredGasProtection.HasGasImmunity(player)) { return; }

    GasTypeData data = TieredGasJSON.GetGasType(TieredGasTypes.GasTypeToString(gasType));
    if (!data) { return; }

    GasTierData tierData = TieredGasJSON.GetTier(gasTier);
    float tierMult = 1.0;
    if (tierData) { tierMult = tierData.damageMultiplier; }

    int suitTier = TieredGasProtection.GetPlayerProtectionTier(player);

    int effectiveTier = suitTier;
    if (maskRequired && !TieredGasProtection.HasValidGasMask(player))
        effectiveTier = 0;

    if (effectiveTier >= gasTier && effectiveTier > 0)
    {
        if (maskRequired)
            TieredGasProtection.DrainGasFilter(player, deltaTime, gasType, gasTier);

        return;
    }


    if (suitTier > 0)
        TieredGasProtection.ApplyGasWear(player, gasTier, deltaTime, tierMult);

    // Leak model:
    // - With protection equipped, effects ramp in gradually as the suit's integrity
    //   drops below a configurable threshold.
    // - This avoids a "binary" on/off leak and also works correctly if protection
    //   items are capped above 0% (e.g. never go ruined).
    float leak = 1.0;
    float leakStart = TieredGasJSON.GetProtectionLeakThreshold();

    if (suitTier > 0)
    {
        float integrity = TieredGasProtection.GetSuitIntegrity01(player);

        // If admins set an invalid/zero threshold, fall back to full leak once damaged.
        if (leakStart <= 0.0)
        {
            if (integrity < 1.0)
            {
                leak = 1.0;
            }
            else
            {
                leak = 0.0;
            }
        }
        else
        {
            // No leak above threshold.
            if (integrity >= leakStart)
            {
                leak = 0.0;
            }
            else
            {
                // Ramp leak 0..1 as integrity goes from leakStart down to 0.
                leak = (leakStart - integrity) / leakStart;
                if (leak < 0.0) leak = 0.0;
                if (leak > 1.0) leak = 1.0;
            }
        }
    }

    if (maskRequired && !TieredGasProtection.HasValidGasMask(player))
        leak = 1.0;

    if (leak > 0.0)
    {
        float mult = tierMult * leak;

        if (data.cough && TieredGasJSON.AllowsTierEffect("COUGH", gasTier))
        {
            player.TG_TryCough(gasTier, leak);
        }

        if (gasType == TieredGasType.NERVE)
        {
            float stamDrain = (5.0 + (gasTier * 2.0)) * mult * deltaTime;
            player.TG_DrainStamina(stamDrain);

            if (TieredGasJSON.AllowsPermanentEffect("NERVE_PERMANENT", gasTier))
                player.TG_AddNerveExposure(leak * deltaTime * (1.0 + (gasTier * 0.25)));
        }

        if (gasType == TieredGasType.BIO)
        {
            if (TieredGasJSON.AllowsPermanentEffect("BIO_INFECTION", gasTier))
                player.TG_AddBioExposure(leak * deltaTime * (1.0 + (gasTier * 0.25)));

            if (TieredGasJSON.AllowsPermanentEffect("BIO_INFECTION", gasTier) && !player.TG_IsBioInfected() && player.TG_CanRollBioNow())
            {
                float baseInf = TieredGasJSON.GetBioInfectionChanceForTier(gasTier);
                float capInf  = TieredGasJSON.GetBioInfectionChanceCap();
                float infChance = baseInf * leak;
                if (infChance > capInf) infChance = capInf;

                if (Math.RandomFloatInclusive(0.0, 1.0) <= infChance)
                    player.TG_SetBioInfected();
}
        }

        player.DecreaseHealth("", "Health", data.healthDamage * mult * deltaTime);
        player.AddHealth("", "Shock", -data.shockDamage * mult * deltaTime);

        if (gasType == TieredGasType.TOXIC && player.TG_CanRollBleedNow())
        {
            float baseChance = TieredGasJSON.GetToxicBleedChanceForTier(gasTier);
            float capChance  = TieredGasJSON.GetToxicBleedChanceCap();

            float chance = baseChance * leak;
            if (chance > capChance) chance = capChance;
            bool added = player.TG_TryAddBleedCut(chance);
            if (added)
            {
                if (TieredGasJSON.AllowsPermanentEffect("TOXIC_WOUND", gasTier))
                    player.TG_TryInfectToxicWound(gasTier, leak);
            }
        }
    }

    if (maskRequired)
        TieredGasProtection.DrainGasFilter(player, deltaTime, gasType, gasTier);

}
