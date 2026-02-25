// Fix: on dedicated server sessions PS_SpectatorMenu.OnMenuOpen() closes the
// menu immediately before m_MapEntity is ever assigned. OnMenuInit always runs
// (even when the menu subsequently closes itself), so we assign m_MapEntity
// there. This ensures external code that holds a reference to s_SpectatorMenu
// and calls map-related helpers (OpenMap / CloseMap / Action_ToggleMap) does
// not crash with a null m_MapEntity.
//
// OpenMapWithConfig: opens the map anchored to m_wMapFrame using an explicit
// map config resource (e.g. the replay config) instead of the gamemode's
// SCR_MapConfigComponent config. Replicates the double-open workaround from
// the original OpenMap() so keybinds and layout are correct.
modded class PS_SpectatorMenu : MenuBase
{
	override void OnMenuInit()
	{
		super.OnMenuInit();

		if (!m_MapEntity)
			m_MapEntity = SCR_MapEntity.GetMapInstance();
	}

	void OpenMapWithConfig(ResourceName mapConfig)
	{
		Print("BC Debug - PS_SpectatorMenu.OpenMapWithConfig: called", LogLevel.NORMAL);

		if (!m_MapEntity)
		{
			Print("BC Debug - PS_SpectatorMenu.OpenMapWithConfig: m_MapEntity is null, aborting", LogLevel.ERROR);
			return;
		}

		if (!m_wMapFrame)
		{
			Print("BC Debug - PS_SpectatorMenu.OpenMapWithConfig: m_wMapFrame is null, aborting", LogLevel.ERROR);
			return;
		}

		SCR_ManualCamera camera = SCR_ManualCamera.Cast(GetGame().GetCameraManager().CurrentCamera());
		if (camera)
			camera.SetInputEnabled(false);

		m_wMapFrame.SetVisible(true);

		SCR_WidgetHelper.RemoveAllChildren(m_wMapFrame.FindAnyWidget("ToolMenuHoriz"));

		Print("BC Debug - PS_SpectatorMenu.OpenMapWithConfig: opening map with replay config", LogLevel.NORMAL);
		MapConfiguration mapConfigFullscreen = m_MapEntity.SetupMapConfig(EMapEntityMode.FULLSCREEN, mapConfig, m_wMapFrame);
		mapConfigFullscreen.MapEntityMode = EMapEntityMode.PLAIN;
		m_MapEntity.OpenMap(mapConfigFullscreen);
		m_MapEntity.CloseMap();
		mapConfigFullscreen.MapEntityMode = EMapEntityMode.FULLSCREEN;
		m_MapEntity.OpenMap(mapConfigFullscreen);
		Print("BC Debug - PS_SpectatorMenu.OpenMapWithConfig: map open complete", LogLevel.NORMAL);
	}
}
