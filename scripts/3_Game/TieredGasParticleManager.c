//---------------------------------------------------------------------------------------------------
// scripts/3_Game/TieredGasParticleManager.c
//
// File summary: Client particle controller: spawns/updates/removes gas zone cloud particles and player-local effects; supports admin-menu preview particles.
//
// TieredGasParticleManager
//
// void Init()
//      Initializes internal maps/arrays used to track spawned particles.
//      Params: none
//
// int GetId(string key)
//      Converts a particle “key” into the registered particle ID (from ParticleList).
//      Params:
//          key: particle key/name used by TieredGas (ex: resolved cloud key)
//
// void UpdateZoneCloud(string uuid, array<vector> anchors, string key, float crossFadeSeconds)
//      Ensures a zone’s cloud particles exist and match the given anchor positions; crossfades when changing particle type.
//      Params:
//          uuid: zone identifier
//          anchors: particle spawn points (world positions)
//          key: particle definition key to use
//          crossFadeSeconds: fade time when switching particle sets
//
// void RemoveZoneCloud(string uuid, float fadeSeconds)
//      Removes/fades out all particles associated with a zone UUID.
//      Params:
//          uuid: zone identifier
//          fadeSeconds: fade-out duration
//
// void UpdatePlayerLocalFromZone(Object ownerZone, int ownerPriority, Object player, string key)
//      Applies/updates a local particle effect on a player caused by a zone (typically “inside gas” effect).
//      Params:
//          ownerZone: zone object that “owns” this local effect
//          ownerPriority: priority to decide which zone wins if multiple overlap
//          player: player object receiving the effect
//          key: local particle key to use
//
// void ClearPlayerLocalIfOwner(Object zone)
//      Clears local player effects if they are currently owned by the given zone.
//      Params:
//          zone: zone object to match against current owner
//
// void SpawnPreview(string key, vector pos)
//      Admin-menu helper: spawns preview particle(s) at a position.
//      Params:
//          key: particle key
//          pos: world position
//
// void StopPreview(bool instant = false)
//      Stops preview particles (immediate or delayed).
//      Params:
//          instant: if true, stop immediately; otherwise fade/stop safely
//
// void StopParticles(array<Particle> ps)
//      Stops a set of particles immediately.
//      Params:
//          ps: particle array
//
// void StopParticleLater(Particle p, int delayMs)
//      Stops one particle after a delay (used for soft cleanup).
//      Params:
//          p: particle
//          delayMs: delay in milliseconds
//
// void StopParticlesLater(array<Particle> ps, int delayMs)
//      Stops a set of particles after a delay.
//      Params:
//          ps: particle array
//          delayMs: delay in milliseconds
//
// void Cleanup()
//      Clears internal tracking + stops remaining particles (shutdown/mission finish safety).
//      Params: none
//---------------------------------------------------------------------------------------------------

class TieredGasParticleManager
{
    
    static ref map<string, ref array<Particle>> m_ZoneCloudParticles;
    static ref map<string, string> m_ZoneCloudKey;

    
    static ref map<string, int> m_ParticleIdCache;

    
    static ref array<Particle> m_PreviewParticles;

    
    static Particle m_PlayerLocalParticle;
    static string m_PlayerLocalKey;
    static Object m_PlayerLocalOwnerZone;
    static int m_PlayerLocalOwnerPriority;

    static void Init()
    {
        if (!m_ZoneCloudParticles)
        {
            m_ZoneCloudParticles = new map<string, ref array<Particle>>;
            m_ZoneCloudKey = new map<string, string>;
            m_ParticleIdCache = new map<string, int>;
            m_PreviewParticles = new array<Particle>;
            Print("[TieredGasMod] Particle Manager initialized");
        }
    }

    
    
    
    static int GetId(string key)
    {
        if (!m_ParticleIdCache) m_ParticleIdCache = new map<string, int>;

        if (m_ParticleIdCache.Contains(key))
        {
            return m_ParticleIdCache.Get(key);
        }

        int id = ParticleList.GetParticleIDByName(key);
        m_ParticleIdCache.Insert(key, id);
        return id;
    }

    
    
    
    static void UpdateZoneCloud(string uuid, array<vector> anchors, string key, float crossFadeSeconds)
    {
        if (uuid == "") return;
        if (!anchors || anchors.Count() == 0) return;

        if (!m_ZoneCloudParticles) Init();

        string oldKey = "";
        if (m_ZoneCloudKey && m_ZoneCloudKey.Contains(uuid))
        {
            oldKey = m_ZoneCloudKey.Get(uuid);
        }

        bool keyChanged = (oldKey != key);

        ref array<Particle> cur = null;
        if (m_ZoneCloudParticles.Contains(uuid))
        {
            cur = m_ZoneCloudParticles.Get(uuid);
        }

        bool needRebuild = false;

        if (!cur) needRebuild = true;
        else if (keyChanged) needRebuild = true;
        else if (cur.Count() != anchors.Count()) needRebuild = true;

        if (!needRebuild)
        {
            return;
        }

        int id = GetId(key);
        if (id <= 0)
        {
            Print("[TieredGasMod] WARNING: Cloud particle key not registered: " + key);
            return;
        }

        ref array<Particle> next = new array<Particle>;

        for (int a = 0; a < anchors.Count(); a++)
        {
            Particle p = Particle.Play(id, anchors[a]);
            next.Insert(p);
        }

        m_ZoneCloudParticles.Set(uuid, next);
        if (!m_ZoneCloudKey) m_ZoneCloudKey = new map<string, string>;
        m_ZoneCloudKey.Set(uuid, key);

        if (cur && cur.Count() > 0)
        {
            int ms = Math.Floor(crossFadeSeconds * 1000.0);
            StopParticlesLater(cur, ms);
        }
    }

    
    
    
    static void RemoveZoneCloud(string uuid, float fadeSeconds)
    {
        if (uuid == "") return;
        
        if (m_ZoneCloudParticles && m_ZoneCloudParticles.Contains(uuid))
        {
            array<Particle> particles = m_ZoneCloudParticles.Get(uuid);
            if (particles)
            {
                
                if (fadeSeconds <= 0.0)
                {
                    
                    StopParticles(particles);
                }
                else
                {
                    
                    int ms = Math.Floor(fadeSeconds * 1000.0);
                    StopParticlesLater(particles, ms);
                }
            }
            m_ZoneCloudParticles.Remove(uuid);
            
            if (m_ZoneCloudKey)
                m_ZoneCloudKey.Remove(uuid);
        }
    }

    
    
    
    static void UpdatePlayerLocalFromZone(Object ownerZone, int ownerPriority, Object player, string key)
    {
        if (!ownerZone || !player) return;
        if (!m_ZoneCloudParticles) Init();

        
        bool takeOwnership = false;

        if (!m_PlayerLocalOwnerZone)
        {
            takeOwnership = true;
        }
        else if (ownerZone == m_PlayerLocalOwnerZone)
        {
            takeOwnership = true;
        }
        else
        {
            if (ownerPriority > m_PlayerLocalOwnerPriority)
            {
                takeOwnership = true;
            }
        }

        if (!takeOwnership) return;

        m_PlayerLocalOwnerZone = ownerZone;
        m_PlayerLocalOwnerPriority = ownerPriority;

        if (m_PlayerLocalParticle && m_PlayerLocalKey == key)
        {
            return; 
        }

        
        if (m_PlayerLocalParticle)
        {
            
            StopParticleLater(m_PlayerLocalParticle, 1200);
            m_PlayerLocalParticle = null;
        }

        int id = GetId(key);
        if (id <= 0)
        {
            Print("[TieredGasMod] WARNING: Local particle key not registered: " + key);
            m_PlayerLocalKey = "";
            return;
        }

        m_PlayerLocalParticle = Particle.PlayOnObject(id, player);
        m_PlayerLocalKey = key;
    }

    static void ClearPlayerLocalIfOwner(Object zone)
    {
        if (!zone) return;

        if (zone != m_PlayerLocalOwnerZone) return;

        if (m_PlayerLocalParticle)
        {
            
            StopParticleLater(m_PlayerLocalParticle, 1500);
            m_PlayerLocalParticle = null;
        }

        m_PlayerLocalKey = "";
        m_PlayerLocalOwnerZone = null;
        m_PlayerLocalOwnerPriority = 0;
    }

    
    
    
    static void SpawnPreview(string key, vector pos)
    {
        StopPreview(true); 

        if (!m_PreviewParticles) m_PreviewParticles = new array<Particle>;

        int id = GetId(key);
        if (id <= 0)
        {
            Print("[TieredGasMod] WARNING: Preview particle key not registered: " + key);
            return;
        }

        Particle p = Particle.Play(id, pos);
        if (p)
        {
            m_PreviewParticles.Insert(p);
        }
    }

    static void StopPreview(bool instant = false)
    {
        if (m_PreviewParticles && m_PreviewParticles.Count() > 0)
        {
            if (instant)
            {
                
                StopParticles(m_PreviewParticles);
            }
            else
            {
                
                int ms = Math.Floor(2.0 * 1000.0);
                StopParticlesLater(m_PreviewParticles, ms);
            }
            m_PreviewParticles.Clear();
        }
    }

    
    
    
    static void StopParticles(array<Particle> ps)
    {
        if (!ps) return;
        for (int i = 0; i < ps.Count(); i++)
        {
            if (ps[i]) ps[i].Stop();
        }
    }

    static void StopParticleLater(Particle p, int delayMs)
    {
        if (!p) return;
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(p.Stop, delayMs, false);
    }

    static void StopParticlesLater(array<Particle> ps, int delayMs)
    {
        if (!ps) return;

        
        ref array<Particle> copy = new array<Particle>;
        for (int i = 0; i < ps.Count(); i++)
        {
            copy.Insert(ps[i]);
        }

        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(StopParticles, delayMs, false, copy);
    }
    
    static void Cleanup()
    {
        
        StopPreview(true);

        
        if (m_PlayerLocalParticle)
        {
            StopParticleLater(m_PlayerLocalParticle, 500);
            m_PlayerLocalParticle = null;
        }

        m_PlayerLocalKey = "";
        m_PlayerLocalOwnerZone = null;
        m_PlayerLocalOwnerPriority = 0;

        
        if (m_ZoneCloudParticles)
        {
            foreach (string uuid, ref array<Particle> ps : m_ZoneCloudParticles)
            {
                StopParticles(ps);
            }
            m_ZoneCloudParticles.Clear();
        }

        if (m_ZoneCloudKey)
        {
            m_ZoneCloudKey.Clear();
        }

        if (m_ParticleIdCache)
        {
            m_ParticleIdCache.Clear();
        }

        Print("[TieredGasMod] ParticleManager Cleanup complete");
    }
};