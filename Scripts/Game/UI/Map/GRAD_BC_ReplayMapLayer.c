[BaseContainerProps()]
class GRAD_BC_ReplayMapLayer : GRAD_MapMarkerLayer // ✅ Inherit from proven working class
{
	// Replay display state
	protected ref array<ref GRAD_BC_ReplayPlayerMarker> m_playerMarkers = {};
	protected ref array<ref GRAD_BC_ReplayProjectileMarker> m_projectileMarkers = {};
	
	// Replay mode flag - true when actively viewing replay, false for normal map use
	protected bool m_bIsInReplayMode = false;
	
	// Keep last frame data for persistent display
	protected ref array<ref GRAD_BC_ReplayPlayerMarker> m_lastFramePlayerMarkers = {};
	protected ref array<ref GRAD_BC_ReplayProjectileMarker> m_lastFrameProjectileMarkers = {};
	protected bool m_hasLastFrame = false;
	
	// Unit type icon textures
	protected ref map<string, string> m_unitTypeTextures = new map<string, string>();
	
	// Cached loaded textures
	protected ref map<string, ref SharedItemRef> m_loadedTextures = new map<string, ref SharedItemRef>();
	
	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		super.Init();
		
		Print("GRAD_BC_ReplayMapLayer: Initializing replay map layer", LogLevel.NORMAL);
		
		// Initialize unit type textures with game icons
		m_unitTypeTextures.Set("AntiTank", "{A0D5026DB9156DA8}UI/Textures/Icons/iconman_at_ca.edds");
		m_unitTypeTextures.Set("AT", "{A0D5026DB9156DA8}UI/Textures/Icons/iconman_at_ca.edds");
		m_unitTypeTextures.Set("AAT", "{A0D5026DB9156DA8}UI/Textures/Icons/iconman_at_ca.edds");
		m_unitTypeTextures.Set("Engineer", "{DB2B78344402FFB6}UI/Textures/Icons/iconmanengineer_ca.edds");
		m_unitTypeTextures.Set("Grenadier", "{46E164B39CEAF539}UI/Textures/Icons/iconmanexplosive_ca.edds");
		m_unitTypeTextures.Set("Leader", "{27462C23B18E544B}UI/Textures/Icons/iconmanleader_ca.edds");
		m_unitTypeTextures.Set("SL", "{27462C23B18E544B}UI/Textures/Icons/iconmanleader_ca.edds");
		m_unitTypeTextures.Set("Officer", "{5C5BA790ABB84C51}UI/Textures/Icons/iconmanofficer_ca.edds");
		m_unitTypeTextures.Set("Medic", "{82BA31D8DF0720DB}UI/Textures/Icons/iconmanmedic_ca.edds");
		m_unitTypeTextures.Set("MachineGunner", "{FDB9BEC99F7C26A3}UI/Textures/Icons/iconmanmg_ca.edds");
		m_unitTypeTextures.Set("AR", "{FDB9BEC99F7C26A3}UI/Textures/Icons/iconmanmg_ca.edds");
		m_unitTypeTextures.Set("AutomaticRifleman", "{FDB9BEC99F7C26A3}UI/Textures/Icons/iconmanmg_ca.edds");
		m_unitTypeTextures.Set("Sharpshooter", "{8CE2F1C396335FAC}UI/Textures/Icons/iconmanrecon_ca.edds");
		m_unitTypeTextures.Set("Recon", "{8CE2F1C396335FAC}UI/Textures/Icons/iconmanrecon_ca.edds");
		m_unitTypeTextures.Set("Spotter", "{8CE2F1C396335FAC}UI/Textures/Icons/iconmanrecon_ca.edds");
		m_unitTypeTextures.Set("Rifleman", "{9731965B995D0B76}UI/Textures/Icons/iconman_ca.edds");
		
		// Vehicle icons
		m_unitTypeTextures.Set("ArmedCar", "{EB7C826CD3D2BE53}UI/Textures/Icons/iconarmedcar_ca.edds");
		m_unitTypeTextures.Set("Car", "{AAEC4011C3FAAE79}UI/Textures/Icons/iconcar_ca.edds");
		m_unitTypeTextures.Set("Helicopter", "{7F728098B42A124F}UI/Textures/Icons/iconheli_ca.edds");
		m_unitTypeTextures.Set("HelicopterArmed", "{0729FDD4F156F1DD}UI/Textures/Icons/iconheliarmed_ca.edds");
		m_unitTypeTextures.Set("Truck", "{DDACA1439DB45633}UI/Textures/Icons/icontruck_ca.edds");
		m_unitTypeTextures.Set("Tank", "{CAC18FF5CC2A427D}UI/Textures/Icons/icontank_ca.edds");
		m_unitTypeTextures.Set("Plane", "{D9861CB8FECEC812}UI/Textures/Icons/iconplane_ca.edds");
		
		// Default fallback for unknown unit types
		m_unitTypeTextures.Set("Default", "{9731965B995D0B76}UI/Textures/Icons/iconman_ca.edds");
		
		Print("GRAD_BC_ReplayMapLayer: Replay map layer ready with unit type textures", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config); // ✅ This handles widget/canvas creation in parent
		
		Print("GRAD_BC_ReplayMapLayer: OnMapOpen called, canvas should be initialized by parent", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		Print("GRAD_BC_ReplayMapLayer: OnMapClose called, cleaning up", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// ✅ Use the proven Draw() pattern from GRAD_MapMarkerManager
	override void Draw()
	{
		// Clear previous commands (inherited from GRAD_MapMarkerLayer)
		m_Commands.Clear();
		
		// Check if we should draw anything
		GRAD_BC_ReplayManager replayManager = GRAD_BC_ReplayManager.GetInstance();
		bool shouldDraw = false;
		array<ref GRAD_BC_ReplayPlayerMarker> markersToRender = {};
		
		if (replayManager && replayManager.IsPlayingBack())
		{
			// During playback - use current markers
			shouldDraw = m_playerMarkers.Count() > 0;
			markersToRender = m_playerMarkers;
			
			static int drawCallCounter = 0;
			drawCallCounter++;
			if (drawCallCounter % 50 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Draw() during playback - %1 markers", markersToRender.Count()));
			}
		}
		else if (m_hasLastFrame)
		{
			// After replay - use saved last frame
			shouldDraw = m_lastFramePlayerMarkers.Count() > 0;
			markersToRender = m_lastFramePlayerMarkers;
			
			static int persistentDrawCounter = 0;
			persistentDrawCounter++;
			if (persistentDrawCounter % 100 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Draw() persistent mode - %1 saved markers", markersToRender.Count()));
			}
		}
		
		if (!shouldDraw || !m_MapEntity)
		{
			return; // Nothing to draw or no map entity
		}
		
		// Draw player markers with unit type icons and directional indicators
		foreach (GRAD_BC_ReplayPlayerMarker marker : markersToRender)
		{
			if (!marker.isVisible)
				continue;
				
			// Get faction color from Faction API
			int color = 0xFFFFFFFF; // Default white
			
			// Get the faction using FactionManager
			FactionManager factionManager = GetGame().GetFactionManager();
			if (factionManager)
			{
				Faction faction = factionManager.GetFactionByKey(marker.factionKey);
				if (faction)
				{
					Color factionColor = faction.GetFactionColor();
					color = factionColor.PackToInt();
					
					// Debug: Log color assignment for first few markers
					static int colorLogCount = 0;
					colorLogCount++;
					if (colorLogCount <= 5)
					{
						Print(string.Format("GRAD_BC_ReplayMapLayer: Faction '%1' color from API: 0x%2", 
							marker.factionKey, color.ToString(16)), LogLevel.NORMAL);
					}
				}
				else
				{
					static int noFactionCount = 0;
					noFactionCount++;
					if (noFactionCount <= 3)
					{
						Print(string.Format("GRAD_BC_ReplayMapLayer: Faction '%1' not found in FactionManager", marker.factionKey), LogLevel.WARNING);
					}
				}
			}
				
			if (!marker.isAlive)
				color = 0x80808080; // Gray for dead
			
			// Draw unit icon with directional chevron
			DrawUnitMarker(marker.position, marker.direction, marker.unitType, color, marker.isInVehicle, marker.isAlive);
		}
		
		// Draw projectiles as lines from firing position to impact position
		array<ref GRAD_BC_ReplayProjectileMarker> projectilesToRender = {};
		bool isLiveReplay = false;
		if (replayManager && replayManager.IsPlayingBack())
		{
			projectilesToRender = m_projectileMarkers;
			isLiveReplay = true;
		}
		else if (m_hasLastFrame)
		{
			projectilesToRender = m_lastFrameProjectileMarkers;
		}
		
		// Debug: Log projectile rendering details
		if (projectilesToRender.Count() > 0)
		{
			string source;
			if (isLiveReplay)
				source = "live replay";
			else
				source = "last frame";
			
			Print(string.Format("GRAD_BC_ReplayMapLayer: Drawing %1 projectiles (source: %2)", 
				projectilesToRender.Count(), 
				source), LogLevel.NORMAL);
		}
		
		int visibleCount = 0;
		foreach (GRAD_BC_ReplayProjectileMarker projMarker : projectilesToRender)
		{
			if (!projMarker.isVisible)
				continue;
			
			visibleCount++;
			
			// Debug: Log first few projectile lines being drawn
			if (visibleCount <= 3)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Drawing projectile line - Type: %1, From: [%2, %3, %4] To: [%5, %6, %7]",
					projMarker.projectileType,
					projMarker.position[0], projMarker.position[1], projMarker.position[2],
					projMarker.impactPosition[0], projMarker.impactPosition[1], projMarker.impactPosition[2]), LogLevel.NORMAL);
			}
				
			// Draw line from firing position to impact position (bright red for visibility)
			DrawLine(projMarker.position, projMarker.impactPosition, 2, Color.RED);
		}
		
		// Debug: Summary of what was drawn
		if (visibleCount > 0)
		{
			Print(string.Format("GRAD_BC_ReplayMapLayer: Drew %1 visible projectile lines (out of %2 total projectiles)",
				visibleCount, projectilesToRender.Count()), LogLevel.NORMAL);
		}
		
		Print(string.Format("GRAD_BC_ReplayMapLayer: Draw() completed - %1 player markers, %2 projectile markers, %3 total commands", 
			markersToRender.Count(), projectilesToRender.Count(), m_Commands.Count()));
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw an image texture at a world position
	override void DrawImage(vector center, int width, int height, SharedItemRef tex)
	{
		ImageDrawCommand cmd = new ImageDrawCommand();
		
		int xcp, ycp;		
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		
		cmd.m_Position = Vector(xcp - (width/2), ycp - (height/2), 0);
		cmd.m_pTexture = tex;
		cmd.m_Size = Vector(width, height, 0);
		
		m_Commands.Insert(cmd);
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw an image with a specific color tint
	void DrawImageColor(vector center, int width, int height, SharedItemRef tex, int color)
	{
		ImageDrawCommand cmd = new ImageDrawCommand();
		
		int xcp, ycp;		
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		
		cmd.m_Position = Vector(xcp - (width/2), ycp - (height/2), 0);
		cmd.m_pTexture = tex;
		cmd.m_Size = Vector(width, height, 0);
		cmd.m_iColor = color;
		
		m_Commands.Insert(cmd);
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw an image with rotation and color tint
	void DrawImageColorRotated(vector center, int width, int height, SharedItemRef tex, int color, float rotationDegrees)
	{
		ImageDrawCommand cmd = new ImageDrawCommand();
		
		int xcp, ycp;		
		m_MapEntity.WorldToScreen(center[0], center[2], xcp, ycp, true);
		
		cmd.m_Position = Vector(xcp - (width/2), ycp - (height/2), 0);
		cmd.m_pTexture = tex;
		cmd.m_Size = Vector(width, height, 0);
		cmd.m_iColor = color;
		cmd.m_fRotation = rotationDegrees;
		
		m_Commands.Insert(cmd);
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw a unit marker with icon and directional chevron
	protected void DrawUnitMarker(vector position, float direction, string unitType, int color, bool isVehicle, bool isAlive)
	{
		// Icon size in world units (meters)
		float iconSize;
		if (isVehicle)
			iconSize = 50.0;
		else
			iconSize = 35.0;
		

		// Get the appropriate texture
		string texturePath = m_unitTypeTextures.Get(unitType);
		if (texturePath == "")
			texturePath = m_unitTypeTextures.Get("Default");
		
		// If unit is dead, use dead icon
		if (!isAlive)
			texturePath = m_unitTypeTextures.Get("Dead");
		
		// Load and draw the unit icon texture
		if (texturePath != "")
		{
			int iconPixelSize;
			if (isVehicle)
				iconPixelSize = 36;
			else
				iconPixelSize = 28;
			
			// Check if texture is already loaded, otherwise load and cache it
			SharedItemRef texture = m_loadedTextures.Get(texturePath);
			if (!texture && m_Canvas)
			{
				texture = m_Canvas.LoadTexture(texturePath);
				if (texture)
					m_loadedTextures.Set(texturePath, texture);
			}
			
			if (texture)
			{
				// Draw faction-colored circle slightly larger than icon
				float circleRadius = (iconPixelSize * 0.65); // Circle 30% larger than icon in screen space
				DrawCircle(position, circleRadius, color, 16);
				
				// Draw icon rotated to show direction, in WHITE color on top of faction-colored circle
				DrawImageColorRotated(position, iconPixelSize, iconPixelSize, texture, 0xFFFFFFFF, direction);
			}
			else
			{
				// Fallback to white circle if texture fails to load
				DrawCircle(position, iconSize * 0.3, 0xFFFFFFFF, 12);
			}
		}
		else
		{
			// Fallback to white circle if no texture path
			DrawCircle(position, iconSize * 0.3, 0xFFFFFFFF, 12);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Called by replay manager to update marker positions
	void UpdateReplayFrame(GRAD_BC_ReplayFrame frame)
	{
		Print(string.Format("GRAD_BC_ReplayMapLayer: Received frame with %1 players, %2 projectiles", 
			frame.players.Count(), frame.projectiles.Count()), LogLevel.NORMAL);
		
		// Clear existing markers
		m_playerMarkers.Clear();
		m_projectileMarkers.Clear();
		
		// Create player markers
		foreach (GRAD_BC_PlayerSnapshot playerSnapshot : frame.players)
		{
			GRAD_BC_ReplayPlayerMarker marker = new GRAD_BC_ReplayPlayerMarker();
			marker.playerId = playerSnapshot.playerId;
			marker.playerName = playerSnapshot.playerName;
			marker.factionKey = playerSnapshot.factionKey;
			marker.position = playerSnapshot.position;
			marker.direction = playerSnapshot.angles[0]; // Yaw
			marker.isAlive = playerSnapshot.isAlive;
			marker.isInVehicle = playerSnapshot.isInVehicle;
			marker.unitType = playerSnapshot.unitRole; // Use role from snapshot
			marker.isVisible = true;
			
			// Debug: Log position and faction data for first few frames
			static int positionLogCount = 0;
			positionLogCount++;
			if (positionLogCount <= 10)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Player %1 (%2) faction: '%3', position: [%4, %5, %6], direction: %7°, type: %8", 
					marker.playerId, marker.playerName, marker.factionKey, marker.position[0], marker.position[1], marker.position[2], 
					marker.direction, marker.unitType));
			}
			
			m_playerMarkers.Insert(marker);
		}
		
		// Create projectile markers
		foreach (GRAD_BC_ProjectileSnapshot projSnapshot : frame.projectiles)
		{
			GRAD_BC_ReplayProjectileMarker marker = new GRAD_BC_ReplayProjectileMarker();
			marker.projectileType = projSnapshot.projectileType;
			marker.position = projSnapshot.position; // firing position
			marker.impactPosition = projSnapshot.impactPosition; // impact position
			marker.velocity = projSnapshot.velocity;
			marker.isVisible = true;
			
			// Debug log for first few projectiles
			static int projLogCount = 0;
			projLogCount++;
			if (projLogCount <= 5)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Projectile %1 - Type: %2, From: [%3, %4, %5], To: [%6, %7, %8]",
					projLogCount, marker.projectileType,
					marker.position[0], marker.position[1], marker.position[2],
					marker.impactPosition[0], marker.impactPosition[1], marker.impactPosition[2]));
			}
			
			m_projectileMarkers.Insert(marker);
		}
		
		// Save this frame as the last frame for persistent display
		m_lastFramePlayerMarkers.Clear();
		m_lastFrameProjectileMarkers.Clear();
		
		// Deep copy current markers to last frame
		foreach (GRAD_BC_ReplayPlayerMarker playerMarker : m_playerMarkers)
		{
			GRAD_BC_ReplayPlayerMarker lastMarker = new GRAD_BC_ReplayPlayerMarker();
			lastMarker.playerId = playerMarker.playerId;
			lastMarker.playerName = playerMarker.playerName;
			lastMarker.factionKey = playerMarker.factionKey;
			lastMarker.position = playerMarker.position;
			lastMarker.direction = playerMarker.direction;
			lastMarker.isAlive = playerMarker.isAlive;
			lastMarker.isInVehicle = playerMarker.isInVehicle;
			lastMarker.unitType = playerMarker.unitType;
			lastMarker.isVisible = playerMarker.isVisible;
			m_lastFramePlayerMarkers.Insert(lastMarker);
		}
		
		foreach (GRAD_BC_ReplayProjectileMarker projMarker : m_projectileMarkers)
		{
			GRAD_BC_ReplayProjectileMarker lastMarker = new GRAD_BC_ReplayProjectileMarker();
			lastMarker.projectileType = projMarker.projectileType;
			lastMarker.position = projMarker.position;
			lastMarker.impactPosition = projMarker.impactPosition;
			lastMarker.velocity = projMarker.velocity;
			lastMarker.isVisible = projMarker.isVisible;
			m_lastFrameProjectileMarkers.Insert(lastMarker);
		}
		
		m_hasLastFrame = true;
		m_bIsInReplayMode = true; // Mark that we're in replay mode
		Print(string.Format("GRAD_BC_ReplayMapLayer: Saved last frame with %1 players, %2 projectiles for persistent display", 
			m_lastFramePlayerMarkers.Count(), m_lastFrameProjectileMarkers.Count()));
		
		// Log frame update for debugging
		Print(string.Format("GRAD_BC_ReplayMapLayer: Updated frame with %1 players, %2 projectiles", 
			m_playerMarkers.Count(), m_projectileMarkers.Count()), LogLevel.NORMAL);
	}
}

// Marker classes for replay display
class GRAD_BC_ReplayPlayerMarker : Managed
{
	int playerId;
	string playerName;
	string factionKey;
	vector position;
	float direction;
	bool isAlive;
	bool isInVehicle;
	string unitType; // Added unit type
	bool isVisible;
}

class GRAD_BC_ReplayProjectileMarker : Managed
{
	string projectileType;
	vector position; // firing position
	vector impactPosition; // impact/endpoint position
	vector velocity;
	bool isVisible;
}
