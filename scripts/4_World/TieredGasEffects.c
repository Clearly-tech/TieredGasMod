//---------------------------------------------------------------------------------------------------
// scripts/4_World/TieredGasEffects.c
//
// Effect and permanent-effect logic
// All methods are static and operate on PlayerBase
//---------------------------------------------------------------------------------------------------

class TieredGasEffects
{
    static void ClientGasFX(PlayerBase player, float deltaTime, bool inGas, int tier, int gasType, bool nervePermanentActive)
    {
        if (GetGame().IsServer()) return;
        if (!player) return;

        float blurTarget = 0.0;

        if (inGas && tier > 0)
        {
            GasTypeData d = TieredGasJSON.GetGasType(TieredGasTypes.GasTypeToString(gasType));
            if (d && d.blur && TieredGasJSON.AllowsTierEffect("BLUR", tier))
            {
                blurTarget = TieredGasJSON.GetGasBlurForTier(tier);

            }
        }

        if (nervePermanentActive)
        {
            int fxTier = tier;
            if (fxTier < 1) fxTier = 1;

            int nowMS = GetGame().GetTime();

            if (nowMS >= player.m_TG_NextPermBlurMS)
            {
                player.m_TG_NextPermBlurMS  = nowMS + Math.RandomInt(20000, 45000);
                player.m_TG_PermBlurUntilMS = nowMS + Math.RandomInt(1500, 3000);
            }

            float minBlur = TieredGasJSON.GetNerveBlurMinForTier(fxTier);

            if (nowMS < player.m_TG_PermBlurUntilMS)
            {
                float spikeMin = TieredGasJSON.GetNerveBlurSpikeMinForTier(fxTier);
                if (minBlur < spikeMin) minBlur = spikeMin;
            }

            if (blurTarget < minBlur) blurTarget = minBlur;

            float speed = 4.0 + (fxTier * 1.0);

            float s = Math.AbsFloat(Math.Sin((nowMS * 0.001) * speed));


        }

        player.m_TG_BlurTarget = blurTarget;


        float t = 5.0 * deltaTime;
        if (t > 1.0) t = 1.0;

        player.m_TG_BlurCurrent = player.m_TG_BlurCurrent + (player.m_TG_BlurTarget - player.m_TG_BlurCurrent) * t;

        PPEffects.SetBlur(player.m_TG_BlurCurrent);
        
    }

    static void RestorePersistentState(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        if (!player.IsAlive()) return;

        int stage = TieredGasEffects.GetPersistentSickStage(player);
        TieredGasEffects.UpdateVanillaSickAgentStage(player, stage);

        int now = GetGame().GetTime();
        if (stage > 0)
        {
            player.m_TG_NextCoughMS  = now + Math.RandomInt(15000, 30000);
            player.m_TG_NextSneezeMS = now + Math.RandomInt(20000, 40000);
        }
    }

    static bool CanRollBleedNow(PlayerBase player)
    {
        int now = GetGame().GetTime(); 
        if (now < player.m_TG_NextBleedRollMS) return false;

        player.m_TG_NextBleedRollMS = now + 5000; 
        return true;
    }

    static bool CanRollBioNow(PlayerBase player)
    {
        int now = GetGame().GetTime();
        if (now < player.m_TG_NextBioRollMS) return false;

        player.m_TG_NextBioRollMS = now + 5000; 
        return true;
    }

    static void TryCough(PlayerBase player, int gasTier, float leak)
    {
        if (!GetGame().IsServer()) return;
        if (!player.IsAlive()) return;

        if (gasTier < 1) gasTier = 1;
        if (leak < 0.0) leak = 0.0;
        if (leak > 1.0) leak = 1.0;

        int now = GetGame().GetTime();
        if (now < player.m_TG_NextCoughMS) return;

        int interval;
        switch (gasTier)
        {
            case 1: interval = 22000; break;
            case 2: interval = 16000; break;
            case 3: interval = 12000; break;
            default: interval = 9000; break;
        }

        float chance = 0.35 + (0.45 * leak);
        if (chance > 0.90) chance = 0.90;

        player.m_TG_NextCoughMS = now + interval;

        if (Math.RandomFloatInclusive(0.0, 1.0) <= chance)
        {
            SymptomManager sm = player.GetSymptomManager();
            if (sm)
                sm.QueueUpPrimarySymptom(SymptomIDs.SYMPTOM_COUGH);
        }
    }

    static bool TryAddBleedCut(PlayerBase player, float chance01)
    {
        if (!GetGame().IsServer()) return false;
        if (chance01 <= 0) return false;

        BleedingSourcesManagerServer bms = player.GetBleedingManagerServer();
        if (!bms) return false;

        if (bms.GetBleedingSourcesCount() >= 5) return false;

        if (Math.RandomFloatInclusive(0.0, 1.0) > chance01) return false;

        string selections[] = {
            "LeftArm", "RightArm",
            "LeftLeg", "RightLeg",
            "Torso", "Head"
        };

        int selCount = 6;
        string sel = selections[Math.RandomInt(0, selCount)];

        bool ok = bms.AttemptAddBleedingSourceBySelection(sel);
        if (!ok)
            ok = bms.AttemptAddBleedingSourceBySelection("Torso");

        return ok;
    }

    static void TryInfectToxicWound(PlayerBase player, int gasTier, float leak)
    {
        if (!GetGame().IsServer()) return;
        if (leak <= 0.0) return;

        if (player.GetSingleAgentCount(eAgents.WOUND_AGENT) > 0) return;

        float baseChance;
        switch (gasTier)
        {
            case 1: baseChance = 0.20; break;
            case 2: baseChance = 0.30; break;
            case 3: baseChance = 0.60; break;
            default: baseChance = 0.80; break; 
        }

        float chance = baseChance * leak;
        if (chance > 0.75) chance = 0.75;

        if (Math.RandomFloatInclusive(0.0, 1.0) > chance) return;

        float agent = 800 + (gasTier * 250);
        player.InsertAgent(eAgents.WOUND_AGENT, agent);
    }

    static void SuppressNervePermanent(PlayerBase player, int durationMS)
    {
        int now = GetGame().GetTime();
        player.m_TG_NerveSuppressedUntilMS = now + durationMS;

        TieredGasEffects.UpdateVanillaSickAgentStage(player, TieredGasEffects.GetPersistentSickStage(player));
    }

    static bool IsNerveSuppressed(PlayerBase player)
    {
        return GetGame().GetTime() < player.m_TG_NerveSuppressedUntilMS;
    }

    static void AddNerveExposure(PlayerBase player, float exposure)
    {
        if (exposure <= 0) return;
        player.m_TG_NerveExposure += exposure;

        float thresh = TieredGasJSON.GetNerveExposureThreshold();
        if (!player.m_TG_NervePermanent && player.m_TG_NerveExposure >= thresh)
        {
            player.m_TG_NervePermanent = true;
        }
    }

    static bool HasPermanentNerveDamage(PlayerBase player)
    {
        return player.m_TG_NervePermanent;
    }

    static void SetBioInfected(PlayerBase player)
    {
        player.m_TG_BioInfected = true;
    }

    static void ClearBioInfection(PlayerBase player)
    {
        player.m_TG_BioInfected = false;
        player.m_TG_BioExposure = 0.0;

        TieredGasEffects.UpdateVanillaSickAgentStage(player, TieredGasEffects.GetPersistentSickStage(player));
    }

    static int GetPersistentSickStage(PlayerBase player)
    {
        int stage = 0;
        if (player.m_TG_NervePermanent && !TieredGasEffects.IsNerveSuppressed(player)) stage++;
        if (player.m_TG_BioInfected) stage++;
        if (stage > 3) stage = 3;
        return stage;
    }

    static bool IsBioInfected(PlayerBase player)
    {
        return player.m_TG_BioInfected;
    }

    static void AddBioExposure(PlayerBase player, float exposure)
    {
        if (exposure <= 0) return;
        player.m_TG_BioExposure += exposure;

        if (!player.m_TG_BioInfected && player.m_TG_BioExposure >= 15.0)
        {
            player.m_TG_BioInfected = true;
        }
    }

    static void UpdateVanillaSickAgentStage(PlayerBase player, int stage)
    {
        if (!GetGame().IsServer()) return;

        const int AGENT = eAgents.INFLUENZA;

        int desired;
        switch (stage)
        {
            case 1: desired = 350; break;
            case 2: desired = 650; break;
            case 3: desired = 950; break;
            default: desired = 0; break;
        }

        int cur = player.GetSingleAgentCount(AGENT);

        if (desired <= 0)
        {
            if (cur > 0) player.RemoveAgent(AGENT);
            return;
        }

        if (cur != desired)
        {
            if (cur > 0) player.RemoveAgent(AGENT);
            player.InsertAgent(AGENT, desired);
        }
    }

    static void TryPermanentRespSymptoms(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        if (!player.IsAlive()) return;

        int now = GetGame().GetTime();

        if (now >= player.m_TG_NextCoughMS)
        {
            player.m_TG_NextCoughMS = now + Math.RandomInt(20000, 40000);
            if (Math.RandomFloatInclusive(0.0, 1.0) <= 0.45)
            {
                SymptomManager sm1 = player.GetSymptomManager();
                if (sm1) sm1.QueueUpPrimarySymptom(SymptomIDs.SYMPTOM_COUGH);
            }
        }

        if (now >= player.m_TG_NextSneezeMS)
        {
            player.m_TG_NextSneezeMS = now + Math.RandomInt(25000, 55000);
            if (Math.RandomFloatInclusive(0.0, 1.0) <= 0.35)
            {
                SymptomManager sm2 = player.GetSymptomManager();
                if (sm2) sm2.QueueUpPrimarySymptom(SymptomIDs.SYMPTOM_SNEEZE);
            }
        }
    }

    static void ApplyPersistentEffects(PlayerBase player, float deltaTime)
    {
        if (!GetGame().IsServer()) return;
        if (!player.IsAlive()) return;

        int stage = TieredGasEffects.GetPersistentSickStage(player);
        TieredGasEffects.UpdateVanillaSickAgentStage(player, stage);

        if (stage > 0)
        {
            TieredGasEffects.TryPermanentRespSymptoms(player);
        }

        if (player.m_TG_NervePermanent && !TieredGasEffects.IsNerveSuppressed(player))
        {
            player.TG_ClampStaminaCap(0.5);
        }

        if (player.m_TG_BioInfected)
        {
            int now = GetGame().GetTime();
            if (now >= player.m_TG_BioNextSymptomMS)
            {
                player.m_TG_BioNextSymptomMS = now + 30000; 

                player.DecreaseHealth("", "Health", 0.2);
                player.AddHealth("", "Shock", -30.0);
                player.TG_DrainStamina(0.5);
            }
        }
    }

};