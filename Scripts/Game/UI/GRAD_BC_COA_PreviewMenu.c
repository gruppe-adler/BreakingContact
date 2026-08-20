// Breaking Contact additions to COALITION's briefing screen (COA_PreviewMenu):
//
//   1. A map dropdown (admins only, BRIEFING phase only) that switches the running
//      scenario to another BC map, behind a two-step confirmation.
//   2. A transparent button overlaid on COA's Time/Weather box that opens the
//      Game Master editor for admins.
//
// COA's Briefing.layout lives inside their binary data.pak and cannot be edited, so
// every widget here is created at runtime and parented into COA's existing hierarchy.
//
// IMPORTANT lifecycle notes about the base class:
//   - OnMenuOpen() early-returns via Close() on dedicated servers, so m_wRoot must be
//     null-checked after calling super.
//   - OnMenuUpdate() runs every frame and re-sets "TimeText" via SetText. We never touch
//     those text widgets; the click target is a separate transparent overlay button.

modded class COA_PreviewMenu
{
	// Set to true to dump COA's widget tree to the log (their layout is binary-only, so this
	// is how the real widget names/types are discovered). NOTE: this now runs from the
	// deferred build, i.e. AFTER the map exists - dumping from OnMenuOpen shows a tree
	// without the map widget and hides exactly the problem we care about.
	protected static const bool BC_DEBUG_DUMP_HIERARCHY = false;

	protected static const int BC_VISIBILITY_REFRESH_MS = 2000;

	// Small delay before building. NOT for z-order: a root-child-order dump proved our panel
	// already lands above MapFrame (map at [0], our panel at [3]) even when built synchronously.
	// The delay only lets COA's four-frame OpenMap chain settle before we query its widgets.
	protected static const int BC_DEFERRED_BUILD_MS = 500;

	// Delay between opening the editor and asking it for the attributes dialog. The editor
	// manager entity and its components are created as part of the open, so this cannot be
	// called inline. Tune upward if the attributes manager is reported unavailable.

	// Panel height must fit title + 5 map buttons + apply, with padding. A VerticalLayout CLIPS
	// children that overflow it, so an undersized panel silently swallows the last child
	// (ApplyButton) - it renders nothing and never appears in a hit test. Keep this generous.
	protected static const float BC_BUTTON_HEIGHT = 34;
	protected static const float BC_APPLY_HEIGHT = 38;
	protected static const float BC_PANEL_WIDTH = 300;
	protected static const float BC_PANEL_HEIGHT = 400;

	protected Widget m_wBCMapSwitchRoot;
	protected Widget m_wBCTimeButton;
	protected Widget m_wBCSpectatorButton;
	protected Widget m_wBCConfirmMenu;
	protected ref array<Widget> m_aBCMapButtons = {};
	protected int m_iBCSelectedMapIndex = -1;
	protected float m_fBCCursorLogTimer = 0;

	// Which index the button labels were last written for. Tracked separately from the selection
	// so BC_RefreshCurrentMapLabels can detect the mission header becoming available after the
	// panel was already built. -2 = never labelled (distinct from -1 = labelled as "no current").
	protected int m_iBCLabelledCurrentIndex = -2;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		// COA closes the menu immediately on dedicated servers; m_wRoot is then unusable.
		if (!m_wRoot)
			return;

		// Deferred: see BC_DEFERRED_BUILD_MS. Building here would put us under the map.
		GetGame().GetCallqueue().CallLater(BC_DeferredBuild, BC_DEFERRED_BUILD_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void BC_DeferredBuild()
	{
		if (!m_wRoot)
			return;

		if (BC_DEBUG_DUMP_HIERARCHY)
			BC_DumpWidgetTree(m_wRoot, 0);

		BC_BuildMapSwitchUI();
		BC_BuildTimeWeatherButton();
		BC_BuildSpectatorButton();

		// Logged after building so our widgets appear in the list - confirms whether they
		// actually landed on top of the map or are still buried under it.
		if (BC_DEBUG_DUMP_HIERARCHY)
			BC_DumpRootChildOrder();

		// Admin status can change mid-session (vote/promotion), and the gamemode can leave
		// BRIEFING while the menu is open. Poll on a timer rather than per-frame - COA already
		// does enough work in OnMenuUpdate.
		GetGame().GetCallqueue().CallLater(BC_RefreshMapSwitchVisibility, BC_VISIBILITY_REFRESH_MS, true);
		BC_RefreshMapSwitchVisibility();

		// One extra early check so a late mission header is picked up in ~1s rather than waiting
		// a full 2s timer cycle - the first briefing of a session is exactly when it is late.
		GetGame().GetCallqueue().CallLater(BC_RefreshMapSwitchVisibility, 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(BC_DeferredBuild);
		GetGame().GetCallqueue().Remove(BC_RefreshMapSwitchVisibility);

		// Deliberately NOT removing the scenario-attributes callback here. Opening the editor
		// makes COA's StartEvents(OPEN) close this very menu, so cancelling it on close would
		// cancel the thing the click just asked for - which is exactly why the attributes dialog
		// never appeared. It is a static call and does not touch this instance.

		BC_CloseConfirmMapSwitch();

		BC_ClearMapButtons();

		if (m_wBCMapSwitchRoot)
			m_wBCMapSwitchRoot.RemoveFromHierarchy();

		if (m_wBCTimeButton)
			m_wBCTimeButton.RemoveFromHierarchy();

		if (m_wBCSpectatorButton)
			m_wBCSpectatorButton.RemoveFromHierarchy();

		m_wBCMapSwitchRoot = null;
		m_wBCTimeButton = null;
		m_wBCSpectatorButton = null;
		m_iBCSelectedMapIndex = -1;
		m_iBCLabelledCurrentIndex = -2;

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	//! Only used to throttle the cursor diagnostic; no gameplay effect.
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		if (!BC_DEBUG_DUMP_HIERARCHY)
			return;

		m_fBCCursorLogTimer += tDelta;
		if (m_fBCCursorLogTimer < 1.0)
			return;

		m_fBCCursorLogTimer = 0;
		BC_DumpWidgetUnderCursor();
	}

	//------------------------------------------------------------------------------------------------
	protected void BC_ClearMapButtons()
	{
		foreach (Widget buttonWidget : m_aBCMapButtons)
		{
			if (!buttonWidget)
				continue;

			SCR_ButtonTextComponent buttonComponent = SCR_ButtonTextComponent.Cast(buttonWidget.FindHandler(SCR_ButtonTextComponent));
			if (buttonComponent)
				buttonComponent.m_OnClicked.Clear();
		}

		m_aBCMapButtons.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//	 MAP SWITCH UI
	//------------------------------------------------------------------------------------------------

	protected void BC_BuildMapSwitchUI()
	{
		if (m_wBCMapSwitchRoot)
			return;

		m_wBCMapSwitchRoot = GetGame().GetWorkspace().CreateWidgets(
			"{A1B2C3D4E5F60718}UI/Layouts/Preview/GRAD_BC_MapSwitch.layout", m_wRoot);

		if (!m_wBCMapSwitchRoot)
		{
			Print("BC Debug - PreviewMenu: failed to create map switch layout", LogLevel.ERROR);
			return;
		}

		// Parented straight to the menu root, which is reliably a FrameWidget, so the slot
		// type is known. Anchoring into COA's own header containers would look better but
		// their slot types are not visible to us (binary layout).
		FrameSlot.SetAnchorMin(m_wBCMapSwitchRoot, 0.0, 0.0);
		FrameSlot.SetAnchorMax(m_wBCMapSwitchRoot, 0.0, 0.0);
		FrameSlot.SetPos(m_wBCMapSwitchRoot, 40, 96);
		FrameSlot.SetSize(m_wBCMapSwitchRoot, BC_PANEL_WIDTH, BC_PANEL_HEIGHT);

		BC_PopulateMapButtons();

		// Geometry is logged on a delay, NOT inline: measured immediately after CreateWidgets every
		// child reports pos=(0,0) size=(0,0) - including buttons that visibly render - because the
		// engine has not run a layout pass yet. Reading it here produced pure noise.
		if (BC_DEBUG_DUMP_HIERARCHY)
			GetGame().GetCallqueue().CallLater(BC_LogPanelGeometry, 1000, false);

		// Same GetButtonBase lookup as the time/weather button - searches children rather than
		// relying on the handler sitting on the widget we happen to have a reference to.
		// GetButtonText (not GetButtonBase) so we get the text-capable component: its
		// HandlerAttached takes ownership of the child Text widget and clears whatever the
		// .layout authored, so "APPLY" has to be set through the component at runtime.
		SCR_ButtonTextComponent applyComponent = SCR_ButtonTextComponent.GetButtonText("ApplyButton", m_wBCMapSwitchRoot);
		if (applyComponent)
		{
			applyComponent.SetText("APPLY");
			applyComponent.m_OnClicked.Insert(BC_ConfirmMapSwitch);
			Print("BC Debug - PreviewMenu: ApplyButton wired up OK", LogLevel.NORMAL);
		}
		else
		{
			Print("BC Debug - PreviewMenu: ApplyButton component NOT FOUND - apply will not work", LogLevel.ERROR);
		}

		BC_UpdateApplyButtonState();
	}

	//------------------------------------------------------------------------------------------------
	//! One button per map, bound to a fixed set of widgets in the layout (MapButton0..MapButton4).
	//!
	//! Deliberately NOT SCR_ListBoxComponent: that handler binds to internal child widgets
	//! (scroll container, element holder) that a hand-authored layout does not provide, which is
	//! why the earlier listbox rendered as an empty box. With a fixed five maps a listbox buys
	//! nothing anyway.
	protected void BC_PopulateMapButtons()
	{
		BC_ClearMapButtons();

		int currentIndex = GRAD_BC_MapSwitch.GetCurrentMapIndex();

		for (int i = 0; i < GRAD_BC_MapSwitch.GetMapCount(); i++)
		{
			string widgetName = string.Format("MapButton%1", i);

			ButtonWidget mapButton = ButtonWidget.Cast(m_wBCMapSwitchRoot.FindAnyWidget(widgetName));
			if (!mapButton)
			{
				Print(string.Format("BC Debug - PreviewMenu: %1 missing from map switch layout", widgetName), LogLevel.ERROR);
				continue;
			}

			string label = GRAD_BC_MapSwitch.GetDisplayName(i);

			// Mark the map we are already running so an admin doesn't reload into the same one.
			if (i == currentIndex)
				label = label + " (current)";

			// The button carries its map index as its user ID, so one shared click handler can
			// recover which map was pressed without a closure per button.
			mapButton.SetUserID(i);

			// NOTE: deliberately no padding applied here. Adding LayoutSlot padding to the five
			// map buttons (and not to ApplyButton) grew each of them vertically and pushed Apply
			// out of the panel - it was found/visible/enabled in script but never drew.
			// Spacing belongs in the .layout file, applied uniformly to every child.

			SCR_ButtonTextComponent buttonComponent = SCR_ButtonTextComponent.Cast(mapButton.FindHandler(SCR_ButtonTextComponent));
			if (!buttonComponent)
			{
				Print(string.Format("BC Debug - PreviewMenu: SCR_ButtonTextComponent handler missing on %1", widgetName), LogLevel.ERROR);
				continue;
			}

			// Set the label through the component, not the child TextWidget directly: the
			// component owns that widget and overwrites it on HandlerAttached.
			buttonComponent.SetText(label);

			buttonComponent.m_OnClicked.Insert(BC_OnMapButtonClicked);

			m_aBCMapButtons.Insert(mapButton);
		}

		// Preselect the running map so the panel opens with something highlighted rather than
		// looking inert. Apply stays harmless in this state - switching to the current map is a
		// no-op reload, and BC_UpdateApplyButtonState still gates on a valid index.
		m_iBCSelectedMapIndex = currentIndex;
		BC_HighlightSelectedMapButton();

		// Remember what we labelled with, so BC_RefreshCurrentMapLabels can tell when the answer
		// changes and relabel. -1 here means "not resolved yet", which is the normal state on the
		// very first briefing if the mission header has not populated.
		m_iBCLabelledCurrentIndex = currentIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-applies the "(current)" suffix if GetCurrentMapIndex() starts returning a real answer
	//! after the panel was already built.
	//!
	//! Labels are written once at build time, ~500ms after the menu opens. On the first briefing
	//! of a session the mission header is not always populated by then, so every button got an
	//! unmarked label and nothing ever revisited it - which is why "(current)" only appeared after
	//! switching maps. Called from the existing 2s visibility timer, so no new timer is needed.
	protected void BC_RefreshCurrentMapLabels()
	{
		if (!m_wBCMapSwitchRoot)
			return;

		int currentIndex = GRAD_BC_MapSwitch.GetCurrentMapIndex();

		if (currentIndex == m_iBCLabelledCurrentIndex)
			return;

		m_iBCLabelledCurrentIndex = currentIndex;

		Print(string.Format("BC Debug - PreviewMenu: current map resolved to index %1, relabelling", currentIndex), LogLevel.NORMAL);

		foreach (Widget buttonWidget : m_aBCMapButtons)
		{
			if (!buttonWidget)
				continue;

			int buttonIndex = buttonWidget.GetUserID();

			SCR_ButtonTextComponent buttonComponent = SCR_ButtonTextComponent.Cast(buttonWidget.FindHandler(SCR_ButtonTextComponent));
			if (!buttonComponent)
				continue;

			string label = GRAD_BC_MapSwitch.GetDisplayName(buttonIndex);
			if (buttonIndex == currentIndex)
				label = label + " (current)";

			buttonComponent.SetText(label);
		}

		// Nothing was selected yet because there was no current map to preselect - do it now.
		if (m_iBCSelectedMapIndex < 0)
			m_iBCSelectedMapIndex = currentIndex;

		BC_HighlightSelectedMapButton();
		BC_UpdateApplyButtonState();
	}

	//------------------------------------------------------------------------------------------------
	//! Logs the real resolved geometry of the panel and each button. Layout arithmetic has been
	//! unreliable to reason about statically, so this reports what the engine actually computed:
	//! if ApplyButton's Y sits beyond the panel height, it is being clipped by the VerticalLayout.
	protected void BC_LogPanelGeometry()
	{
		// Guarded here as well as at the call site, so this stays silent even if something
		// else ever invokes it.
		if (!BC_DEBUG_DUMP_HIERARCHY)
			return;

		if (!m_wBCMapSwitchRoot)
			return;

		vector rootSize = FrameSlot.GetSize(m_wBCMapSwitchRoot);
		vector rootPos = FrameSlot.GetPos(m_wBCMapSwitchRoot);
		Print(string.Format("BC Debug - Geometry: panel root pos=(%1,%2) size=(%3,%4)", rootPos[0], rootPos[1], rootSize[0], rootSize[1]), LogLevel.NORMAL);

		// NOTE: only the panel root sits in a FrameWidgetSlot, so FrameSlot.GetSize is meaningful
		// for it alone. The buttons live in a VerticalLayout and always report (0,0) through
		// FrameSlot - that is a wrong-slot-type reading, not a real zero size. Their actual
		// height comes from SizeX/SizeY in the .layout file.
		Widget applyButton = m_wBCMapSwitchRoot.FindAnyWidget("ApplyButton");
		if (!applyButton)
		{
			Print("BC Debug - Geometry: ApplyButton widget NOT FOUND in panel", LogLevel.ERROR);
			return;
		}

		Print(string.Format("BC Debug - Geometry: ApplyButton found, visible=%1 enabled=%2 opacity=%3", applyButton.IsVisible(), applyButton.IsEnabled(), applyButton.GetOpacity()), LogLevel.NORMAL);

		// Walk the VerticalLayout's children in order and report each one. This is the only
		// reading that matters: whether ApplyButton is actually a sibling of the map buttons at
		// runtime, and in which position.
		//
		// (A TraceWidgets probe was tried here and was useless - it reported nothing at any Y even
		// while the engine's own GetWidgetUnderCursor was resolving MapButton0..4 at the same
		// instant, because the probe coordinates were in the wrong resolution space.)
		Widget panel = m_wBCMapSwitchRoot.FindAnyWidget("MapSwitchPanel");
		if (!panel)
		{
			Print("BC Debug - Geometry: MapSwitchPanel NOT FOUND", LogLevel.ERROR);
			return;
		}

		int childIndex = 0;
		Widget child = panel.GetChildren();

		while (child)
		{
			// GetScreenPos/GetScreenSize report where the engine ACTUALLY laid the child out,
			// which is the one number that separates "not rendered" from "rendered off-panel".
			float screenX, screenY, screenW, screenH;
			child.GetScreenPos(screenX, screenY);
			child.GetScreenSize(screenW, screenH);

			Print(string.Format("BC Debug - PanelChild[%1]: %2 pos=(%3,%4) size=(%5,%6) visible=%7",
				childIndex, child.GetName(), screenX, screenY, screenW, screenH, child.IsVisible()), LogLevel.NORMAL);

			childIndex++;
			child = child.GetSibling();
		}

		Print(string.Format("BC Debug - Geometry: panel has %1 children total", childIndex), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	protected void BC_OnMapButtonClicked(SCR_ButtonBaseComponent buttonComponent)
	{
		if (!buttonComponent)
			return;

		Widget buttonWidget = buttonComponent.GetRootWidget();
		if (!buttonWidget)
			return;

		m_iBCSelectedMapIndex = buttonWidget.GetUserID();

		Print(string.Format("BC Debug - PreviewMenu: selected map index %1", m_iBCSelectedMapIndex), LogLevel.NORMAL);

		BC_HighlightSelectedMapButton();
		BC_UpdateApplyButtonState();
	}

	//------------------------------------------------------------------------------------------------
	//! Three visual states, so "the map we are on" and "the map about to be switched to" are not
	//! confusable: the running map is muted red, the pending selection is bright red, everything
	//! else is the default dark.
	protected void BC_HighlightSelectedMapButton()
	{
		int currentIndex = GRAD_BC_MapSwitch.GetCurrentMapIndex();

		foreach (Widget buttonWidget : m_aBCMapButtons)
		{
			if (!buttonWidget)
				continue;

			int buttonIndex = buttonWidget.GetUserID();

			if (buttonIndex == m_iBCSelectedMapIndex && buttonIndex != currentIndex)
			{
				// Pending switch target.
				buttonWidget.SetColor(Color.FromRGBA(196, 36, 36, 255));
				continue;
			}

			if (buttonIndex == currentIndex)
			{
				// The map already running - highlighted, but muted so it does not read as a
				// pending action.
				buttonWidget.SetColor(Color.FromRGBA(92, 40, 40, 235));
				continue;
			}

			buttonWidget.SetColor(Color.FromRGBA(13, 13, 13, 200));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Selection alone never switches - the admin must then press Apply and confirm.
	//!
	//! The button stays ENABLED at all times and validates on click instead. Disabling it via
	//! SetEnabled(false) at build time (when nothing is selected yet) left it permanently
	//! unresponsive - a disabled ButtonWidget stops routing clicks to its handler, and
	//! re-enabling it later did not restore that. Greying is done with opacity instead.
	protected void BC_UpdateApplyButtonState()
	{
		if (!m_wBCMapSwitchRoot)
			return;

		ButtonWidget applyButton = ButtonWidget.Cast(m_wBCMapSwitchRoot.FindAnyWidget("ApplyButton"));
		if (!applyButton)
		{
			Print("BC Debug - PreviewMenu: ApplyButton widget not found", LogLevel.ERROR);
			return;
		}

		applyButton.SetVisible(true);

		// Dim when the selection is the map already running - applying would be a pointless
		// full reload. Opacity only, never SetEnabled(false): a disabled ButtonWidget stops
		// routing clicks to its handler and does not recover when re-enabled.
		if (m_iBCSelectedMapIndex == GRAD_BC_MapSwitch.GetCurrentMapIndex())
			applyButton.SetOpacity(0.45);
		else
			applyButton.SetOpacity(1.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Show/hide the dropdown. Admin-only, and BRIEFING phase only: switching reloads the whole
	//! session, so it must not be reachable once a round is running.
	protected void BC_RefreshMapSwitchVisibility()
	{
		// NOTE: do not early-return on a missing m_wBCMapSwitchRoot - the time/weather and
		// spectator buttons are built independently and must still be refreshed if the map
		// switch panel failed to build.
		if (m_wBCMapSwitchRoot)
		{
			// Piggyback on this timer: the mission header may only become available after the
			// panel was built, in which case the "(current)" marker still needs applying.
			BC_RefreshCurrentMapLabels();

			bool allowed = GRAD_BC_MapSwitch.CanLocalPlayerSwitchMap();

			if (allowed && m_Gamemode && m_Gamemode.m_GamemodeState != COA_EGamemodeState.BRIEFING)
				allowed = false;

			m_wBCMapSwitchRoot.SetVisible(allowed);
		}

		if (m_wBCTimeButton)
			m_wBCTimeButton.SetVisible(GRAD_BC_MapSwitch.CanLocalPlayerSwitchMap());

		// Admin-only. No phase restriction: this menu IS COA's briefing screen, so gating on
		// "not BRIEFING" hid the button in the only phase the menu is ever open.
		if (m_wBCSpectatorButton)
			m_wBCSpectatorButton.SetVisible(GRAD_BC_MapSwitch.CanLocalPlayerSwitchMap());
	}

	//------------------------------------------------------------------------------------------------
	//	 CONFIRMATION (reuses COA's own ConfirmationMenu.layout, as COA's EditorMenuUI does)
	//------------------------------------------------------------------------------------------------

	//! Parameterless, matching how COA's own EditorMenuUI binds ToggleListen / CleanUpBodies
	//! to m_OnClicked - ScriptInvoker tolerates a listener taking fewer args than it passes.
	protected void BC_ConfirmMapSwitch()
	{
		Print(string.Format("BC Debug - PreviewMenu: ApplyButton CLICKED (selected index %1)", m_iBCSelectedMapIndex), LogLevel.NORMAL);

		if (m_iBCSelectedMapIndex < 0 || m_iBCSelectedMapIndex >= GRAD_BC_MapSwitch.GetMapCount())
		{
			Print("BC Debug - PreviewMenu: no map selected yet, ignoring Apply", LogLevel.WARNING);
			return;
		}

		// Switching to the map already running would kick everyone through a full reload for no
		// reason, so refuse it rather than confirming it.
		if (m_iBCSelectedMapIndex == GRAD_BC_MapSwitch.GetCurrentMapIndex())
		{
			Print("BC Debug - PreviewMenu: selected map is already running, ignoring Apply", LogLevel.WARNING);
			return;
		}

		BC_CloseConfirmMapSwitch();

		m_wBCConfirmMenu = GetGame().GetWorkspace().CreateWidgets(
			"{905BF1B70A9A44AC}UI/layouts/Menus/PauseMenu/AdminMenuWidgets/ConfirmationMenu.layout");

		if (!m_wBCConfirmMenu)
		{
			Print("BC Debug - PreviewMenu: failed to create confirmation menu", LogLevel.ERROR);
			return;
		}

		// "ExcuteButton" is COA's own spelling in ConfirmationMenu.layout - matched verbatim.
		SCR_ButtonTextComponent runButton = SCR_ButtonTextComponent.Cast(
			m_wBCConfirmMenu.FindAnyWidget("ExcuteButton").FindHandler(SCR_ButtonTextComponent));
		SCR_ButtonTextComponent cancelButton = SCR_ButtonTextComponent.Cast(
			m_wBCConfirmMenu.FindAnyWidget("CancelButton").FindHandler(SCR_ButtonTextComponent));

		if (cancelButton)
			cancelButton.m_OnClicked.Insert(BC_CloseConfirmMapSwitch);

		if (runButton)
			runButton.m_OnClicked.Insert(BC_SendMapSwitchRequest);
	}

	//------------------------------------------------------------------------------------------------
	protected void BC_CloseConfirmMapSwitch()
	{
		if (m_wBCConfirmMenu)
		{
			delete m_wBCConfirmMenu;
			m_wBCConfirmMenu = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BC_SendMapSwitchRequest()
	{
		BC_CloseConfirmMapSwitch();

		if (m_iBCSelectedMapIndex < 0 || m_iBCSelectedMapIndex >= GRAD_BC_MapSwitch.GetMapCount())
			return;

		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
		if (!playerComponent)
		{
			Print("BC Debug - PreviewMenu: GRAD_PlayerComponent unavailable, cannot request map switch", LogLevel.ERROR);
			return;
		}

		Print(string.Format("BC Debug - PreviewMenu: requesting scenario change to index %1", m_iBCSelectedMapIndex), LogLevel.NORMAL);
		playerComponent.Ask_ChangeScenario(m_iBCSelectedMapIndex);
	}

	//------------------------------------------------------------------------------------------------
	//	 TIME / WEATHER -> GAME MASTER
	//------------------------------------------------------------------------------------------------

	//! COA's TimeText/WeatherText are TextWidgets and cannot receive clicks, and OnMenuUpdate
	//! rewrites TimeText every frame. So we overlay a transparent button rather than replacing
	//! or re-parenting anything COA owns.
	protected void BC_BuildTimeWeatherButton()
	{
		if (m_wBCTimeButton)
			return;

		// Parented to the MENU ROOT, not to COA's "TimeWeather" container. Our layout's root uses
		// a FrameWidgetSlot, which only means anything inside a FrameWidget - parenting into
		// COA's container produced a widget with no resolved size that never appeared in a
		// hit test. The menu root is reliably a FrameWidget, so position explicitly here.
		m_wBCTimeButton = GetGame().GetWorkspace().CreateWidgets(
			"{B2C3D4E5F6071829}UI/Layouts/Preview/GRAD_BC_TimeWeatherButton.layout", m_wRoot);

		if (!m_wBCTimeButton)
		{
			Print("BC Debug - PreviewMenu: failed to create time/weather button layout", LogLevel.ERROR);
			return;
		}

		FrameSlot.SetAnchorMin(m_wBCTimeButton, 1.0, 0.0);
		FrameSlot.SetAnchorMax(m_wBCTimeButton, 1.0, 0.0);
		FrameSlot.SetPos(m_wBCTimeButton, -260, 100);
		FrameSlot.SetSize(m_wBCTimeButton, 240, 90);

		// GetButtonBase searches CHILDREN (searchAllChildren defaults true). FindHandler on the
		// layout root does not - the handler lives on the child ButtonWidget, not the FrameWidget
		// root, so FindHandler returned null here and the click was never wired up.
		SCR_ButtonBaseComponent buttonComponent = SCR_ButtonBaseComponent.GetButtonBase("TimeWeatherButton", m_wBCTimeButton);

		if (!buttonComponent)
		{
			Print("BC Debug - PreviewMenu: SCR_ButtonBaseComponent NOT FOUND on time/weather button - click will not work", LogLevel.ERROR);
			return;
		}

		buttonComponent.m_OnClicked.Insert(BC_OnTimeWeatherClicked);
		Print("BC Debug - PreviewMenu: time/weather button wired up OK", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//	 ENTER SPECTATOR (admin only)
	//------------------------------------------------------------------------------------------------

	//! Sits directly under the time/weather button and reuses the same layout - only the label
	//! and click handler differ. See BC_BuildTimeWeatherButton for why this is parented to the
	//! menu root and positioned explicitly rather than into a COA-owned container.
	protected void BC_BuildSpectatorButton()
	{
		if (m_wBCSpectatorButton)
			return;

		m_wBCSpectatorButton = GetGame().GetWorkspace().CreateWidgets(
			"{B2C3D4E5F6071829}UI/Layouts/Preview/GRAD_BC_TimeWeatherButton.layout", m_wRoot);

		if (!m_wBCSpectatorButton)
		{
			Print("BC Debug - PreviewMenu: failed to create spectator button layout", LogLevel.ERROR);
			return;
		}

		FrameSlot.SetAnchorMin(m_wBCSpectatorButton, 1.0, 0.0);
		FrameSlot.SetAnchorMax(m_wBCSpectatorButton, 1.0, 0.0);
		FrameSlot.SetPos(m_wBCSpectatorButton, -260, 200);
		FrameSlot.SetSize(m_wBCSpectatorButton, 240, 90);

		// The shared layout ships with an empty label; the time/weather button draws COA's own
		// text underneath it, but this one has nothing behind it, so give it a visible caption.
		TextWidget label = TextWidget.Cast(m_wBCSpectatorButton.FindAnyWidget("Text"));
		if (label)
			label.SetText("SPECTATE");

		SCR_ButtonBaseComponent buttonComponent = SCR_ButtonBaseComponent.GetButtonBase("TimeWeatherButton", m_wBCSpectatorButton);

		if (!buttonComponent)
		{
			Print("BC Debug - PreviewMenu: SCR_ButtonBaseComponent NOT FOUND on spectator button - click will not work", LogLevel.ERROR);
			return;
		}

		buttonComponent.m_OnClicked.Insert(BC_OnSpectatorClicked);
		Print("BC Debug - PreviewMenu: spectator button wired up OK", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void BC_OnSpectatorClicked()
	{
		Print("BC Debug - PreviewMenu: spectator button CLICKED", LogLevel.NORMAL);

		// COA's own combined spectator/moderator/admin gate, same as the Game Master button.
		// The server re-checks admin status in RpcAsk_Server_EnterSpectator - this is only
		// here so non-admins never see a button that would be rejected anyway.
		if (!SCR_EditorManagerEntity.COA_HasUnlimitedEditorAccess())
		{
			Print("BC Debug - PreviewMenu: COA_HasUnlimitedEditorAccess() == false, spectator blocked", LogLevel.WARNING);
			return;
		}

		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.Cast(playerController.FindComponent(GRAD_PlayerComponent));
		if (!playerComponent)
		{
			Print("BC Debug - PreviewMenu: GRAD_PlayerComponent not found, cannot enter spectator", LogLevel.ERROR);
			return;
		}

		Print("BC Debug - PreviewMenu: requesting spectator switch", LogLevel.NORMAL);
		playerComponent.Ask_EnterSpectator();

		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void BC_OnTimeWeatherClicked()
	{
		Print("BC Debug - PreviewMenu: time/weather box CLICKED", LogLevel.NORMAL);

		// COA's own combined spectator/moderator/admin gate.
		if (!SCR_EditorManagerEntity.COA_HasUnlimitedEditorAccess())
		{
			Print("BC Debug - PreviewMenu: COA_HasUnlimitedEditorAccess() == false, blocked", LogLevel.WARNING);
			return;
		}

		// Routes through COA's modded CanOpen(), so their limited/unlimited rules apply here too.
		if (!SCR_EditorManagerEntity.CanOpenInstance())
		{
			Print("BC Debug - PreviewMenu: CanOpenInstance() == false, editor cannot be opened right now", LogLevel.WARNING);
			return;
		}

		Print("BC Debug - PreviewMenu: opening Game Master from time/weather box", LogLevel.NORMAL);

		// OpenInstance, not ToggleInstance: a stray double-click on a toggle closes it again.
		// COA's SCR_EditorManagerEntity.StartEvents(OPEN) closes this menu, and OpenUI() reopens
		// it on close - our OnMenuOpen then rebuilds these widgets, so the round trip self-heals.
		SCR_EditorManagerEntity.OpenInstance();

		// The admin presses V once inside to open scenario properties. Two ways to do that step
		// automatically were tried and BOTH failed - do not spend time on them again:
		//
		//   1. SCR_AttributesManagerEditorComponent.StartEditing(entity). The dialog needs a
		//      scenario/world entity, but enumerating all 83-89 registered editables showed only
		//      GROUP/SYSTEM/VEHICLE/CHARACTER/COMMENT/FACTION - no scenario entity exists to pass.
		//      (Matching the name "#AR-AttributesDialog_TitlePage_Entity_Text" looks promising but
		//      is the engine's PLACEHOLDER name for an unnamed entity; it resolved to a CHARACTER.)
		//
		//   2. InputManager.ActivateAction("EditorAttributes") - the V keybind. The action name is
		//      correct and ActivateAction returns true, even when deferred until
		//      IsOpenedInstance() reports the editor open. The editor still never acts on it:
		//      accepting a synthetic action is not the same as consuming it.

	}


	//------------------------------------------------------------------------------------------------



	//------------------------------------------------------------------------------------------------
	//	 DEBUG
	//------------------------------------------------------------------------------------------------

	//! Recursively logs COA's widget tree. Their layout is binary-only, so this is the only way
	//! to discover widget names and slot types. Toggle BC_DEBUG_DUMP_HIERARCHY to enable.
	//!
	//! Walks siblings at EVERY depth including the root's direct children - sibling ORDER is
	//! the thing that matters here, since later siblings draw on top and take the cursor first.
	//! Run this from BC_DeferredBuild (after the map exists), never from OnMenuOpen.
	protected void BC_DumpWidgetTree(Widget widget, int depth)
	{
		if (!widget)
			return;

		string indent = string.Empty;
		for (int i = 0; i < depth; i++)
		{
			indent = indent + "  ";
		}

		Print(string.Format("BC Debug - Widget: %1%2 [%3]", indent, widget.GetName(), widget.Type().ToString()), LogLevel.NORMAL);

		BC_DumpWidgetTree(widget.GetChildren(), depth + 1);
		BC_DumpWidgetTree(widget.GetSibling(), depth);
	}

	//------------------------------------------------------------------------------------------------
	//! Reports which widget is actually under the mouse right now. If our button is wired up but
	//! never fires, run this while hovering it: whatever this prints is the widget eating the click.
	//! Called from OnMenuUpdate on a throttle while BC_DEBUG_DUMP_HIERARCHY is on.
	protected void BC_DumpWidgetUnderCursor()
	{
		Widget hovered = WidgetManager.GetWidgetUnderCursor();

		if (!hovered)
		{
			Print("BC Debug - Cursor: (nothing under cursor)", LogLevel.NORMAL);
			return;
		}

		// Walk up a few parents so we can tell WHOSE subtree the hovered widget belongs to.
		string chain = hovered.GetName();
		Widget parent = hovered.GetParent();
		int depth = 0;

		while (parent && depth < 4)
		{
			chain = chain + " < " + parent.GetName();
			parent = parent.GetParent();
			depth++;
		}

		Print(string.Format("BC Debug - Cursor over: %1", chain), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Lists the menu root's direct children in order. The LAST entry is the one that wins the
	//! cursor, so if our panel is not at/near the end, the map is still on top of it.
	protected void BC_DumpRootChildOrder()
	{
		if (!m_wRoot)
			return;

		Print("BC Debug - Root child order (last = topmost, wins clicks):", LogLevel.NORMAL);

		int index = 0;
		Widget child = m_wRoot.GetChildren();

		while (child)
		{
			Print(string.Format("BC Debug -   [%1] %2 [%3]", index, child.GetName(), child.Type().ToString()), LogLevel.NORMAL);
			index++;
			child = child.GetSibling();
		}
	}
}
