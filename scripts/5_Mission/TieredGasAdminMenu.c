//---------------------------------------------------------------------------------------------------
// scripts/5_Mission/TieredGasAdminMenu.c
//
// File summary: Admin UI: open/close menu, build widgets, read inputs, send RPC commands, show responses, and preview zones.
//
// TieredGasAdminMenu
//
// void TieredGasAdminMenu()
//      Constructor; sets up references/defaults.
//      Params: none
//
// void Open()
//      Opens and builds the menu.
//      Params: none
//
// void Close()
//      Closes/hides the menu and stops previews if needed.
//      Params: none
//
// void OnHide()
//      Called when menu is hidden (cleanup).
//      Params: none
//
// bool IsOpen()
//      Returns whether menu is currently open.
//      Params: none
//
// void SetStatusText(string text, bool isError = false)
//      Updates status output text in the UI.
//      Params:
//          text: message
//          isError: error styling flag
//
// void BuildLayout()
//      Creates widgets (buttons, combos, edits) and binds handlers.
//      Params: none
//
// void AddComboInt(Widget parent, string label, out ComboBoxWidget combo, array<int> values, int defaultValue)
//      Helper to create an integer combo box.
//      Params: as named (parent/label/combo out/values/default)
//
// void AddComboGasType(Widget parent, string label, out ComboBoxWidget combo, int defaultType)
//      Helper to create gas type combo box.
//      Params: as named
//
// int ReadTier()
//      Reads tier selection from UI.
//      Params: none
//
// int ReadGasType()
//      Reads gas type selection from UI.
//      Params: none
//
// float ReadRadius()
//      Reads radius input from UI.
//      Params: none
//
// vector ReadPosition()
//      Reads position input (or uses player position depending on your UI).
//      Params: none
//
// string ReadDensity()
//      Reads density selection.
//      Params: none
//
// string ReadColorId()
//      Reads color selection.
//      Params: none
//
// bool ReadMaskRequired()
//      Reads mask-required selection.
//      Params: none
//
// void SendSpawnZoneRPC()
//      Sends RPC to spawn a zone using current UI inputs.
//      Params: none
//
// void SendRemoveZoneRPC()
//      Sends RPC to remove a zone (nearest/uuid depending on your UI state).
//      Params: none
//
// void SendReloadConfigRPC()
//      Sends RPC to reload config on server.
//      Params: none
//
// void SendReloadAdminsRPC()
//      Sends RPC to reload admin list on server.
//      Params: none
//
// void SendReloadZonesRPC()
//      Sends RPC to reload zones on server.
//      Params: none
//
// void StartPreview()
//      Spawns preview particles for chosen zone appearance.
//      Params: none
//
// void StopPreview()
//      Stops preview particles.
//      Params: none
//
// bool OnClick(Widget w, int x, int y, int button)
//      Handles button clicks.
//      Params: standard UI click args
//
// void Update()
//      Per-frame menu update (poll messages/status).
//      Params: none
//
// void PollAdminMessages()
//      Consumes queued admin messages from TieredGasClientBridge and displays them.
//      Params: none
//
// void PollAdminStatus()
//      Consumes admin status and enables/disables admin UI accordingly.
//      Params: none
//
// void RequestAdminStatusIfNeeded()
//      Triggers admin check RPC if status not known.
//      Params: none
//---------------------------------------------------------------------------------------------------
class TieredGasAdminMenu : ScriptedWidgetEventHandler
{

    static const string LAYOUT_PATH = "TieredGasMod/GUI/layouts/TieredGas/AdminMenu.layout";

    static const string PARTICLE_BASE = "TieredGasCloud";

    static const int TAB_ZONES     = 0;
    static const int TAB_SPAWNER   = 1;
    static const int TAB_CONFIG    = 2;
    static const int TAB_PARTICLES = 3;

    protected Widget m_Root;
    protected Widget m_Background;
    protected TextWidget m_Title;
    protected TextWidget m_StatusText;

    protected ButtonWidget m_BtnNavZones;
    protected ButtonWidget m_BtnNavSpawner;
    protected ButtonWidget m_BtnNavConfig;
    protected ButtonWidget m_BtnNavParticles;
    protected ButtonWidget m_BtnClose;

    protected Widget m_ZonesPanel;
    protected Widget m_SpawnerPanel;
    protected Widget m_ConfigPanel;
    protected Widget m_ParticlePanel;

    protected TextWidget m_ZonesHeader;
    protected TextListboxWidget m_ListZones;
    protected ButtonWidget m_BtnListZones;
    protected ButtonWidget m_BtnSpawnZone;  
    protected ButtonWidget m_BtnRemoveZone;

    protected ref array<string> m_ZoneUuids;

    protected TextWidget m_SpawnerHeader;
    protected EditBoxWidget m_EditSpawnPos;      
    protected EditBoxWidget m_EditSpawnRadius;
    protected EditBoxWidget m_EditSpawnHeight;
    protected EditBoxWidget m_EditSpawnBottomOffset;
    protected XComboBoxWidget m_ComboSpawnTier;
    protected XComboBoxWidget m_ComboSpawnGasType;
    protected XComboBoxWidget m_ComboSpawnColor;
    protected XComboBoxWidget m_ComboSpawnDensity;
    protected CheckBoxWidget m_CheckSpawnMask;
    protected CheckBoxWidget m_CheckSpawnLow;    
    protected ButtonWidget m_BtnSpawnerPreview;
    protected ButtonWidget m_BtnStopPreview;     
    protected ButtonWidget m_BtnSpawnerSpawn;

    protected ref array<int>    m_TierItems;
    protected ref array<int>    m_GasTypeItems;
    protected ref array<string> m_GasTypeLabels;
    protected ref array<string> m_ColorItems;
    protected ref array<string> m_DensityItems;

    protected TextWidget m_ConfigHeader;
    protected EditBoxWidget m_EditAnchorSpacing;
    protected EditBoxWidget m_EditAnchorMax;
    protected EditBoxWidget m_EditParticleLifetime;
    protected ButtonWidget m_BtnSaveConfig;
    protected ButtonWidget m_BtnReloadConfig;

    protected TextWidget m_ParticlesHeader;
    protected TextListboxWidget m_ListParticles;
    protected ButtonWidget m_BtnReloadParticles;

    protected bool m_IsAdmin = false;
    protected static bool m_IsOpen = false;
    protected int  m_ActiveTab = TAB_ZONES;

    void TieredGasAdminMenu()
    {
        m_ZoneUuids = new array<string>;
        m_TierItems = new array<int>;
        m_GasTypeItems = new array<int>;
        m_GasTypeLabels = new array<string>;
        m_ColorItems = new array<string>;
        m_DensityItems = new array<string>;
    }

    void Open()
    {
        if (m_Root)
        {
            return;
        }

        Print("[TieredGasAdminMenu] Open()");
        m_IsOpen = true;

        m_Root = GetGame().GetWorkspace().CreateWidgets(LAYOUT_PATH);
        if (!m_Root)
        {
            Print("[TieredGasAdminMenu] ERROR: Failed to create widgets from layout: " + LAYOUT_PATH);
            return;
        }

        m_Root.SetHandler(this);
        CacheWidgets();
        GetGame().GetUIManager().ShowUICursor(true);
        SetTab(TAB_ZONES);
        PlayerBase pb = PlayerBase.Cast(GetGame().GetPlayer());
        if (pb && pb.GetIdentity())
        {
            GetGame().RPCSingleParam(pb, RPC_ADMIN_CHECK, null, true, pb.GetIdentity());
        }
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.RefreshParticlesList, 500, false);
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.SafeRefreshZones, 500, false);
    }

    void Close()
    {
        if (!m_Root)
        {
            return;
        }

        Print("[TieredGasAdminMenu] Close()");
        m_IsOpen = false;
        TieredGasParticleManager.StopPreview();

        GetGame().GetUIManager().ShowUICursor(false);

        m_Root.Unlink();
        m_Root = null;
    }

    void OnHide()
    {
        TieredGasParticleManager.StopPreview();
    }

    static bool IsOpen()
    {
        return m_IsOpen;
    }

    void SetAdminStatus(bool isAdmin)
    {
        m_IsAdmin = isAdmin;

        if (!m_IsAdmin)
        {
            SetStatus("ACCESS DENIED - Not an admin", true);
        }
        else
        {
            SetStatus("Admin access granted", false);
        }
    }

    protected void CacheWidgets()
    {

        m_Background = m_Root.FindAnyWidget("Background");
        m_Title      = TextWidget.Cast(m_Root.FindAnyWidget("Title"));
        m_StatusText = TextWidget.Cast(m_Root.FindAnyWidget("StatusText"));

        m_BtnNavZones     = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnNavZones"));
        m_BtnNavSpawner   = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnNavSpawner"));
        m_BtnNavConfig    = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnNavConfig"));
        m_BtnNavParticles = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnNavParticles"));
        m_BtnClose        = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnClose"));

        m_ZonesPanel    = m_Root.FindAnyWidget("ZonesPanel");
        m_SpawnerPanel  = m_Root.FindAnyWidget("SpawnerPanel");
        m_ConfigPanel   = m_Root.FindAnyWidget("ConfigPanel");
        m_ParticlePanel = m_Root.FindAnyWidget("ParticlePanel");

        m_ZonesHeader   = TextWidget.Cast(m_Root.FindAnyWidget("ZonesHeader"));
        m_ListZones     = TextListboxWidget.Cast(m_Root.FindAnyWidget("ListZones"));
        m_BtnListZones  = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnListZones"));
        m_BtnSpawnZone  = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnSpawnZone"));
        m_BtnRemoveZone = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnRemoveZone"));

        m_SpawnerHeader     = TextWidget.Cast(m_Root.FindAnyWidget("SpawnerHeader"));
        m_EditSpawnPos      = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditSpawnPos")); 
        m_EditSpawnRadius        = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditSpawnRadius"));
        m_EditSpawnHeight        = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditSpawnHeight"));
        m_EditSpawnBottomOffset  = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditSpawnBottomOffset"));
        m_ComboSpawnTier    = XComboBoxWidget.Cast(m_Root.FindAnyWidget("ComboSpawnTier"));
        m_ComboSpawnGasType = XComboBoxWidget.Cast(m_Root.FindAnyWidget("ComboSpawnGastype"));
        m_ComboSpawnColor   = XComboBoxWidget.Cast(m_Root.FindAnyWidget("ComboSpawnColor"));
        m_ComboSpawnDensity = XComboBoxWidget.Cast(m_Root.FindAnyWidget("ComboSpawnDensity"));
        m_CheckSpawnMask    = CheckBoxWidget.Cast(m_Root.FindAnyWidget("CheckSpawnMask"));
        m_CheckSpawnLow     = CheckBoxWidget.Cast(m_Root.FindAnyWidget("CheckSpawnLow")); 
        m_BtnSpawnerPreview = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnSpawnerPreview"));
        m_BtnStopPreview    = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnStopPreview")); 
        
        if (m_EditSpawnHeight && m_EditSpawnHeight.GetText() == "") m_EditSpawnHeight.SetText("20");
        if (m_EditSpawnBottomOffset && m_EditSpawnBottomOffset.GetText() == "") m_EditSpawnBottomOffset.SetText("10");
        if (m_EditSpawnRadius && m_EditSpawnRadius.GetText() == "") m_EditSpawnRadius.SetText("50");

        m_BtnSpawnerSpawn   = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnSpawnerSpawn"));

        m_ConfigHeader         = TextWidget.Cast(m_Root.FindAnyWidget("ConfigHeader"));
        m_EditAnchorSpacing    = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditAnchorSpacing"));
        m_EditAnchorMax        = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditAnchorMax"));
        m_EditParticleLifetime = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditParticleLifetime"));
        m_BtnSaveConfig        = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnSaveConfig"));
        m_BtnReloadConfig      = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnReloadConfig"));

        m_ParticlesHeader    = TextWidget.Cast(m_Root.FindAnyWidget("ParticlesHeader"));
        m_ListParticles      = TextListboxWidget.Cast(m_Root.FindAnyWidget("ListParticles"));
        m_BtnReloadParticles = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnReloadParticles"));

        InitSpawnerCombos();
    }

    void ShowMessage(string msg, bool err)
    {
        SetStatus(msg, err);
    }

    protected void SetStatus(string msg, bool isError)
    {
        if (m_StatusText)
        {
            m_StatusText.SetText(msg);

            if (isError)
            {
                m_StatusText.SetColor(ARGB(255, 255, 80, 80));
            }
            else
            {
                m_StatusText.SetColor(ARGB(255, 80, 255, 80));
            }
        }
    }

    protected void SetTab(int tab)
    {
        m_ActiveTab = tab;

        if (m_ZonesPanel)    m_ZonesPanel.Show(tab == TAB_ZONES);
        if (m_SpawnerPanel)  m_SpawnerPanel.Show(tab == TAB_SPAWNER);
        if (m_ConfigPanel)   m_ConfigPanel.Show(tab == TAB_CONFIG);
        if (m_ParticlePanel) m_ParticlePanel.Show(tab == TAB_PARTICLES);
    }

    void UpdateZonesList(array<ref GasZoneConfig> zones)
    {
        Print("[TieredGasAdminMenu] UpdateZonesList START");

        if (!m_IsOpen || !m_Root || !m_ListZones)
        {
            Print("[TieredGasAdminMenu] Menu or widgets not ready");
            return;
        }

        if (!m_ZoneUuids)
            m_ZoneUuids = new array<string>;

        m_ListZones.ClearItems();
        m_ZoneUuids.Clear();

        if (!zones)
        {
            Print("[TieredGasAdminMenu] Zones array NULL");
            return;
        }
        for (int i = 0; i < zones.Count(); i++)
        {
            GasZoneConfig z = zones[i];
            if (!z)
            {
                Print("[TieredGasAdminMenu] Zone[" + i + "] NULL");
                continue;
            }

            string line = string.Format("%1 | %2 | Tier %3 | R %4",z.uuid,z.name,z.tier,z.radius);

            int row = m_ListZones.AddItem(line, null, 0);
            if (row >= 0)
                m_ZoneUuids.Insert(z.uuid);
        }
        SetStatus("Done!", false);
        Print("[TieredGasAdminMenu] UpdateZonesList END");
    }

    void RefreshZonesFromClientCache()
    {
        if (!m_IsOpen)
        {
            return;
        }

        if (!m_Root || !m_ListZones)
        {
            return;
        }

        if (!TieredGasZoneSpawner.m_ClientConfigsByUUID)
        {
            SetStatus("No client zones cache yet", true);
            return;
        }

        array<ref GasZoneConfig> zones = new array<ref GasZoneConfig>;

        foreach (string uuid, ref GasZoneConfig cfg : TieredGasZoneSpawner.m_ClientConfigsByUUID)
        {
            if (cfg)
            {
                zones.Insert(cfg);
            }
        }

        SetStatus("Loading...", false);
        UpdateZonesList(zones);
    }

    void SafeRefreshZones()
    {
        if (m_IsOpen && m_Root)
        {
            RefreshZonesFromClientCache();
        }
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (!m_Root)
        {
            return false;
        }

        if (w == m_BtnClose)
        {
            Close();
            return true;
        }

        if (w == m_BtnNavZones)
        {
            SetTab(TAB_ZONES);
            return true;
        }

        if (w == m_BtnNavSpawner)
        {
            SetTab(TAB_SPAWNER);
            return true;
        }

        if (w == m_BtnNavConfig)
        {
            SetTab(TAB_CONFIG);
            return true;
        }

        if (w == m_BtnNavParticles)
        {
            SetTab(TAB_PARTICLES);
            return true;
        }

        if (!m_IsAdmin)
        {
            SetStatus("Not an admin", true);
            return true;
        }

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player || !player.GetIdentity())
        {
            SetStatus("Player/identity not available", true);
            return true;
        }

        vector pos = "0 0 0";
        float radius = 0.0;
        string key = "";
        bool low = false;

        if (w == m_BtnListZones)
        {
            GetGame().RPCSingleParam(player, RPC_TIERED_GAS_ZONES_REQUEST, null, true, player.GetIdentity());
            SetStatus("Requesting zones sync...", false);

            GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.SafeRefreshZones, 500, false);
            return true;
        }

        if (w == m_BtnRemoveZone)
        {
            if (!m_ListZones || !m_ZoneUuids)
            {
                SetStatus("Zones list not ready", true);
                return true;
            }

            int rowSelx = m_ListZones.GetSelectedRow();
            if (rowSelx < 0 || rowSelx >= m_ZoneUuids.Count())
            {
                SetStatus("Select a zone to remove", true);
                return true;
            }

            string uuidSelrevx = m_ZoneUuids[rowSelx];
            if (uuidSelrevx == "")
            {
                SetStatus("Invalid zone UUID (empty)", true);
                return true;
            }

            if (GetGame().IsClient() || !GetGame().IsMultiplayer())
            {
                TieredGasParticleManager.RemoveZoneCloud(uuidSelrevx, 0.0); 

                if (TieredGasZoneSpawner.m_ClientZonesByUUID && TieredGasZoneSpawner.m_ClientZonesByUUID.Contains(uuidSelrevx))
                {
                    TieredGasZone z = TieredGasZoneSpawner.m_ClientZonesByUUID.Get(uuidSelrevx);
                    if (z)
                        GetGame().ObjectDelete(z);
                    TieredGasZoneSpawner.m_ClientZonesByUUID.Remove(uuidSelrevx);
                }

                if (TieredGasZoneSpawner.m_ClientConfigsByUUID)
                    TieredGasZoneSpawner.m_ClientConfigsByUUID.Remove(uuidSelrevx);
            }

            Param1<string> pUuid = new Param1<string>(uuidSelrevx);
            GetGame().RPCSingleParam(player, RPC_ADMIN_REMOVE_ZONE_BY_UUID, pUuid, true, player.GetIdentity());
            SetStatus("Remove requested: " + uuidSelrevx, false);
            return true;
        }

        
        if (m_BtnStopPreview && w == m_BtnStopPreview)
        {
            TieredGasParticleManager.StopPreview(true); 
            SetStatus("Preview stopped.", false);
            return true;
        }

        if (w == m_BtnSpawnerPreview)
        {
            pos = GetPreviewPosition();
            low = (m_CheckSpawnLow && m_CheckSpawnLow.IsChecked());
            key = BuildParticleKey(low);

            TieredGasParticleManager.SpawnPreview(key, pos);
            SetStatus("Preview: " + key, false);
            return true;
        }

        if (w == m_BtnSpawnerSpawn || w == m_BtnSpawnZone)
        {
            pos = GetPreviewPosition();
            radius = ReadEditFloat(m_EditSpawnRadius, 50.0);

            int tier = ReadTier(1);
            int gasType = ReadGasType(1);
            string colorId = ReadColor("black");
            string density = NormalizeDensityLocal(ReadDensity("Normal"));

            bool maskReq = (m_CheckSpawnMask && m_CheckSpawnMask.IsChecked());

            string zoneName = "Admin Spawned Zone";

            if (w == m_BtnSpawnZone)
            {
                int rowSel = -1;
                if (m_ListZones)
                {
                    rowSel = m_ListZones.GetSelectedRow();
                }

                if (rowSel < 0)
                {
                    SetStatus("Select a zone first", true);
                    return true;
                }

                string uuidSel;
                m_ListZones.GetItemText(rowSel, 0, uuidSel);
                string nameSel;
                m_ListZones.GetItemText(rowSel, 1, nameSel);

                if (nameSel != "")
                {
                    zoneName = "SpawnHere: " + nameSel;
                }
            }
            bool cycle = false;
            float cycleSeconds = 0.0;

            float height = ReadEditFloat(m_EditSpawnHeight, 6.0);
            float bottomOffset = ReadEditFloat(m_EditSpawnBottomOffset, 0.0);

            ref TieredGasSpawnPayload payload = new TieredGasSpawnPayload();
                payload.tier = tier;
                payload.gasType = gasType;
                payload.radius = radius;
                payload.zoneName = zoneName;
                payload.colorId = colorId;
                payload.density = density;
                payload.cycle = cycle;
                payload.cycleSeconds = cycleSeconds;
                payload.height = height;
                payload.bottomOffset = bottomOffset;
                payload.maskRequired = maskReq;
                payload.verticalMargin = 1.0;

            GetGame().RPCSingleParam(player, RPC_ADMIN_SPAWN_ZONE, payload, true, player.GetIdentity());

            SetStatus("Spawn requested: tier " + tier.ToString() + " type=" + gasType.ToString() + " r=" + radius.ToString(), false);
            return true;
        }

        if (w == m_BtnReloadConfig)
        {
            GetGame().RPCSingleParam(player, RPC_ADMIN_RELOAD_CONFIG, null, true, player.GetIdentity());
            SetStatus("Reload config requested.", false);
            return true;
        }

        if (w == m_BtnSaveConfig)
        {
            SetStatus("SaveConfig: no server RPC implemented", true);
            return true;
        }

        if (w == m_BtnReloadParticles)
        {
            RefreshParticlesList();
            SetStatus("Particles list refreshed.", false);
            return true;
        }

        return false;
    }

    protected void InitSpawnerCombos()
    {

        if (!m_TierItems)
        {
            m_TierItems = new array<int>;
        }

        if (!m_GasTypeItems)
        {
            m_GasTypeItems = new array<int>;
        }

        if (!m_GasTypeLabels)
        {
            m_GasTypeLabels = new array<string>;
        }

        if (!m_ColorItems)
        {
            m_ColorItems = new array<string>;
        }

        if (!m_DensityItems)
        {
            m_DensityItems = new array<string>;
        }

        m_TierItems.Clear();
        if (m_ComboSpawnTier)
        {
            m_ComboSpawnTier.ClearAll();
            AddComboInt(m_ComboSpawnTier, m_TierItems, 1);
            AddComboInt(m_ComboSpawnTier, m_TierItems, 2);
            AddComboInt(m_ComboSpawnTier, m_TierItems, 3);
            AddComboInt(m_ComboSpawnTier, m_TierItems, 4);
            m_ComboSpawnTier.SetCurrentItem(0);
        }
        m_GasTypeItems.Clear();
        m_GasTypeLabels.Clear();
        if (m_ComboSpawnGasType)
        {
            m_ComboSpawnGasType.ClearAll();
            AddComboGasType(m_ComboSpawnGasType, m_GasTypeItems, m_GasTypeLabels, 0, "Toxic");
            AddComboGasType(m_ComboSpawnGasType, m_GasTypeItems, m_GasTypeLabels, 1, "Nerve");
            AddComboGasType(m_ComboSpawnGasType, m_GasTypeItems, m_GasTypeLabels, 2, "Bio");
            m_ComboSpawnGasType.SetCurrentItem(0);
        }

        m_ColorItems.Clear();
        if (m_ComboSpawnColor)
        {
            m_ComboSpawnColor.ClearAll();
            AddComboString(m_ComboSpawnColor, m_ColorItems, "default");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "red");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "orange");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "green");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "black");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "purple");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "yellow");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "white");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "blue");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "cyan");
            AddComboString(m_ComboSpawnColor, m_ColorItems, "pink");
            m_ComboSpawnColor.SetCurrentItem(0);
        }

        m_DensityItems.Clear();
        if (m_ComboSpawnDensity)
        {
            m_ComboSpawnDensity.ClearAll();
            AddComboString(m_ComboSpawnDensity, m_DensityItems, "Normal");
            AddComboString(m_ComboSpawnDensity, m_DensityItems, "Dense");
            AddComboString(m_ComboSpawnDensity, m_DensityItems, "Light");
            m_ComboSpawnDensity.SetCurrentItem(0);
        }
    }

    protected void AddComboString(XComboBoxWidget cmb, array<string> store, string value)
    {
        if (store)
        {
            store.Insert(value);
        }

        if (cmb)
        {
            cmb.AddItem(value);
        }
    }

    protected void AddComboInt(XComboBoxWidget cmb, array<int> store, int value)
    {
        if (store)
        {
            store.Insert(value);
        }

        if (cmb)
        {
            cmb.AddItem(value.ToString());
        }
    }

    protected void AddComboGasType(XComboBoxWidget cmb, array<int> storeValues, array<string> storeLabels, int value, string label)
    {
        if (storeValues)
        {
            storeValues.Insert(value);
        }

        if (storeLabels)
        {
            storeLabels.Insert(label);
        }

        if (cmb)
        {
            cmb.AddItem(label);
        }
    }

    protected int ReadTier(int fallback)
    {
        if (!m_ComboSpawnTier || !m_TierItems)
        {
            return fallback;
        }

        int idx = m_ComboSpawnTier.GetCurrentItem();
        if (idx < 0 || idx >= m_TierItems.Count())
        {
            return fallback;
        }

        return m_TierItems[idx];
    }

    protected int ReadGasType(int fallback)
    {
        if (!m_ComboSpawnGasType || !m_GasTypeItems)
        {
            return fallback;
        }

        int idx = m_ComboSpawnGasType.GetCurrentItem();
        if (idx < 0 || idx >= m_GasTypeItems.Count())
        {
            return fallback;
        }

        return m_GasTypeItems[idx];
    }

    protected string ReadColor(string fallback)
    {
        if (!m_ComboSpawnColor || !m_ColorItems)
        {
            return fallback;
        }

        int idx = m_ComboSpawnColor.GetCurrentItem();
        if (idx < 0 || idx >= m_ColorItems.Count())
        {
            return fallback;
        }

        return m_ColorItems[idx];
    }

    protected string ReadDensity(string fallback)
    {
        if (!m_ComboSpawnDensity || !m_DensityItems)
        {
            return fallback;
        }

        int idx = m_ComboSpawnDensity.GetCurrentItem();
        if (idx < 0 || idx >= m_DensityItems.Count())
        {
            return fallback;
        }

        return m_DensityItems[idx];
    }

    protected string NormalizeDensityLocal(string density)
    {
        density.Trim();

        if (density == "Dense" || density == "dense" || density == "High" || density == "high")
        {
            return "Dense";
        }

        if (density == "Light" || density == "light" || density == "Low" || density == "low")
        {
            return "Light";
        }

        return "Normal";
    }

    protected string BuildParticleKey(bool low)
    {
        string color = ReadColor("black");
        string dens  = NormalizeDensityLocal(ReadDensity("Normal"));

        string key = PARTICLE_BASE + "_" + color + "_" + dens;
        if (low)
        {
            key = key + "_low";
        }

        return key;
    }

    protected vector GetPreviewPosition()
    {
        PlayerBase pb = PlayerBase.Cast(GetGame().GetPlayer());
        if (!pb)
        {
            return "0 0 0";
        }

        if (!m_EditSpawnPos)
        {
            return pb.GetPosition();
        }

        string posText = m_EditSpawnPos.GetText();
        if (posText == "")
        {
            return pb.GetPosition();
        }

        vector pos = posText.ToVector();

        if (pos[0] == 0 && pos[1] == 0 && pos[2] == 0 && posText != "0 0 0")
        {
            return pb.GetPosition();
        }

        return pos;
    }

    protected float ReadEditFloat(EditBoxWidget eb, float fallback)
    {
        if (!eb)
        {
            return fallback;
        }

        string t = eb.GetText();
        if (t == "")
        {
            return fallback;
        }

        return t.ToFloat();
    }

    protected void RefreshParticlesList()
    {
        if (!m_ListParticles)
        {
            return;
        }

        m_ListParticles.ClearItems();

        if (TieredGasParticleManager.m_ParticleIdCache)
        {
            foreach (string k, int id : TieredGasParticleManager.m_ParticleIdCache)
            {
                int row = m_ListParticles.AddItem(k, null, 0);
            }
        }
        else
        {
            m_ListParticles.AddItem("No cache", null, 0);
        }
    }
};