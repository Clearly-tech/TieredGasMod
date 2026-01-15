//---------------------------------------------------------------------------------------------------
// scripts/5_Mission/TieredGasHud.c
//
// File summary: HUD widget/controller for showing gas status.
//
// TieredGasHUD
//
// void TieredGasHUD()
//      Constructor.
//      Params: none
//
// void CreateWidgets()
//      Creates/loads the HUD layout widgets.
//      Params: none
//
// void Show()
//      Shows HUD.
//      Params: none
//
// void Hide()
//      Hides HUD.
//      Params: none
//---------------------------------------------------------------------------------------------------
class TieredGasHUD
{
    private bool m_IsShowing = false;
    private string m_LastGasType = "";
    private int m_LastTier = 0;

    private ref map<string, string> m_IconPaths;

    private Widget m_RootWidget;
    private ImageWidget m_IconWidget;
    private bool m_WidgetsCreated = false;

    void TieredGasHUD()
    {
        Print("==============================================");
        Print("[TieredGasMod] Gas HUD Constructor Called");
        Print("==============================================");
        m_IconPaths = new map<string, string>;
        m_IconPaths.Insert("TOXIC_1", "TieredGasMod/mod_icons/toxic_t1.paa");
        m_IconPaths.Insert("TOXIC_2", "TieredGasMod/mod_icons/toxic_t2.paa");
        m_IconPaths.Insert("TOXIC_3", "TieredGasMod/mod_icons/toxic_t3.paa");
        m_IconPaths.Insert("TOXIC_4", "TieredGasMod/mod_icons/toxic_t4.paa");

        m_IconPaths.Insert("NERVE_1", "TieredGasMod/mod_icons/nerve_t1.paa");
        m_IconPaths.Insert("NERVE_2", "TieredGasMod/mod_icons/nerve_t2.paa");
        m_IconPaths.Insert("NERVE_3", "TieredGasMod/mod_icons/nerve_t3.paa");
        m_IconPaths.Insert("NERVE_4", "TieredGasMod/mod_icons/nerve_t4.paa");

        m_IconPaths.Insert("BIO_1", "TieredGasMod/mod_icons/bio_t1.paa");
        m_IconPaths.Insert("BIO_2", "TieredGasMod/mod_icons/bio_t2.paa");
        m_IconPaths.Insert("BIO_3", "TieredGasMod/mod_icons/bio_t3.paa");
        m_IconPaths.Insert("BIO_4", "TieredGasMod/mod_icons/bio_t4.paa");

        Print("[TieredGasMod] Icon paths registered: " + m_IconPaths.Count() + " entries");
        Print("[TieredGasMod] HUD Constructor complete - widgets will be created on demand");
    }

    bool CreateWidgets()
    {
        if (m_WidgetsCreated)
        {
            Print("[TieredGasMod] Widgets already created, skipping");
            return true;
        }

        Print("[TieredGasMod] CreateWidgets() called");
        
        if (!GetGame())
        {
            Print("[TieredGasMod] ERROR: GetGame() returned null!");
            return false;
        }
        
        WorkspaceWidget workspace = GetGame().GetWorkspace();
        if (!workspace)
        {
            Print("[TieredGasMod] ERROR: GetWorkspace() returned null!");
            return false;
        }
        
        if (!GetGame().GetMission())
        {
            Print("[TieredGasMod] ERROR: GetMission() returned null!");
            return false;
        }

        Print("[TieredGasMod] Attempting to load HUD layout from: TieredGasMod/GUI/layouts/TieredGas/HUD.layout");
        
        m_RootWidget = workspace.CreateWidgets("TieredGasMod/GUI/layouts/TieredGas/HUD.layout");
        
        if (!m_RootWidget)
        {
            Print("[TieredGasMod] ERROR: Failed to create root widget! Check if layout file exists.");
            return false;
        }
        
        Print("[TieredGasMod] SUCCESS: Root widget created");
        Print("[TieredGasMod] Root widget name: " + m_RootWidget.GetName());
        Print("[TieredGasMod] Root widget visible: " + m_RootWidget.IsVisible());

        m_RootWidget.Show(true);
        
        m_IconWidget = ImageWidget.Cast(m_RootWidget.FindAnyWidget("GasIcon"));
        
        if (!m_IconWidget)
        {
            Print("[TieredGasMod] ERROR: Failed to find 'GasIcon' widget in layout!");
            Print("[TieredGasMod] Trying to list all child widgets...");

            Widget child = m_RootWidget.GetChildren();
            if (child)
            {
                Print("[TieredGasMod] Found child widget: " + child.GetName());
            }
            else
            {
                Print("[TieredGasMod] No child widgets found!");
            }

            if (m_RootWidget)
            {
                m_RootWidget.Unlink();
                m_RootWidget = null;
            }
            
            return false;
        }
        
        Print("[TieredGasMod] SUCCESS: Icon widget found");
        Print("[TieredGasMod] Icon widget name: " + m_IconWidget.GetName());
        Print("[TieredGasMod] Icon widget visible: " + m_IconWidget.IsVisible());

        float x, y;
        m_IconWidget.GetScreenPos(x, y);
        Print("[TieredGasMod] Icon widget screen position: X=" + x + " Y=" + y);
        
        float w, h;
        m_IconWidget.GetScreenSize(w, h);
        Print("[TieredGasMod] Icon widget screen size: W=" + w + " H=" + h);

        m_IconWidget.Show(false);
        
        m_WidgetsCreated = true;
        Print("[TieredGasMod] Widget creation complete - HUD is ready");
        
        return true;
    }

    void Show(string gasType, int tier)
    {
        if (!m_WidgetsCreated)
        {
            Print("[TieredGasMod] Widgets not created yet, attempting to create...");
            if (!CreateWidgets())
            {
                Print("[TieredGasMod] Failed to create widgets, cannot show HUD");
                return;
            }
        }
        
        if (!m_IconWidget)
        {
            Print("[TieredGasMod] ERROR: Cannot show HUD - Icon widget is null!");
            return;
        }

        if (m_IsShowing && m_LastGasType == gasType && m_LastTier == tier)
        {
            return;
        }

        Print("[TieredGasMod] Show() called - Type: " + gasType + " Tier: " + tier);

        string key = gasType + "_" + tier;
        Print("[TieredGasMod] Looking for icon with key: '" + key + "'");
        
        string iconPath = m_IconPaths.Get(key);

        if (!iconPath || iconPath == "")
        {
            Print("[TieredGasMod] ERROR: No icon path found for key: " + key);
            return;
        }

        Print("[TieredGasMod] Loading icon from path: " + iconPath);

        m_IconWidget.LoadImageFile(0, iconPath);
        float w, h;
        m_IconWidget.GetSize(w, h);
        m_IconWidget.Show(true);
        m_RootWidget.Show(true);
 
        m_IconWidget.SetColor(ARGB(255, 255, 255, 255));
        
        Print("[TieredGasMod] Image loaded and displayed");

        m_LastGasType = gasType;
        m_LastTier = tier;
        m_IsShowing = true;
        
        Print("[TieredGasMod] SUCCESS: HUD icon displayed!");
    }

    void Hide()
    {
        if (!m_IconWidget)
        {
            return;
        }
        
        if (!m_IsShowing)
        {
            return; 
        }
        
        Print("[TieredGasMod] Hide() called");
        
        m_IconWidget.Show(false);

        m_LastGasType = "";
        m_LastTier = 0;
        m_IsShowing = false;
        
        Print("[TieredGasMod] HUD hidden");
    }

    void ~TieredGasHUD()
    {
        Print("[TieredGasMod] Gas HUD Destructor called");
        
        if (m_RootWidget)
        {
            m_RootWidget.Unlink();
            Print("[TieredGasMod] Root widget unlinked");
        }
        
        Print("[TieredGasMod] Gas HUD destroyed");
    }
};