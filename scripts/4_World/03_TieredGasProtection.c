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
    // ---------------------------------------------------------------
    // Protection item lookup (admin-configurable slot)
    // ---------------------------------------------------------------
    static ItemBase GetProtectionItem(PlayerBase player)
    {
        if (!player) return null;

        string slotName = TieredGasJSON.GetProtectionSlot();
        if (!slotName || slotName.Length() == 0)
            slotName = "Armband";

        // Prefer attachments-by-slot-name for most wearable slots.
        ItemBase it = ItemBase.Cast(player.FindAttachmentBySlotName(slotName));
        if (it) return it;

        // Fallback for some slot implementations.
        return ItemBase.Cast(player.GetItemOnSlot(slotName));
    }

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

        ItemBase protectionItem = GetProtectionItem(player);
        if (!protectionItem) return 0.0;

        float maxH = protectionItem.GetMaxHealth("", "Health");
        if (maxH <= 0) return 0.0;

        float h = protectionItem.GetHealth("", "Health");
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

        ItemBase protectionItem = GetProtectionItem(player);
        if (!protectionItem) { return 0; }

        // 1) If it's the built-in NBCSuit_Base, use its tier logic.
        NBCSuit_Base suit = NBCSuit_Base.Cast(protectionItem);
        if (suit) { return suit.GetProtectionTier(); }

        // 2) Otherwise match directly against configured classnames.
        int cfgTier = TieredGasJSON.GetConfiguredProtectionTierForItem(protectionItem);
        if (cfgTier > 0) return cfgTier;

        // 3) Backwards-compatible fallback: infer from classname containing TierX.
        string t = protectionItem.GetType();
        if (t.Contains("Tier1")) return 1;
        if (t.Contains("Tier2")) return 2;
        if (t.Contains("Tier3")) return 3;
        if (t.Contains("Tier4")) return 4;

        return 0;
    }

    static bool HasValidGasMask(PlayerBase player)
    {
        ItemBase mask = ItemBase.Cast(player.FindAttachmentBySlotName("Mask"));
        if (!mask) { return false; }

        return mask.GetHealthLevel() != GameConstants.STATE_RUINED;
    }

    static bool HasGasImmunity(PlayerBase player)
    {
        ItemBase protectionItem = GetProtectionItem(player);
        if (!protectionItem) { return false; }

        string cfgPath = "CfgVehicles " + protectionItem.GetType() + " GasImmunity";
        if (GetGame().ConfigIsExisting(cfgPath)) { return GetGame().ConfigGetInt(cfgPath) == 1; }

        return false;
    }

    static void ApplyGasWear(PlayerBase player, int gasTier, float deltaTime, float tierMult = 1.0)
    {
        ItemBase protectionItem = GetProtectionItem(player);
        if (!protectionItem) { return; }

        int suitTier = GetPlayerProtectionTier(player);
        if (suitTier <= 0) { return; }
        int diff = gasTier - suitTier;
        if (diff <= 0) { return; }

        float baseWearPerSecond = 0.20;
        float diffMult          = (1.0 + diff);
        float tierMultExtra     = (1.0 + (gasTier * 0.25));

        float wear = baseWearPerSecond * diffMult * tierMultExtra * tierMult * deltaTime;
        DamageProtectionItemClamped(protectionItem, wear);
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
