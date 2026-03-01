// BC modded PS_SpectatorMenu:
//
// - OnMenuOpen: closes BriefingMapMenu before super (it is never closed by PSCore
//   when SpectatorMenu opens, causing its VoiceChatFrame/map to show through).
//   Must run BEFORE super.OnMenuOpen() because on singleplayer-host sessions
//   super.OnMenuOpen() may call Close() on itself immediately, and any code after
//   it would never run.
//
// - OnMenuInit: assigns m_MapEntity if it is null (on dedicated server,
//   OnMenuOpen() returns early so the base class never assigns it; OnMenuInit
//   always runs, so we do it here).
//   Also re-applies panel hiding if a second SpectatorMenu instance opens during
//   replay (SwitchToObserver has no guard against re-opening an existing menu).
//
// - OpenReplayMap / CloseReplayMap: open/close the map using a custom replay
//   config resource, then surgically hide/restore specific SpectatorMenu panels
//   so only the map + BC filter buttons are visible.
//   Avoids Action_SwitchSpectatorUI() because that hides OverlayFooter entirely,
//   which would also hide the BC_ToggleEmptyVehicles / BC_ToggleCivilians children.
//
// - Stacked-menu guard: SwitchToObserver() calls OpenMenu(SpectatorMenu) with no
//   prior-close check. If it fires twice (e.g. spectator camera entity triggers a
//   second OnControlledEntityChanged), a second SpectatorMenu instance is pushed
//   on top with all panels visible. We detect this in OnMenuInit via the static
//   s_ReplayMapOpen flag and immediately re-hide panels on the new instance.
//
modded class PS_SpectatorMenu : MenuBase
{
	protected bool m_bReplayMapOpen;

	// Static flag so a newly-opened second instance knows replay is active
	// (instance members are fresh on each new menu open).
	static bool s_bAnyReplayMapOpen;

	// Instance counter for diagnosing stacked-menu openings
	static int s_iOpenCount;

	// Cached nav button refs — set in OpenReplayMap, cleared in CloseReplayMap
	protected Widget m_wNavChat;
	protected Widget m_wNavVoice;
	protected Widget m_wNavSwitch;

	override void OnMenuOpen()
	{
		s_iOpenCount++;
		Print("BC Debug - SpectatorMenu.OnMenuOpen #" + s_iOpenCount + " this=" + this + " s_SpectatorMenu=" + s_SpectatorMenu, LogLevel.NORMAL);

		// Get root BEFORE CloseMenuByPreset — PSCore removes it synchronously so FindMenuByPreset
		// returns null after the close call.
		MenuBase briefingMenu = GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.BriefingMapMenu);

		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.BriefingMapMenu);

		// Forcibly hide BriefingMapMenu's widget tree (VoiceChatFrame + OverlayFooter nav buttons)
		// which persists and bleeds through even after the menu is logically closed.
		if (briefingMenu)
			briefingMenu.GetRootWidget().SetVisible(false);

		super.OnMenuOpen();
	}

	override void OnMenuInit()
	{
		super.OnMenuInit();

		Print("BC Debug - SpectatorMenu.OnMenuInit #" + s_iOpenCount + " this=" + this + " s_bAnyReplayMapOpen=" + s_bAnyReplayMapOpen, LogLevel.NORMAL);

		if (!m_MapEntity)
			m_MapEntity = SCR_MapEntity.GetMapInstance();

		// If a second SpectatorMenu instance opens while replay is active, re-apply
		// panel hiding immediately. OnMenuInit always runs even when OnMenuOpen
		// self-closes, and widget refs are valid here.
		if (s_bAnyReplayMapOpen)
		{
			Print("BC Debug - SpectatorMenu.OnMenuInit: replay active on new instance, re-hiding panels", LogLevel.NORMAL);
			m_bReplayMapOpen = true;
			HideReplayPanels();
		}
	}

	// Override Action_SwitchSpectatorUI to be a no-op during replay.
	// When the replay map is open (m_wMapFrame visible), the I key must not restore
	// the panels we hid — PSCore's toggle checks IsVisible() and restores everything.
	override void Action_SwitchSpectatorUI()
	{
		if (m_wMapFrame && m_wMapFrame.IsVisible())
			return;
		super.Action_SwitchSpectatorUI();
	}

	// Re-enforce panel visibility every frame during replay.
	// PSCore's OnMenuUpdate slides panels via FrameSlot.SetPosX unconditionally —
	// this may re-show widgets even after SetVisible(false), so we undo it here.
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		if (!m_bReplayMapOpen)
			return;

		HideReplayPanels();
	}

	// Hides the two side panels and three vanilla nav buttons.
	// Called from OnMenuInit (new stacked instance) and OnMenuUpdate (every frame).
	protected void HideReplayPanels()
	{
		if (m_wAlivePlayerList)
			m_wAlivePlayerList.SetVisible(false);
		if (m_wVoiceChatList)
			m_wVoiceChatList.SetVisible(false);

		// Nav buttons: use cached refs if available, otherwise find them
		if (!m_wNavChat)
		{
			Widget root = GetRootWidget();
			if (root)
			{
				m_wNavChat = root.FindAnyWidget("NavigationChat");
				m_wNavVoice = root.FindAnyWidget("NavigationFactionVoice");
				m_wNavSwitch = root.FindAnyWidget("NavigationSwitchSpectatorUI");
			}
		}
		if (m_wNavChat)
			m_wNavChat.SetVisible(false);
		if (m_wNavVoice)
			m_wNavVoice.SetVisible(false);
		if (m_wNavSwitch)
			m_wNavSwitch.SetVisible(false);
	}

	// Open the map using a custom replay config, then hide side panels and vanilla
	// nav buttons while keeping OverlayFooter visible so BC filter buttons can show.
	void OpenReplayMap(ResourceName mapConfig)
	{
		if (!m_MapEntity)
		{
			Print("BC Debug - OpenReplayMap: m_MapEntity null, aborting", LogLevel.ERROR);
			return;
		}

		if (!m_wMapFrame)
		{
			Print("BC Debug - OpenReplayMap: m_wMapFrame null, aborting", LogLevel.ERROR);
			return;
		}

		// Disable spectator camera input while map is shown
		SCR_ManualCamera camera = SCR_ManualCamera.Cast(GetGame().GetCameraManager().CurrentCamera());
		if (camera)
			camera.SetInputEnabled(false);

		m_wMapFrame.SetVisible(true);
		SCR_WidgetHelper.RemoveAllChildren(m_wMapFrame.FindAnyWidget("ToolMenuHoriz"));

		// Double-open workaround (same as PSCore OpenMap)
		MapConfiguration mapConfigFullscreen = m_MapEntity.SetupMapConfig(EMapEntityMode.FULLSCREEN, mapConfig, m_wMapFrame);
		mapConfigFullscreen.MapEntityMode = EMapEntityMode.PLAIN;
		m_MapEntity.OpenMap(mapConfigFullscreen);
		m_MapEntity.CloseMap();
		mapConfigFullscreen.MapEntityMode = EMapEntityMode.FULLSCREEN;
		m_MapEntity.OpenMap(mapConfigFullscreen);
		Print("BC Debug - OpenReplayMap: map opened", LogLevel.NORMAL);

		m_bReplayMapOpen = true;
		s_bAnyReplayMapOpen = true;

		// Hide the two slide-in side panels (protected members of base class)
		Print("BC Debug - OpenReplayMap: m_wAlivePlayerList=" + m_wAlivePlayerList + " m_wVoiceChatList=" + m_wVoiceChatList, LogLevel.NORMAL);
		HideReplayPanels();
		Print("BC Debug - OpenReplayMap: after hide - AliveVisible=" + (m_wAlivePlayerList && m_wAlivePlayerList.IsVisible()) + " VoiceVisible=" + (m_wVoiceChatList && m_wVoiceChatList.IsVisible()), LogLevel.NORMAL);

		// Show BC replay filter buttons
		Widget root = GetRootWidget();
		Widget toggleVehicles = root.FindAnyWidget("BC_ToggleEmptyVehicles");
		if (toggleVehicles)
			toggleVehicles.SetVisible(true);
		Widget toggleCivilians = root.FindAnyWidget("BC_ToggleCivilians");
		if (toggleCivilians)
			toggleCivilians.SetVisible(true);

		Print("BC Debug - OpenReplayMap: panels hidden, BC buttons shown", LogLevel.NORMAL);
	}

	// Close the replay map and restore all hidden panels/buttons.
	void CloseReplayMap()
	{
		if (!m_MapEntity)
			return;

		Print("BC Debug - CloseReplayMap: called", LogLevel.NORMAL);

		m_bReplayMapOpen = false;
		s_bAnyReplayMapOpen = false;
		s_iOpenCount = 0;

		// Hide BC replay filter buttons
		Widget root = GetRootWidget();
		Widget toggleVehicles = root.FindAnyWidget("BC_ToggleEmptyVehicles");
		if (toggleVehicles)
			toggleVehicles.SetVisible(false);
		Widget toggleCivilians = root.FindAnyWidget("BC_ToggleCivilians");
		if (toggleCivilians)
			toggleCivilians.SetVisible(false);

		// Restore the three vanilla nav buttons (use cached refs from OpenReplayMap)
		if (m_wNavChat)
			m_wNavChat.SetVisible(true);
		if (m_wNavVoice)
			m_wNavVoice.SetVisible(true);
		if (m_wNavSwitch)
			m_wNavSwitch.SetVisible(true);
		m_wNavChat = null;
		m_wNavVoice = null;
		m_wNavSwitch = null;

		// Restore the two side panels
		if (m_wAlivePlayerList)
			m_wAlivePlayerList.SetVisible(true);
		if (m_wVoiceChatList)
			m_wVoiceChatList.SetVisible(true);

		// PSCore's own CloseMap: re-enables camera, hides m_wMapFrame, calls SCR_MapEntity.CloseMap()
		CloseMap();

		Print("BC Debug - CloseReplayMap: map closed, panels restored", LogLevel.NORMAL);
	}
}
