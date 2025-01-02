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
	void CreateCircle(notnull Widget rootW)
	{
		m_wRootW = rootW;
		
		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			return;
		
		m_wCircle = GetGame().GetWorkspace().CreateWidgets("{4B995CEAA55BBECC}UI/Layouts/Map/MapDrawCircle.layout", mapFrame);
		m_wCircleImage = ImageWidget.Cast(m_wCircle.FindAnyWidget("DrawCircleImage"));
		
		if (isSpawnMarker) {
			m_wCircleImage.LoadImageTexture (0, "{7B0B54A321361079}UI/Textures/Map/circle_spawn.edds", false, false);
			UpdateCircle(); // set opacity to 1 for spawn marker
		};
	}
	
	//---
	void RemoveCircle(Widget rootW, MapWidget circle)
	{
		rootW.RemoveChild(circle);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetType(string type)
	{
		m_sType = type;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetVisibility(bool visible)
	{
		if (m_wCircleImage)
			m_wCircleImage.SetVisible(visible);
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdateCircle()
	{	
		if (!m_wCircle)	// can happen due to callater used for update
			return;
				
		int screenX, screenY, endX, endY;
		
		// gets logged REALLY often, only uncomment if necessary
		// Print(string.Format("GRAD CirclemarkerUI: m_sType is %1, m_textureCache is %2", m_sType, m_textureCache), LogLevel.NORMAL);
		
		if (m_sType != "" && m_textureCache != m_sType && !isSpawnMarker) {
			
			bool textureLoaded = m_wCircleImage.LoadImageTexture (0, m_sType, false, false);
			
			if (textureLoaded) {
				m_textureCache = m_sType;  // as we have no getter for existing texture WHYEVER :[[
			};
			
			Print(string.Format("GRAD CirclemarkerUI: LoadImageTexture success update"), LogLevel.NORMAL);
		};

		
		m_wCircle.SetOpacity(m_opacity);
		
		m_MapEntity.WorldToScreen(m_fStartPointX, m_fStartPointY, screenX, screenY, true);
		m_MapEntity.WorldToScreen(m_fEndPointX, m_fEndPointY, endX, endY, true);
		
		vector circleVector = vector.Zero;
		circleVector[0] = m_fStartPointX - m_fEndPointX;
		circleVector[1] = m_fStartPointY - m_fEndPointY;

		vector angles = circleVector.VectorToAngles();
		if (angles[0] == 90)
			angles[1] =  180 - angles[1]; 	// reverse angles when passing vertical axis
		
		m_wCircleImage.SetRotation(angles[1]);
		
		circleVector = m_MapEntity.GetMapWidget().SizeToPixels(circleVector);
		m_wCircleImage.SetSize(GetGame().GetWorkspace().DPIUnscale(circleVector.Length()), GetGame().GetWorkspace().DPIUnscale(circleVector.Length()));
		
		FrameSlot.SetPos(m_wCircle, GetGame().GetWorkspace().DPIUnscale(screenX), GetGame().GetWorkspace().DPIUnscale(screenY));	// needs unscaled coords
	}
	
	//------------------------------------------------------------------------------------------------
	void MapCircle(SCR_MapEntity mapEnt, GRAD_MapMarkerUI ownerComp)
	{
		m_MapEntity = mapEnt;
		m_OwnerComponent = ownerComp;
	}
};

//------------------------------------------------------------------------------------------------
class GRAD_MapMarkerUI
{
	protected Widget m_wDrawingContainer;
	
	protected ref array<ref MapCircle> m_aTransmissionCircles = new array <ref MapCircle>();
	protected ref array<ref MapCircle> m_aSpawnCircles = new array <ref MapCircle>();
	
	protected SCR_MapEntity m_MapEntity;
	
	protected vector m_vSpawnPos;
	
	//----
	vector GetSpawnCoords() 
	{
		return m_vSpawnPos;
	}
	
	//----
	void SetSpawnPos(vector spawnPos) 
	{
		m_vSpawnPos = spawnPos;
	}
	
	
	//------------------------------------------------------------------------------------------------
	//! SCR_MapEntity event
	protected void OnMapPan(float x, float y, bool adjustedPan)
	{
		foreach (MapCircle circle: m_aTransmissionCircles)
		{
			circle.UpdateCircle();
		}
		foreach (MapCircle circle: m_aSpawnCircles)
		{
			circle.UpdateCircle();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SCR_MapEntity event
	protected void OnMapPanEnd(float x, float y)
	{
		foreach (MapCircle circle: m_aTransmissionCircles)
		{
			GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false); // needs to be delayed by a frame as it cant always update the size after zoom correctly within the same frame
		}
		foreach (MapCircle circle: m_aSpawnCircles)
		{
			GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false); // needs to be delayed by a frame as it cant always update the size after zoom correctly within the same frame
		}
	}
	
	protected void OnMapZoom(float x, float y)
	{
		foreach (MapCircle circle: m_aTransmissionCircles)
		{
			circle.UpdateCircle();
		}
		foreach (MapCircle circle: m_aSpawnCircles)
		{
			circle.UpdateCircle();
		}
	}
	
	protected void OnMapZoomEnd(float x, float y)
	{
		foreach (MapCircle circle: m_aTransmissionCircles)
		{
			circle.UpdateCircle();
		}
		foreach (MapCircle circle: m_aSpawnCircles)
		{
			circle.UpdateCircle();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnMapOpen(MapConfiguration config)
	{
		m_wDrawingContainer = FrameWidget.Cast(config.RootWidgetRef.FindAnyWidget(SCR_MapConstants.DRAWING_CONTAINER_WIDGET_NAME));
		
		if (!m_wDrawingContainer) {
			Print(string.Format("GRAD_MapMarkerUI: Cant find m_wDrawingContainer"), LogLevel.ERROR);
			return;
		}
		
		foreach (MapCircle circle: m_aTransmissionCircles)
		{
			circle.CreateCircle(m_wDrawingContainer);
			GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false);
		}
		
		foreach (MapCircle circle: m_aSpawnCircles)
		{
			circle.CreateCircle(m_wDrawingContainer);
			GetGame().GetCallqueue().CallLater(circle.UpdateCircle, 1, false);
		}
						
		m_MapEntity.GetOnMapPan().Insert(OnMapPan);		// pan for scaling
		m_MapEntity.GetOnMapPanEnd().Insert(OnMapPanEnd);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnMapClose(MapConfiguration config)
	{
		m_MapEntity.GetOnMapPan().Remove(OnMapPan);
		m_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
	}
	
	//------------------------------------------------------------------------------------------------
	void Init()
	{
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		// SCR_MapEntity.GetOnSelectionChanged().Insert(OnMapSelectionChanged);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
		
		SCR_MapEntity.GetOnSelection().Remove(SendPlayerCoords);
		SCR_MapEntity.GetOnSelection().Insert(SendPlayerCoords);
	}
	
	void SendPlayerCoords(vector coords) {
		Print(string.Format("GRAD CirclemarkerUI: Map Selection Changed"), LogLevel.WARNING);
		
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity) {
			Print(string.Format("GRAD CirclemarkerUI: mapEntity is false"), LogLevel.WARNING);	
			return;
		}
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(SCR_PlayerController.GetLocalPlayerId()));
		if (!playerController) {
			Print(string.Format("GRAD CirclemarkerUI: playerController is false"), LogLevel.WARNING);	
			return;
		}
		
		if (!playerController.IsChoosingSpawn()) {
			Print(string.Format("GRAD CirclemarkerUI: IsChoosingSpawn is false"), LogLevel.WARNING);	
			return;
		}
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!ch)  {
			Print(string.Format("SCR_ChimeraCharacter missing in playerController"), LogLevel.NORMAL);
			return;
		}
		
		string factionKey = ch.GetFactionKey();
		
		float x, y;
		mapEntity.ScreenToWorld(coords[0], coords[2], x, y);
		coords[0] = x;
		coords[2] = y;
	
		vector worldPos = {coords[0], GetGame().GetWorld().GetSurfaceY(coords[0], coords[2]), coords[2]};
        bool spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(worldPos, worldPos, 2, 2);

		if (!spawnEmpty)
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ACTION_FAILED);
			SCR_NotificationsComponent.SendLocal(ENotification.FASTTRAVEL_PLAYER_LOCATION_WRONG);
			SCR_HintManagerComponent.GetInstance().ShowCustomHint("Position not suitable for spawn, pick another one.", "OOPS", 5, true);
			return;
		}

		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_MAP_CLICK_POINT_ON);	
		CreateOrMoveSpawnMarker(factionKey, coords); // sync marker/delete previous marker
		SetSpawnPos(worldPos); // for other fnc to grab
	}
	
	// broadcast
	void CreateOrMoveSpawnMarker(string currentfaction, vector coords) 
	{
		RpcAsk_Authority_CreateOrMoveSpawnMarker(currentfaction, coords);
	}
	
	// everyone evaluates and adds circle if faction is correct
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_Authority_CreateOrMoveSpawnMarker(string currentfaction, vector coords)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(SCR_PlayerController.GetLocalPlayerId()));
		if (!playerController) {
			Print(string.Format("GRAD MapmarkerUI: No playercontroller"), LogLevel.ERROR);
			return;
		}
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!ch)  {
			Print(string.Format("SCR_ChimeraCharacter missing in playerController"), LogLevel.NORMAL);
			return;
		}
		
		string factionKey = ch.GetFactionKey();
		
		
		// only create marker for same faction!
		bool createMarker = factionKey == currentfaction;
		
		
		Print(string.Format("GRAD MapmarkerUI: factionKey is " + factionKey + " | " + "createMarker is " + createMarker.ToString()), LogLevel.NORMAL);
		
		if (!createMarker) {
			Print(string.Format("Breaking Contact - skipping marker creation, wrong faction"), LogLevel.NORMAL);
			return;
		}
		
		// todo fix hardcoded
		playerController.AddCircleMarker(
			coords[0] - 500.0,
			coords[2] + 500.0,
			coords[0] + 500.0,
			coords[2] + 500.0,
			-1,
			true
		);
	}
	
	//------------------------------------------------------------------------------------------------
	void RemoveSpawnMarker() {
		foreach (MapCircle singleCircle: m_aSpawnCircles)
		{
			singleCircle.SetVisibility(false); // make previous circles invisible
			m_aSpawnCircles.Remove(m_aSpawnCircles.Find(singleCircle));
			Print(string.Format("GRAD CirclemarkerUI: making previous spawn circles invisible"), LogLevel.NORMAL);
		}
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
		circle.m_opacity = 0.5;
		
		if (isSpawnMarker) { circle.m_opacity = 1; };
		
		if (isSpawnMarker) {
			foreach (MapCircle singleCircle: m_aSpawnCircles)
			{
				singleCircle.SetVisibility(false); // make previous circles invisible
				m_aSpawnCircles.Remove(m_aSpawnCircles.Find(singleCircle));
				Print(string.Format("GRAD CirclemarkerUI: making previous spawn circles invisible"), LogLevel.WARNING);
			}
			m_aSpawnCircles.Insert(circle);
			// fix null pointer
			if (!m_wDrawingContainer) {
				Print(string.Format("GRAD_AddCircle: Cant find m_wDrawingContainer"), LogLevel.ERROR);
				return;
			}
			
			circle.CreateCircle(m_wDrawingContainer); // actually create too :P
			foreach (MapCircle singleCircle: m_aSpawnCircles)
			{
				GetGame().GetCallqueue().CallLater(singleCircle.UpdateCircle, 1, false); // needs to be delayed by a frame as it cant always update the size after zoom correctly within the same frame
			}
		} else {
			m_aTransmissionCircles.Insert(circle);
			// fix null pointer
			if (!m_wDrawingContainer) {
				Print(string.Format("GRAD_AddCircle: Cant find m_wDrawingContainer"), LogLevel.ERROR);
				return;
			}
			
			circle.CreateCircle(m_wDrawingContainer); // actually create too :P
			foreach (MapCircle singleCircle: m_aSpawnCircles)
			{
				GetGame().GetCallqueue().CallLater(singleCircle.UpdateCircle, 1, false); // needs to be delayed by a frame as it cant always update the size after zoom correctly within the same frame
			}
		};
	}
	
	
	//--
	void SetCircleInactive(RplId rplId)
	{
		foreach (MapCircle circle: m_aTransmissionCircles)
		{
			if (circle.rplId == rplId) {	
				circle.SetType("{F4DD93249A5EC259}UI/Textures/Map/circle_range.edds");
				Print(string.Format("GRAD CirclemarkerUI: SetCircleInactive"), LogLevel.WARNING);
				circle.m_opacity = 0;
				circle.UpdateCircle();
			};
		}
	}
	
	//--
	void SetCircleActive(RplId rplId)
	{	
		foreach (MapCircle circle: m_aTransmissionCircles)
		{
			if (circle.rplId == rplId) {
				circle.SetType("{BBDE49CD7C1A52F7}UI/Textures/Map/CircleMarker.edds");
				Print(string.Format("GRAD CirclemarkerUI: SetCircleActive"), LogLevel.WARNING);
				circle.m_opacity = 0.5;
				circle.UpdateCircle();
			};
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void GRAD_MapMarkerUI()
	{
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}
};