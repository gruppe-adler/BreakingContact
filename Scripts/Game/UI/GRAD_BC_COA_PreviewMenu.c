// Breaking Contact additions to COALITION's briefing screen (COA_PreviewMenu):
//
//   1. A transparent button overlaid on COA's Time/Weather box that opens the
//      Game Master editor for admins.
//   2. A SPECTATE button beneath it, for admins.
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

	// Small delay before building, so COA's four-frame OpenMap chain settles before we query
	// its widgets.
	protected static const int BC_DEFERRED_BUILD_MS = 500;

	protected Widget m_wBCTimeButton;
	protected Widget m_wBCSpectatorButton;
	protected float m_fBCCursorLogTimer = 0;

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

		BC_BuildTimeWeatherButton();
		BC_BuildSpectatorButton();

		// Logged after building so our widgets appear in the list - confirms whether they
		// actually landed on top of the map or are still buried under it.
		if (BC_DEBUG_DUMP_HIERARCHY)
			BC_DumpRootChildOrder();

		// Admin status can change mid-session (vote/promotion), so poll on a timer rather than
		// per-frame - COA already does enough work in OnMenuUpdate.
		GetGame().GetCallqueue().CallLater(BC_RefreshButtonVisibility, BC_VISIBILITY_REFRESH_MS, true);
		BC_RefreshButtonVisibility();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(BC_DeferredBuild);
		GetGame().GetCallqueue().Remove(BC_RefreshButtonVisibility);

		// Deliberately NOT removing the scenario-attributes callback here. Opening the editor
		// makes COA's StartEvents(OPEN) close this very menu, so cancelling it on close would
		// cancel the thing the click just asked for - which is exactly why the attributes dialog
		// never appeared. It is a static call and does not touch this instance.

		if (m_wBCTimeButton)
			m_wBCTimeButton.RemoveFromHierarchy();

		if (m_wBCSpectatorButton)
			m_wBCSpectatorButton.RemoveFromHierarchy();

		m_wBCTimeButton = null;
		m_wBCSpectatorButton = null;

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
	//! Show/hide the admin-only buttons. Admin status can change mid-session, so this is polled.
	protected void BC_RefreshButtonVisibility()
	{
		// CanLocalPlayerSwitchMap() is just the admin check - it long predates the map switch
		// panel being removed, and is still the gate these two buttons use.
		bool isAdmin = GRAD_BC_MapSwitch.CanLocalPlayerSwitchMap();

		if (m_wBCTimeButton)
			m_wBCTimeButton.SetVisible(isAdmin);

		// No phase restriction: this menu IS COA's briefing screen, so gating on "not BRIEFING"
		// hid the button in the only phase the menu is ever open.
		if (m_wBCSpectatorButton)
			m_wBCSpectatorButton.SetVisible(isAdmin);
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
