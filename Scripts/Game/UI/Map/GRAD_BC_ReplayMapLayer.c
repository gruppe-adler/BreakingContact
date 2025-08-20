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
	
	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		super.Init();
		
		Print("GRAD_BC_ReplayMapLayer: Initializing replay map layer", LogLevel.NORMAL);
		
		// Initialize faction colors
		m_factionColors.Set("US", Color.BLUE);
		m_factionColors.Set("USSR", Color.RED);
		m_factionColors.Set("", Color.WHITE); // Default/unknown
		
		Print("GRAD_BC_ReplayMapLayer: Replay map layer ready", LogLevel.NORMAL);
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
		
		// ✅ Use the proven DrawCircle method from GRAD_MapMarkerLayer parent class
		foreach (GRAD_BC_ReplayPlayerMarker marker : markersToRender)
		{
			if (!marker.isVisible)
				continue;
				
			// Get faction color
			int color = m_factionColors.Get(marker.factionKey);
			if (color == 0)
				color = 0xFFFFFFFF; // White as integer
				
			// Make colors brighter with full alpha
			if (marker.factionKey == "US")
				color = 0xFF0080FF; // Bright blue as integer
			else if (marker.factionKey == "USSR") 
				color = 0xFFFF4040; // Bright red as integer
			else
				color = 0xFFFFFF40; // Bright yellow as integer
				
			if (!marker.isAlive)
				color = 0x80FF0000; // Semi-transparent red for dead as integer
			
			// ✅ Use proven DrawCircle method - draws at world position with range in world units
			DrawCircle(marker.position, 30.0, color, 16); // 30m radius, 16 segments for smooth circle
			
			static int markerLogCounter = 0;
			markerLogCounter++;
			if (markerLogCounter % 25 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Drawing player %1 at world [%2, %3, %4] with color %5", 
					marker.playerName, marker.position[0], marker.position[1], marker.position[2], color));
			}
		}
		
		// Draw projectiles as smaller circles
		array<ref GRAD_BC_ReplayProjectileMarker> projectilesToRender = {};
		if (replayManager && replayManager.IsPlayingBack())
		{
			projectilesToRender = m_projectileMarkers;
		}
		else if (m_hasLastFrame)
		{
			projectilesToRender = m_lastFrameProjectileMarkers;
		}
		
		foreach (GRAD_BC_ReplayProjectileMarker projMarker : projectilesToRender)
		{
			if (!projMarker.isVisible)
				continue;
				
			// Small bright yellow circles for projectiles
			DrawCircle(projMarker.position, 5.0, 0xFFFFFF00, 8); // 5m radius, yellow as integer
		}
		
		Print(string.Format("GRAD_BC_ReplayMapLayer: Draw() completed - %1 player markers, %2 projectile markers, %3 total commands", 
			markersToRender.Count(), projectilesToRender.Count(), m_Commands.Count()));
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
			marker.isVisible = true;
			
			// Debug: Log position data for first few frames
			static int positionLogCount = 0;
			positionLogCount++;
			if (positionLogCount <= 10)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Player %1 (%2) position: [%3, %4, %5]", 
					marker.playerId, marker.playerName, marker.position[0], marker.position[1], marker.position[2]));
			}
			
			m_playerMarkers.Insert(marker);
		}
		
		// Create projectile markers
		foreach (GRAD_BC_ProjectileSnapshot projSnapshot : frame.projectiles)
		{
			GRAD_BC_ReplayProjectileMarker marker = new GRAD_BC_ReplayProjectileMarker();
			marker.projectileType = projSnapshot.projectileType;
			marker.position = projSnapshot.position;
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
			lastMarker.isVisible = playerMarker.isVisible;
			m_lastFramePlayerMarkers.Insert(lastMarker);
		}
		
		foreach (GRAD_BC_ReplayProjectileMarker projMarker : m_projectileMarkers)
		{
			GRAD_BC_ReplayProjectileMarker lastMarker = new GRAD_BC_ReplayProjectileMarker();
			lastMarker.projectileType = projMarker.projectileType;
			lastMarker.position = projMarker.position;
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
	bool isVisible;
}

class GRAD_BC_ReplayProjectileMarker : Managed
{
	string projectileType;
	vector position;
	vector velocity;
	bool isVisible;
}
