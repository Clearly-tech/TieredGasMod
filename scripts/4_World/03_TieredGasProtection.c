//---------------------------------------------------------------------------------------------------
// scripts/4_World/03_TieredGasProtection.c
//
// File summary: Evaluates gear protection vs gas tier/type; handles mask validity, immunity, wear, and filter drain.
//
// TieredGasProtection
//
// int GetEffectiveProtectionTier(PlayerBase player, int gasTier, Object zone)
//      Computes protection tier actually applied (player gear + zone rules + gas tier).
//      Params:
//          player: player being evaluated
//          gasTier: zone gas tier
//          zone: zone object (used if zone-specific modifiers apply)
//
// int GetPlayerProtectionTier(PlayerBase player)
//      Computes player’s protection tier based on equipped items.
//      Params:
//          player: player
//
// bool HasValidGasMask(PlayerBase player)
//      Checks if player has a valid/functional gas mask equipped.
//      Params:
//      player: player
//
// bool HasGasImmunity(PlayerBase player)
//      Checks if player is immune to gas effects (special items/status).
//      Params:
//          player: player
//
// void ApplyGasWear(PlayerBase player, int gasTier, float deltaTime)
//      Applies durability wear to gear due to gas exposure over time.
//      Params:
//          player: player
//          gasTier: severity tier
//          deltaTime: time step used for scaling wear
//
// void DrainGasFilter(PlayerBase player, float deltaTime, int gasType)
//      Drains gas filter/consumable over time while in gas.
//      Params:
//          player: player
//          deltaTime: time step
//          gasType: gas type (can drain differently by type)
//---------------------------------------------------------------------------------------------------

class TieredGasProtection
{
    // Gas wear should never ruin tiered protection items; clamp to a minimum health cap.
    static void DamageProtectionItemClamped(ItemBase item, float damage)
    {
        if (!item || damage <= 0) return;

        // Use same health component used elsewhere in this mod
        string hpSel = "Health";

        float maxH = item.GetMaxHealth("", hpSel);
        if (maxH <= 0)
        {
            // Fallback if selection differs
            maxH = item.GetMaxHealth("", "");
            hpSel = "";
        }
        if (maxH <= 0) return;

        float cap01 = TieredGasJSON.GetProtectionMinHealthCap();
        float minH  = maxH * cap01;

        float curH = item.GetHealth("", hpSel);
        if (curH <= minH) return; // already at/under cap

        float newH = curH - damage;
        if (newH < minH) newH = minH;

        item.SetHealth("", hpSel, newH);
    }

    static float GetSuitIntegrity01(PlayerBase player)
    {
        if (!player) return 0.0;

        ItemBase armband = ItemBase.Cast(player.GetItemOnSlot("Armband"));
        if (!armband) return 0.0;

        float maxH = armband.GetMaxHealth("", "Health");
        if (maxH <= 0) return 0.0;

        float h = armband.GetHealth("", "Health");
        float r = h / maxH;
        if (r < 0) r = 0;
        if (r > 1) r = 1;
        return r;
    }

    static int GetEffectiveProtectionTier(PlayerBase player, int gasTier, Object zone)
    {
        if (!player || !zone) return 0;

        int suitTier = GetPlayerProtectionTier(player);
        if (suitTier <= 0) return 0;

        bool maskRequired = TieredGasJSON.GetZoneRequiresMask(zone);

        if (maskRequired && !HasValidGasMask(player))
            return 0;

        return suitTier;
    }

    static int GetPlayerProtectionTier(PlayerBase player)
    {
        if (!player) { return 0; }

        ItemBase armband = ItemBase.Cast(player.GetItemOnSlot("Armband"));
        if (!armband) { return 0; }

        NBCSuit_Base suit = NBCSuit_Base.Cast(armband);
        if (!suit) { return 0; }

        return suit.GetProtectionTier();
    }

    static bool HasValidGasMask(PlayerBase player)
    {
        ItemBase mask = ItemBase.Cast(player.FindAttachmentBySlotName("Mask"));
        if (!mask) { return false; }

        return mask.GetHealthLevel() != GameConstants.STATE_RUINED;
    }

    static bool HasGasImmunity(PlayerBase player)
    {
        ItemBase armband = ItemBase.Cast(player.GetItemOnSlot("Armband"));
        if (!armband) { return false; }

        NBCSuit_Base suit = NBCSuit_Base.Cast(armband);
        if (!suit) { return false; }

        string cfgPath = "CfgVehicles " + armband.GetType() + " GasImmunity";
        if (GetGame().ConfigIsExisting(cfgPath)) { return GetGame().ConfigGetInt(cfgPath) == 1; }

        return false;
    }

    static void ApplyGasWear(PlayerBase player, int gasTier, float deltaTime, float tierMult = 1.0)
    {
        ItemBase armband = ItemBase.Cast(player.GetItemOnSlot("Armband"));
        if (!armband) { return; }

        NBCSuit_Base suit = NBCSuit_Base.Cast(armband);
        if (!suit) { return; }

        int suitTier = suit.GetProtectionTier();
        int diff = gasTier - suitTier;
        if (diff <= 0) { return; }

        float baseWearPerSecond = 0.20;
        float diffMult          = (1.0 + diff);
        float tierMultExtra     = (1.0 + (gasTier * 0.25));

        float wear = baseWearPerSecond * diffMult * tierMultExtra * tierMult * deltaTime;
        DamageProtectionItemClamped(armband, wear);
    }

    static void DrainGasFilter(PlayerBase player, float deltaTime, int gasType, int gasTier)
    {
        ItemBase mask = ItemBase.Cast(player.FindAttachmentBySlotName("Mask"));
        if (!mask) { return; }

        float drainRate = TieredGasJSON.GetFilterDrain(gasType);

        GasTierData tierData = TieredGasJSON.GetTier(gasTier);
        if (tierData)
            drainRate *= tierData.filterMultiplier;

        ItemBase filter;
        int slotFilter = InventorySlots.GetSlotIdFromString("GasMaskFilter");
        if (slotFilter >= 0)
            filter = ItemBase.Cast(mask.GetInventory().FindAttachment(slotFilter));

        if (!filter)
            filter = ItemBase.Cast(mask.FindAttachmentBySlotName("GasFilter"));

        if (filter)
        {
            if (filter.HasQuantity())
            {
                float newQty = filter.GetQuantity() - (drainRate * deltaTime);
                filter.SetQuantity(Math.Max(newQty, 0));
            }
            else
            {
                filter.AddHealth("", "Health", -(drainRate * deltaTime));
            }
            return;
        }

        const float MASK_DRAIN_RATIO = 0.10; 
        float maskDrain = drainRate * MASK_DRAIN_RATIO * deltaTime;
        mask.AddHealth("", "Health", -maskDrain);
    }
};
