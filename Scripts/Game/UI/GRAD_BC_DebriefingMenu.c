modded class PS_DebriefingMenu
{
	protected MapEntity m_MapEntity;
	
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		
		// Initialize map for replay
		InitializeMapForReplay();
	}
	
	void InitializeMapForReplay()
	{
		// Find the map widget in the layout
		Widget mapWidget = GetRootWidget().FindAnyWidget("Map");
		if (!mapWidget)
		{
			Print("GRAD_BC_DebriefingMenu: Map widget not found in layout", LogLevel.WARNING);
			return;
		}
		
		Print("GRAD_BC_DebriefingMenu: Map widget found, initializing map entity", LogLevel.NORMAL);
		
		// Get the map entity from the world
		m_MapEntity = SCR_MapEntity.GetMapInstance();
		if (!m_MapEntity)
		{
			Print("GRAD_BC_DebriefingMenu: Map entity not found in world", LogLevel.WARNING);
			return;
		}
		
		Print("GRAD_BC_DebriefingMenu: Map entity found, map initialized for replay", LogLevel.NORMAL);
		
		// The map entity should already have the replay layer from the config
		// Check if replay layer is present
		GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.Cast(m_MapEntity.GetMapModule(GRAD_BC_ReplayMapLayer));
		if (replayLayer)
		{
			Print("GRAD_BC_DebriefingMenu: Replay layer found on map entity!", LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_DebriefingMenu: WARNING - Replay layer not found on map entity", LogLevel.WARNING);
		}
	}
}
