//------------------------------------------------------------------------------------------------
//! Map line
class MapCircle
{
    float m_fStartPointX, m_fStartPointY;
    float m_fEndPointX, m_fEndPointY;
    
    Widget m_wRootW;
    Widget m_wCircle;
    ImageWidget m_wCircleImage;
    
    SCR_MapEntity m_MapEntity;
    GRAD_MapMarkerUI m_OwnerComponent;
    
    string m_textureCache;
    string m_sType;
    bool isSpawnMarker;
    float m_opacity;
    
    RplId rplId;
    
    //------------------------------------------------------------------------------------------------
    void MapCircle(SCR_MapEntity mapEnt, GRAD_MapMarkerUI ownerComp)
    {
        m_MapEntity = mapEnt;
        m_OwnerComponent = ownerComp;
    }

    //------------------------------------------------------------------------------------------------
    void CreateCircle(notnull Widget rootW)
    {
        m_wRootW = rootW;
        
        if (!m_MapEntity) m_MapEntity = SCR_MapEntity.GetMapInstance();
        
        // Find the map frame to attach to
        Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
        // Fallback to the root drawing container if mapFrame isn't found, though usually mapFrame is preferred for scaling
        if (!mapFrame) mapFrame = rootW; 
        
        m_wCircle = GetGame().GetWorkspace().CreateWidgets("{4B995CEAA55BBECC}UI/Layouts/Map/MapDrawCircle.layout", mapFrame);
        m_wCircleImage = ImageWidget.Cast(m_wCircle.FindAnyWidget("DrawCircleImage"));
        
        // Initial setup based on marker type
        if (isSpawnMarker) {
            SetType("{7B0B54A321361079}UI/Textures/Map/circle_spawn.edds");
            m_opacity = 1.0;
        } else {
            SetType("{F4DD93249A5EC259}UI/Textures/Map/circle_range.edds");
            m_opacity = 0.5;
        }
        
        UpdateCircle();
    }
    
    //------------------------------------------------------------------------------------------------
    void RemoveCircle()
    {
        if (m_wCircle)
        {
            m_wCircle.RemoveFromHierarchy(); // Properly destroys the widget
            m_wCircle = null;
        }
    }
    
    //------------------------------------------------------------------------------------------------
    void SetType(string type)
    {
        // PERFORMANCE FIX: Only load texture if it actually changed
        if (m_sType == type && m_textureCache == type)
            return;

        m_sType = type;
        
        if (m_wCircleImage && m_sType != "") 
        {
            bool textureLoaded = m_wCircleImage.LoadImageTexture(0, m_sType, false, false);
            if (textureLoaded) {
                m_textureCache = m_sType;
            }
        }
    }
    
    //------------------------------------------------------------------------------------------------
    void SetVisibility(bool visible)
    {
        if (m_wCircle)
            m_wCircle.SetVisible(visible);
    }
    
    //------------------------------------------------------------------------------------------------
    void UpdateCircle()
    {   
        if (!m_wCircle || !m_MapEntity)
            return;
            
        // Apply Opacity
        m_wCircle.SetOpacity(m_opacity);
        
        // --- Calculate Vector ---
        vector circleVector = vector.Zero;
        circleVector[0] = m_fStartPointX - m_fEndPointX;
        circleVector[1] = m_fStartPointY - m_fEndPointY;

        vector angles = circleVector.VectorToAngles();
        if (angles[0] == 90)
            angles[1] =  180 - angles[1];
        
        if (m_wCircleImage)
            m_wCircleImage.SetRotation(angles[1]);
        
        // --- Calculate Size ---
        vector screenCircleVector = m_MapEntity.GetMapWidget().SizeToPixels(circleVector);
        float size = GetGame().GetWorkspace().DPIUnscale(screenCircleVector.Length());
        
        // Resize Root Widget (FrameSlot)
        FrameSlot.SetSize(m_wCircle, size, size);
        
        // Resize Image Widget
        if (m_wCircleImage)
            m_wCircleImage.SetSize(size, size);

        // --- Calculate Center in World Space ---
        float worldCenterX = (m_fStartPointX + m_fEndPointX) * 0.5;
        float worldCenterY = (m_fStartPointY + m_fEndPointY) * 0.5;
        
        float screenCenterX, screenCenterY;
        m_MapEntity.WorldToScreen(worldCenterX, worldCenterY, screenCenterX, screenCenterY, true);

        float posX = GetGame().GetWorkspace().DPIUnscale(screenCenterX);
        float posY = GetGame().GetWorkspace().DPIUnscale(screenCenterY);

        // --- Alignment ---
        FrameSlot.SetAlignment(m_wCircle, 0.5, 0); // move it half a circle down due to placement bug
        FrameSlot.SetPos(m_wCircle, posX, posY);
    }
};

//------------------------------------------------------------------------------------------------
class GRAD_MapMarkerUI
{
    protected Widget m_wDrawingContainer;
    
    protected ref array<ref MapCircle> m_aTransmissionCircles = new array <ref MapCircle>();
    protected ref array<ref MapCircle> m_aSpawnCircles = new array <ref MapCircle>();
    
    protected SCR_MapEntity m_MapEntity;
    
    //------------------------------------------------------------------------------------------------
    // Helper to update all circles at once
    protected void UpdateAllCircles()
    {
        foreach (MapCircle circle: m_aTransmissionCircles)
        {
            if (circle) circle.UpdateCircle();
        }
        foreach (MapCircle circle: m_aSpawnCircles)
        {
             if (circle) circle.UpdateCircle();
        }
    }

    // Helper to delayed update (useful for ZoomEnd)
    protected void UpdateAllCirclesDelayed()
    {
        foreach (MapCircle circle: m_aTransmissionCircles)
        {
            if (circle) GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false);
        }
        foreach (MapCircle circle: m_aSpawnCircles)
        {
            if (circle) GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false);
        }
    }

    //------------------------------------------------------------------------------------------------
    //! SCR_MapEntity event
    protected void OnMapPan(float x, float y, bool adjustedPan)
    {
        UpdateAllCircles();
    }
    
    //------------------------------------------------------------------------------------------------
    //! SCR_MapEntity event
    protected void OnMapPanEnd(float x, float y)
    {
        UpdateAllCirclesDelayed();
    }
    
    //------------------------------------------------------------------------------------------------
    protected void OnMapZoom(float zoomVal)
    {
        UpdateAllCircles();
    }
    
    protected void OnMapZoomEnd(float zoomVal)
    {
        UpdateAllCirclesDelayed();
    }
    
    //------------------------------------------------------------------------------------------------
    void OnMapOpen(MapConfiguration config)
    {
        m_MapEntity        = SCR_MapEntity.GetMapInstance();
        m_wDrawingContainer = FrameWidget.Cast(config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.DRAWING_CONTAINER_WIDGET_NAME));
        
        if (!m_wDrawingContainer) {
            Print(string.Format("GRAD_MapMarkerUI: Cant find m_wDrawingContainer"), LogLevel.ERROR);
            return;
        }
        
        // Re-create visuals for existing data circles
        foreach (MapCircle circle: m_aTransmissionCircles)
        {
            circle.m_MapEntity = m_MapEntity;
            circle.CreateCircle(m_wDrawingContainer);
            GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false);
        }
        
        foreach (MapCircle circle: m_aSpawnCircles)
        {
            circle.m_MapEntity = m_MapEntity;
            circle.CreateCircle(m_wDrawingContainer);
            GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false);
        }
                        
        m_MapEntity.GetOnMapPan().Insert(OnMapPan);     
        m_MapEntity.GetOnMapPanEnd().Insert(OnMapPanEnd);
        m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
        m_MapEntity.GetOnMapZoomEnd().Insert(OnMapZoomEnd);
    }
    
    //------------------------------------------------------------------------------------------------
    void OnMapClose(MapConfiguration config)
    {
        if (m_MapEntity)
        {
            m_MapEntity.GetOnMapPan().Remove(OnMapPan);
            m_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
            m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
            m_MapEntity.GetOnMapZoomEnd().Remove(OnMapZoomEnd);
        }
    }
    
    //------------------------------------------------------------------------------------------------
    void Init()
    {
        SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
        SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
        
        SCR_MapEntity.GetOnSelection().Remove(SendPlayerCoords);
        SCR_MapEntity.GetOnSelection().Insert(SendPlayerCoords);
    }
    
    //------------------------------------------------------------------------------------------------
    void SendPlayerCoords(vector coords) {
        // [Existing logic kept same]
        SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
        if (!mapEntity) return;
        
        SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
        if (!playerController) return;
        
        if (!GRAD_PlayerComponent.GetInstance().IsChoosingSpawn()) return;
        
        SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
        if (!ch) return;
        
        string factionKey = ch.GetFactionKey();
        
        float x, y;
        mapEntity.ScreenToWorld(coords[0], coords[2], x, y);
        coords[0] = x;
        coords[2] = y;
    
        vector worldPos = {coords[0], GetGame().GetWorld().GetSurfaceY(coords[0], coords[2]), coords[2]};
        int spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(worldPos, worldPos, 2, 2);

        if (!spawnEmpty)
        {
            SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ACTION_FAILED);
            SCR_NotificationsComponent.SendLocal(ENotification.FASTTRAVEL_PLAYER_LOCATION_WRONG);
            SCR_HintManagerComponent.GetInstance().ShowCustomHint("Position not suitable for spawn, pick another one.", "OOPS", 5, true);
            return;
        }

        SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_MAP_CLICK_POINT_ON);
        
        if (factionKey == "USSR")
            GRAD_PlayerComponent.GetInstance().SetOpforSpawn(worldPos);
    }
    
    // broadcast
    void CreateOrMoveSpawnMarker(string currentfaction, vector coords) 
    {
        RpcAsk_Authority_CreateOrMoveSpawnMarker(currentfaction, coords);
    }
    
    //------------------------------------------------------------------------------------------------
    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    void RpcAsk_Authority_CreateOrMoveSpawnMarker(string currentfaction, vector coords)
    {
        SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(SCR_PlayerController.GetLocalPlayerId()));
        if (!playerController) return;
        
        SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
        if (!ch) return;
        
        string factionKey = ch.GetFactionKey();
        
        if (factionKey != currentfaction) return;
        
        if (GRAD_BC_BreakingContactManager.IsDebugMode())
        	Print(string.Format("GRAD MapmarkerUI: creating spawn marker for faction %1", factionKey), LogLevel.NORMAL);
        
        // Create spawn marker circle: center at coords, radius 500m
        GRAD_PlayerComponent.GetInstance().AddCircleMarker(
            coords[0] - 500.0, 
            coords[2] - 500.0,
            coords[0] + 500.0,
            coords[2] + 500.0,
            -1,
            true
        );
    }
    
    //------------------------------------------------------------------------------------------------
    void RemoveSpawnMarker() 
    {
        // CRITICAL FIX: Do not remove from array while iterating it.
        // Clean up widgets first
        foreach (MapCircle singleCircle: m_aSpawnCircles)
        {
            if (singleCircle) singleCircle.RemoveCircle();
        }
        // Then clear the array
        m_aSpawnCircles.Clear();
        if (GRAD_BC_BreakingContactManager.IsDebugMode())
        	Print("GRAD CirclemarkerUI: Cleared spawn markers", LogLevel.NORMAL);
    }

    //------------------------------------------------------------------------------------------------
    void AddCircle(float startPointX, float startPointY, float endPointX, float endPointY, RplId rplId, bool isSpawnMarker)
    {
        MapCircle circle = new MapCircle(m_MapEntity, this);
        
        circle.m_fStartPointX = startPointX;
        circle.m_fStartPointY = startPointY;
        circle.m_fEndPointX = endPointX;
        circle.m_fEndPointY = endPointY;
        circle.rplId = rplId;
        circle.isSpawnMarker = isSpawnMarker;
        
        // Logic split by type
        if (isSpawnMarker) 
        {
            RemoveSpawnMarker(); // Clear old ones safely
            m_aSpawnCircles.Insert(circle);
        } 
        else 
        {
            m_aTransmissionCircles.Insert(circle);
        }
        
        // Visual creation (Shared Logic)
        if (m_wDrawingContainer) 
        {
            circle.CreateCircle(m_wDrawingContainer);
            // Delay update slightly to ensure layout system has processed the new widget
            GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false);
        } 
        else 
        {
            Print("GRAD_AddCircle: Cant find m_wDrawingContainer", LogLevel.ERROR);
        }
    }
    
    //------------------------------------------------------------------------------------------------
    void SetCircleInactive(RplId rplId)
    {
        foreach (MapCircle circle: m_aTransmissionCircles)
        {
            if (circle.rplId == rplId) {    
                circle.SetType("{F4DD93249A5EC259}UI/Textures/Map/circle_range.edds");
                circle.m_opacity = 0;
                circle.UpdateCircle();
            }
        }
    }
    
    //------------------------------------------------------------------------------------------------
    void SetCircleActive(RplId rplId)
    {   
        foreach (MapCircle circle: m_aTransmissionCircles)
        {
            if (circle.rplId == rplId) {
                circle.SetType("{BBDE49CD7C1A52F7}UI/Textures/Map/CircleMarker.edds");
                circle.m_opacity = 0.5;
                circle.UpdateCircle();
            }
        }
    }
    
    //------------------------------------------------------------------------------------------------
    void Cleanup()
    {
        // Remove static event subscriptions
        SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
        SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
        SCR_MapEntity.GetOnSelection().Remove(SendPlayerCoords);

        // Remove instance-level event subscriptions
        if (m_MapEntity)
        {
            m_MapEntity.GetOnMapPan().Remove(OnMapPan);
            m_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
            m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
            m_MapEntity.GetOnMapZoomEnd().Remove(OnMapZoomEnd);
        }

        // Clean up circle widgets
        foreach (MapCircle circle: m_aTransmissionCircles)
        {
            if (circle) circle.RemoveCircle();
        }
        m_aTransmissionCircles.Clear();

        foreach (MapCircle circle: m_aSpawnCircles)
        {
            if (circle) circle.RemoveCircle();
        }
        m_aSpawnCircles.Clear();
    }

    //------------------------------------------------------------------------------------------------
    void GRAD_MapMarkerUI()
    {
        m_MapEntity = SCR_MapEntity.GetMapInstance();
    }
};