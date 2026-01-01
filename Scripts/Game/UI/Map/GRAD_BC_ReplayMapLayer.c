[BaseContainerProps()]
class GRAD_BC_ReplayMapLayer : GRAD_MapMarkerLayer // ✅ Inherit from proven working class
{
	// Replay display state
	protected ref array<ref GRAD_BC_ReplayPlayerMarker> m_playerMarkers = {};
	protected ref array<ref GRAD_BC_ReplayProjectileMarker> m_projectileMarkers = {};
	
	// Keep last frame data for persistent display
	protected ref array<ref GRAD_BC_ReplayPlayerMarker> m_lastFramePlayerMarkers = {};
	protected ref array<ref GRAD_BC_ReplayProjectileMarker> m_lastFrameProjectileMarkers = {};
	protected bool m_hasLastFrame = false;
	
	// Colors for different factions
	protected ref map<string, int> m_factionColors = new map<string, int>();
	
	// Unit type icon textures
	protected ref map<string, string> m_unitTypeTextures = new map<string, string>();
	
	// Cached loaded textures
	protected ref map<string, ref SharedItemRef> m_loadedTextures = new map<string, ref SharedItemRef>();
	
	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		super.Init();
		
		Print("GRAD_BC_ReplayMapLayer: Initializing replay map layer", LogLevel.NORMAL);
		
		// Initialize faction colors
		m_factionColors.Set("US", Color.BLUE);
		m_factionColors.Set("USSR", Color.RED);
		m_factionColors.Set("", Color.WHITE); // Default/unknown
		
		// Initialize unit type textures
		m_unitTypeTextures.Set("AmmoBearer", "{FB48FAD32DA8BC91}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_AmmoBearer.edds");
		m_unitTypeTextures.Set("AntiTank", "{00B30C29FAF85E1C}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_AntiTank.edds");
		m_unitTypeTextures.Set("Custom", "{A489F552FB7489C3}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Custom.edds");
		m_unitTypeTextures.Set("Dead", "{54B61CF30B644B48}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Dead.edds");
		m_unitTypeTextures.Set("Grenadier", "{DDAEEF112BEBCF94}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Grenadier.edds");
		m_unitTypeTextures.Set("Leader", "{A26C465A6AE2AA17}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Leader.edds");
		m_unitTypeTextures.Set("MachineGunner", "{EF1F445746A3391A}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_MachineGunner.edds");
		m_unitTypeTextures.Set("Medic", "{F3FCC3B9732551D9}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Medic.edds");
		m_unitTypeTextures.Set("Player", "{9F4D0043E24255E8}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Player.edds");
		m_unitTypeTextures.Set("RadioOperator", "{B9F0BD39FF1881A3}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_RadioOperator.edds");
		m_unitTypeTextures.Set("Rifleman", "{AE53796BC5D21A08}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Rifleman.edds");
		m_unitTypeTextures.Set("Sharpshooter", "{0A78405E73C36477}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Sharpshooter.edds");
		m_unitTypeTextures.Set("Spotter", "{9A61AD7EADB131FD}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Spotter.edds");
		m_unitTypeTextures.Set("Unarmed", "{9164E45B9A237FE9}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Unarmed.edds");
		
		// Default fallback
		m_unitTypeTextures.Set("Default", "{AE53796BC5D21A08}UI/Textures/Editor/EditableEntities/Characters/EditableEntity_Character_Rifleman.edds");
		
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
				
			// Get faction color
			int color = m_factionColors.Get(marker.factionKey);
			if (color == 0)
				color = 0xFFFFFFFF;
				
			// Make colors brighter with full alpha
			if (marker.factionKey == "US")
				color = 0xFF0080FF; // Bright blue
			else if (marker.factionKey == "USSR") 
				color = 0xFFFF4040; // Bright red
			else
				color = 0xFFFFFF40; // Bright yellow
				
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
		
		// Draw direction chevron underneath icon
		DrawDirectionalChevron(position, direction, color, iconSize * 1.5);
		
		// Load and draw the unit icon texture
		if (texturePath != "")
		{
			int iconPixelSize;
			if (isVehicle)
				iconPixelSize = 40;
			else
				iconPixelSize = 32;
			
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
				DrawImage(position, iconPixelSize, iconPixelSize, texture);
			}
			else
			{
				// Fallback to circles if texture fails to load
				DrawCircle(position, iconSize * 0.5, color, 12);
				DrawCircle(position, iconSize * 0.2, color, 8);
			}
		}
		else
		{
			// Fallback to circles if no texture path
			DrawCircle(position, iconSize * 0.5, color, 12);
			DrawCircle(position, iconSize * 0.2, color, 8);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Draw a directional chevron showing unit facing
	protected void DrawDirectionalChevron(vector position, float direction, int color, float size)
	{
		// Convert yaw angle to radians (direction is in degrees)
		float angleRad = direction * Math.DEG2RAD;
		
		float halfSize = size * 0.5;
		
		// Create chevron shape pointing north (0 degrees)
		array<vector> shapePoints = {};
		
		// Chevron shape (V pointing up/north initially)
		// Tip
		shapePoints.Insert(Vector(0, 0, -halfSize));
		// Left wing
		shapePoints.Insert(Vector(-halfSize * 0.5, 0, 0));
		// Back indent (makes it look like chevron)
		shapePoints.Insert(Vector(0, 0, -halfSize * 0.2));
		// Right wing  
		shapePoints.Insert(Vector(halfSize * 0.5, 0, 0));
		
		// Rotate all points by the direction angle
		array<vector> rotatedPoints = {};
		foreach (vector point : shapePoints)
		{
			float rotatedX = point[0] * Math.Cos(angleRad) - point[2] * Math.Sin(angleRad);
			float rotatedZ = point[0] * Math.Sin(angleRad) + point[2] * Math.Cos(angleRad);
			rotatedPoints.Insert(position + Vector(rotatedX, 0, rotatedZ));
		}
		
		// Draw the chevron outline
		for (int i = 0; i < rotatedPoints.Count(); i++)
		{
			int nextIdx = (i + 1) % rotatedPoints.Count();
			DrawLine(rotatedPoints[i], rotatedPoints[nextIdx], 2, color);
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
			
			// Debug: Log position data for first few frames
			static int positionLogCount = 0;
			positionLogCount++;
			if (positionLogCount <= 10)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Player %1 (%2) position: [%3, %4, %5], direction: %6, type: %7", 
					marker.playerId, marker.playerName, marker.position[0], marker.position[1], marker.position[2], 
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
