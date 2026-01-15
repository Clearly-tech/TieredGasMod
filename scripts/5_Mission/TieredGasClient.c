//---------------------------------------------------------------------------------------------------
// scripts/5_Mission/TieredGasClient.c
//
// File summary: Client mission hooks: initializes TieredGas client systems, creates HUD, handles admin menu references, and performs periodic updates.
//
// MissionGameplay (modded)
//
// void OnInit()
//      Client init: sets up particle manager and client systems.
//      Params: none
//
// void OnMissionStart()
//      Creates HUD + initializes admin UI integration.
//      Params: none
//
// void ClearAdminMenuRef()
//      Clears stored admin menu reference.
//      Params: none
//
// void OnUpdate(float timeslice)
//      Per-frame tick: updates HUD/admin menu state and handles delayed closes.
//      Params:
//          timeslice: frame delta time
//
// TieredGasAdminMenu GetAdminMenu()
//      Returns current admin menu instance (if any).
//      Params: none
//
// void OpenAdminMenu()
//      Opens admin menu if allowed/enabled.
//      Params: none
//
// void CloseAdminMenu()
//      Closes admin menu immediately.
//      Params: none
//
// void CloseAdminMenuDelayed(int delayMs)
//      Schedules menu close after delay.
//      Params:
//          delayMs: delay milliseconds
//
// void OnMissionFinish()
//      Cleanup (stop particles/HUD).
//      Params: none
//
// bool OnKeyPress(int key)
//      Hotkey handling for admin menu toggle (if implemented in your file).
//      Params:
//          key: key code
//
// bool IsAdminMenuEnabled()
//      Checks local setting + status gating for admin menu.
//      Params: none
//
// void UpdateHUD()
//      Updates HUD visibility/state based on player gas state.
//      Params: none
//
// void UpdateAdminMenu()
//      Updates admin menu (poll bridge messages, etc.).
//      Params: none
//
// void EnsureAdminStatus()
//      Requests admin check if unknown.
//      Params: none
//
// void HandleAdminMenuToggle()
//      Opens/closes menu based on current state.
//      Params: none
//
// void CleanupClientSystems()
//      Stops particles and clears refs (safe shutdown).
//      Params: none
//
// void HandleDelayedClose(float timeslice)
//      Executes delayed close countdown.
//      Params:
//          timeslice: frame delta time
//---------------------------------------------------------------------------------------------------

modded class MissionGameplay
{
    ref TieredGasHUD m_GasHUD;
    ref TieredGasAdminMenu m_AdminMenu;

    private bool m_AdminControlsLocked = false;
    private float m_DebugTimer = 0;
    private bool m_HUDInitialized = false;
    private bool m_ZonesRequested = false;
    private bool m_AdminStatusRequested = false;
    private bool m_AdminKnown = false;
    private bool m_IsAdminCached = false;
    private bool m_AdminMenuOpenPending = false;
    private int m_NextAdminCheckMs = 0;
    private const int ADMIN_CHECK_COOLDOWN_MS = 30000; 

    override void OnInit()
    {
        super.OnInit();

        if (GetGame().IsClient() || !GetGame().IsMultiplayer())
        {
            TieredGasParticleManager.Init();
        }
    }

    override void OnMissionStart()
    {
        super.OnMissionStart();

        if (GetGame().IsClient() || !GetGame().IsMultiplayer())
        {
            if (!m_GasHUD)
            {
                m_GasHUD = new TieredGasHUD();
                m_HUDInitialized = (m_GasHUD != null);
            }
        }
    }

    void ClearAdminMenuRef()
    {
        m_AdminMenu = null;
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        if (!GetGame().IsClient() && GetGame().IsMultiplayer())
        {
            return;
        }

        HandleAdminHotkeys();
        UpdateAdminControlLock();

        if (!m_ZonesRequested)
        {
            PlayerBase p0 = PlayerBase.Cast(GetGame().GetPlayer());
            if (p0 && p0.GetIdentity())
            {
                GetGame().RPCSingleParam(p0, RPC_TIERED_GAS_ZONES_REQUEST, null, true, p0.GetIdentity());
                m_ZonesRequested = true;
            }
        }

        if (!m_AdminStatusRequested && !m_AdminKnown)
        {
            PlayerBase pAdmin = PlayerBase.Cast(GetGame().GetPlayer());
            if (pAdmin && pAdmin.GetIdentity())
            {
                RequestAdminCheck(false);
                m_AdminStatusRequested = true;
            }
        }

        string aMsg;
        bool aErr;
        while (TieredGasClientBridge.PopAdminMessage(aMsg, aErr))
        {
            if (m_AdminMenu)
            {
                m_AdminMenu.ShowMessage(aMsg, aErr);
            }
        }

        bool isAdmin;
        if (TieredGasClientBridge.ConsumeAdminStatus(isAdmin))
        {
            m_AdminKnown = true;
            m_IsAdminCached = isAdmin;
            if (m_AdminMenuOpenPending)
            {
                m_AdminMenuOpenPending = false;

                if (isAdmin)
                {
                    OpenAdminMenu();
                }
                else
                {
                    if (m_AdminMenu && m_AdminMenu.IsOpen())
                    {
                        CloseAdminMenu();
                    }
                }
            }

            if (m_AdminMenu)
            {
                m_AdminMenu.SetAdminStatus(isAdmin);
            }
        }

        if (!m_HUDInitialized && !m_GasHUD)
        {
            m_DebugTimer += timeslice;
            if (m_DebugTimer > 2.0)
            {
                m_GasHUD = new TieredGasHUD();
                if (m_GasHUD) m_HUDInitialized = true;
                m_DebugTimer = 0;
            }
            return;
        }

        if (!m_GasHUD)
        {
            return;
        }

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
        {
            return;
        }

        bool inGas = player.IsInGasZone();

        if (inGas)
        {
            string gasType = player.GetCurrentGasType();
            int gasTier = player.GetCurrentGasTier();

            if (gasType == "" || gasType == "0") { gasType = "TOXIC"; }
            if (gasTier < 1 || gasTier > 4) { gasTier = 1; }

            m_GasHUD.Show(gasType, gasTier);
        }
        else
        {
            m_GasHUD.Hide();
        }
    }

    void UpdateAdminControlLock()
    {
        if (!GetGame() || (!GetGame().IsClient() && GetGame().IsMultiplayer()))
            return;

        bool menuOpen = (m_AdminMenu && m_AdminMenu.IsOpen());
        bool rmbHeld = false;
        Input inp = GetGame().GetInput();
        if (inp)
        {

            rmbHeld = inp.LocalHold("UAAim");
        }

        if (!rmbHeld)
        {
            UAInput aim = GetUApi().GetInputByName("UAAim");
            if (aim)
                rmbHeld = aim.LocalHold();
        }

        if (menuOpen && !rmbHeld)
        {
            if (!m_AdminControlsLocked)
            {
                m_AdminControlsLocked = true;
                GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);
                GetGame().GetUIManager().ShowCursor(true);
                Print("[TieredGasMod][AdminLock] Controls LOCKED (menu open)");
            }
            return;
        }
        if (m_AdminControlsLocked)
        {
            m_AdminControlsLocked = false;
            GetGame().GetMission().PlayerControlEnable(true);

            if (menuOpen)
                GetGame().GetUIManager().ShowCursor(true);

        }
    }

    override void OnKeyPress(int key)
    {
        super.OnKeyPress(key);
    }

    private int NowMs()
    {
        return GetGame().GetTime();
    }

    private void RequestAdminCheck(bool wantOpenMenu)
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p || !p.GetIdentity()) return;

        int now = NowMs();

        if (wantOpenMenu)
        {
            m_AdminMenuOpenPending = true;
        }

        if (now < m_NextAdminCheckMs)
        {
            return;
        }

        m_NextAdminCheckMs = now + ADMIN_CHECK_COOLDOWN_MS;

        GetGame().RPCSingleParam(p, RPC_ADMIN_CHECK, null, true, p.GetIdentity());
    }

    private bool HasAdminCached()
    {
        return (m_AdminKnown && m_IsAdminCached);
    }

    private bool EnsureAdminCached(bool wantOpenMenu)
    {
        if (HasAdminCached())
        {
            return true;
        }

        if (m_AdminKnown && !m_IsAdminCached)
        {
            return false;
        }

        RequestAdminCheck(wantOpenMenu);
        return false;
    }

    void HandleAdminHotkeys()
    {
        bool enabled = true;

        if (!enabled)
        {
            if (m_AdminMenu && m_AdminMenu.IsOpen())
            {
                CloseAdminMenu();
            }
            return;
        }

        Input inp = GetGame().GetInput();
        if (!inp) return;

        if (inp.LocalPress("UATG_ToggleAdminMenu"))
        {
            Print("[TieredGasMod][Input] Toggle Admin Menu");
            ToggleAdminMenu();
            return;
        }

        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p || !p.GetIdentity()) return;

        if (inp.LocalPress("UATG_ReloadGasZones"))
        {
            if (EnsureAdminCached(false))
            {
                Print("[TieredGasMod][Input] Reload Zones");
                GetGame().RPCSingleParam(p, RPC_ADMIN_RELOAD_ZONES, null, true, p.GetIdentity());
            }
            return;
        }

        if (inp.LocalPress("UATG_ReloadGasConfig"))
        {
            if (EnsureAdminCached(false))
            {
                Print("[TieredGasMod][Input] Reload Config");
                GetGame().RPCSingleParam(p, RPC_ADMIN_RELOAD_CONFIG, null, true, p.GetIdentity());
            }
            return;
        }

        if (inp.LocalPress("UATG_ReloadAdmins"))
        {
            if (EnsureAdminCached(false))
            {
                Print("[TieredGasMod][Input] Reload Admins");
                GetGame().RPCSingleParam(p, RPC_ADMIN_RELOAD_ADMINS, null, true, p.GetIdentity());
            }
            return;
        }
    }

    void ToggleAdminMenu()
    {
        if (m_AdminMenu && m_AdminMenu.IsOpen())
        {
            CloseAdminMenu();
            return;
        }

        if (EnsureAdminCached(true))
        {
            OpenAdminMenu();
        }
    }

    void OpenAdminMenu()
    {
        if (m_AdminMenu && m_AdminMenu.IsOpen())
        {
            return;
        }

        Print("[TieredGasMod] OpenAdminMenu (overlay)");

        if (!m_AdminMenu)
        {
            m_AdminMenu = new TieredGasAdminMenu();
        }

        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.OpenAdminMenuDelayed, 1, false);
    }

    void OpenAdminMenuDelayed()
    {
        if (!m_AdminMenu) return;
        m_AdminMenu.Open();
    }

    void CloseAdminMenu()
    {
        Print("[TieredGasMod] CloseAdminMenu (overlay)");
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.CloseAdminMenuDelayed, 1, false);
    }

    void CloseAdminMenuDelayed()
    {
        if (m_AdminMenu)
        {
            m_AdminMenu.Close();
            m_AdminMenu = null;
        }
    }

    override void OnMissionFinish()
    {
        if (m_AdminControlsLocked)
        {
            m_AdminControlsLocked = false;
            GetGame().GetMission().PlayerControlEnable(true);
            GetGame().GetUIManager().ShowCursor(false);
        }

        if (GetGame().IsClient() || !GetGame().IsMultiplayer())
        {
            TieredGasParticleManager.Cleanup();
        }

        if (m_GasHUD)
        {
            delete m_GasHUD;
            m_GasHUD = null;
        }

        if (m_AdminMenu)
        {
            CloseAdminMenu();
        }

        m_HUDInitialized = false;
        super.OnMissionFinish();
    }
}
