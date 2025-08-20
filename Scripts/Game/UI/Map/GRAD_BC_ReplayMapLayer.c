[BaseContainerProps()]
class GRAD_BC_ReplayMapLayer : SCR_MapModuleBase
{
	protected Widget m_Widget;
	protected CanvasWidget m_Canvas;
	
	protected ResourceName m_Layout = "{A6A79ABB08D490BE}UI/Layouts/Map/MapCanvasLayer.layout";
	
	// Replay display state
	protected ref array<ref GRAD_BC_ReplayPlayerMarker> m_playerMarkers = {};
	protected ref array<ref GRAD_BC_ReplayProjectileMarker> m_projectileMarkers = {};
	
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
		super.OnMapOpen(config);
		
		Print("GRAD_BC_ReplayMapLayer: OnMapOpen called, initializing canvas", LogLevel.NORMAL);
		
		// Create the widget and canvas when map opens
		m_Widget = GetGame().GetWorkspace().CreateWidgets(m_Layout);
		if (m_Widget)
		{
			m_Canvas = CanvasWidget.Cast(m_Widget.FindAnyWidget("Canvas"));
			if (m_Canvas)
			{
				Print("GRAD_BC_ReplayMapLayer: Canvas initialized successfully", LogLevel.NORMAL);
			}
			else
			{
				Print("GRAD_BC_ReplayMapLayer: Failed to find Canvas widget in layout", LogLevel.ERROR);
			}
		}
		else
		{
			Print("GRAD_BC_ReplayMapLayer: Failed to create widget from layout", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);
		
		Print("GRAD_BC_ReplayMapLayer: OnMapClose called, cleaning up", LogLevel.NORMAL);
		
		// Remove widget from hierarchy when map closes
		if (m_Widget)
			m_Widget.RemoveFromHierarchy();
	}
	
	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		if (!m_Canvas)
			return;
			
		// Draw replay markers on the map
		GRAD_BC_ReplayManager replayManager = GRAD_BC_ReplayManager.GetInstance();
		if (replayManager && replayManager.IsPlayingBack())
		{
			UpdateReplayMarkerDrawCommands();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdateReplayMarkerDrawCommands()
	{
		if (!m_Canvas)
		{
			Print("GRAD_BC_ReplayMapLayer: Canvas is null, cannot draw markers", LogLevel.ERROR);
			return;
		}
			
		// Create draw commands array
		array<ref CanvasWidgetCommand> drawCommands = {};
		
		// Get map entity for coordinate conversion
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
		{
			Print("GRAD_BC_ReplayMapLayer: Map entity is null, cannot convert coordinates", LogLevel.ERROR);
			return;
		}
			
		// Create draw commands for player markers
		foreach (GRAD_BC_ReplayPlayerMarker marker : m_playerMarkers)
		{
			if (!marker.isVisible)
			{
				Print("GRAD_BC_ReplayMapLayer: Skipping invisible marker");
				continue;
			}
				
			// Convert world position to screen coordinates
			float screenX, screenY;
			mapEntity.WorldToScreen(marker.position[0], marker.position[2], screenX, screenY, true);
			
			// Always log first few conversions for debugging
			static int debugConversionCount = 0;
			debugConversionCount++;
			if (debugConversionCount <= 5)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Converting world pos [%1, %2, %3] -> screen [%4, %5]", 
					marker.position[0], marker.position[1], marker.position[2], screenX, screenY));
			}
			
			// Debug: Log coordinate conversion (only occasionally)
			static int coordLogCounter = 0;
			coordLogCounter++;
			if (coordLogCounter % 100 == 0) // Only log every 100th marker
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Player at world pos [%1, %2, %3] -> screen [%4, %5]", 
					marker.position[0], marker.position[1], marker.position[2], screenX, screenY));
			}
			
			// Check if marker is within visible bounds (skip if way off screen)
			if (screenX < -100 || screenX > 2000 || screenY < -100 || screenY > 2000)
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Marker at [%1, %2] is outside visible bounds, skipping", screenX, screenY));
				continue;
			}
			
			Print(string.Format("GRAD_BC_ReplayMapLayer: Creating circle for player at screen [%1, %2]", screenX, screenY));
			
			// Create circle draw command for player
			PolygonDrawCommand playerCircle = new PolygonDrawCommand();
			
			// Get faction color
			int color = m_factionColors.Get(marker.factionKey);
			if (color == 0)
				color = Color.WHITE;
				
			// Set up circle properties
			float markerSize = 6.0; // radius in pixels
			int numPoints = 12; // circle resolution
			
			// Create circle vertices (flat array of x,y coordinates)
			playerCircle.m_Vertices = new array<float>();
			for (int i = 0; i < numPoints; i++)
			{
				float angle = (i * Math.PI * 2) / numPoints;
				float x = screenX + Math.Cos(angle) * markerSize;
				float y = screenY + Math.Sin(angle) * markerSize;
				playerCircle.m_Vertices.Insert(x);
				playerCircle.m_Vertices.Insert(y);
			}
			
			if (marker.isAlive)
			{
				// Set faction color based on team
				if (marker.factionKey == "US")
					playerCircle.m_iColor = Color.FromInt(0xC8006CFF); // Blue with alpha 200
				else if (marker.factionKey == "USSR") 
					playerCircle.m_iColor = Color.FromInt(0xC8FF0000); // Red with alpha 200
				else
					playerCircle.m_iColor = Color.FromInt(0xC8FFFFFF); // White with alpha 200
			}
			else
			{
				// Dead players - semi-transparent red
				playerCircle.m_iColor = Color.FromInt(0x64FF0000); // Semi-transparent red (alpha 100)
			}
			
			// Debug: Log color and vertices (only occasionally)
			if (coordLogCounter % 100 == 0) // Use same counter as above
			{
				Print(string.Format("GRAD_BC_ReplayMapLayer: Created circle with color %1, %2 vertices at [%3, %4]", 
					playerCircle.m_iColor, playerCircle.m_Vertices.Count(), screenX, screenY));
			}
			
			drawCommands.Insert(playerCircle);
		}
		
		// Create draw commands for projectile markers
		foreach (GRAD_BC_ReplayProjectileMarker marker : m_projectileMarkers)
		{
			if (!marker.isVisible)
				continue;
				
			// Convert world position to screen coordinates
			float screenX, screenY;
			mapEntity.WorldToScreen(marker.position[0], marker.position[2], screenX, screenY, true);
			
			// Create small circle for projectile
			PolygonDrawCommand projectileCircle = new PolygonDrawCommand();
			
			float markerSize = 2.0; // Small radius for projectiles
			int numPoints = 8;
			
			projectileCircle.m_Vertices = new array<float>();
			for (int i = 0; i < numPoints; i++)
			{
				float angle = (i * Math.PI * 2) / numPoints;
				float x = screenX + Math.Cos(angle) * markerSize;
				float y = screenY + Math.Sin(angle) * markerSize;
				projectileCircle.m_Vertices.Insert(x);
				projectileCircle.m_Vertices.Insert(y);
			}
			
			projectileCircle.m_iColor = Color.FromInt(0xFFFFFF00); // Bright yellow
			drawCommands.Insert(projectileCircle);
		}
		
		// Apply all draw commands to canvas
		m_Canvas.SetDrawCommands(drawCommands);
		
		// Always log drawing operations for debugging
		Print(string.Format("GRAD_BC_ReplayMapLayer: Applied %1 draw commands to canvas", drawCommands.Count()));
		
		// Status logging - always for debugging
		Print(string.Format("GRAD_BC_ReplayMapLayer: Drew %1 players, %2 projectiles on map", 
			m_playerMarkers.Count(), m_projectileMarkers.Count()), LogLevel.NORMAL);
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
