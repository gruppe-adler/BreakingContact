//------------------------------------------------------------------------------------------------
// Stops SCR_MapCursorModule throwing every frame while the replay map is open.
//
// A single replay produced 4300+ "NULL pointer to instance" exceptions from this module, split
// between UpdateCrosshairUI() and GetCursorPosition(). Both are called every frame from Update(),
// and both dereference widgets without checking them.
//
// The crosshair widgets themselves are NOT the problem: COALITION's SpectatorHUD MapFrame inherits
// {0651202E9F2646DE}UI/layouts/Map/Map.layout, the same layout the normal map menu uses, so
// "CursorCrosshair" resolves normally.
//
// What the two failing functions have in common is m_MapWidget, taken from
// m_MapEntity.GetMapWidget(). The replay opens the map anchored to the spectator menu's own frame
// rather than through the standard map menu flow, and that is the difference between this map and
// the briefing map, which throws only a handful of times.
//
// Rather than guess further, each guard logs once naming what was actually null, so a single run
// identifies the culprit. The guards are worth keeping regardless: skipping a frame of cursor
// visuals is harmless, while an exception per frame is not.
//------------------------------------------------------------------------------------------------
modded class SCR_MapCursorModule
{
	// One-shot logging, so a null that persists for a whole replay reports once rather than
	// replacing one flood with another.
	protected bool m_bBCLoggedCrosshairNull = false;
	protected bool m_bBCLoggedCursorPosNull = false;

	//------------------------------------------------------------------------------------------------
	//! Widgets under the cursor, used by the HUD's available-actions system.
	//!
	//! The base implementation guards on IsOpen() but then calls GetMapMenuRoot(), which throws on a
	//! null m_wMapRoot. A map opened into a custom frame reports IsOpen() == true while having no
	//! standard map menu root, so the existing guard does not cover it - the exact case the replay
	//! map hits, 105 times in one session.
	static override array<Widget> GetMapWidgetsUnderCursor()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen() || !mapEntity.GetMapMenuRoot())
		{
			s_aTracedWidgets.Clear();
			return s_aTracedWidgets;
		}

		return super.GetMapWidgetsUnderCursor();
	}

	//------------------------------------------------------------------------------------------------
	//! Called every frame from Update(). Skipped when anything it needs is missing.
	override protected void UpdateCrosshairUI()
	{
		if (!m_MapWidget)
		{
			if (!m_bBCLoggedCrosshairNull)
			{
				m_bBCLoggedCrosshairNull = true;
				Print("BC: map cursor crosshair skipped - m_MapWidget is null", LogLevel.WARNING);
			}
			return;
		}

		// m_wCrossMCenter is the first thing the base implementation dereferences, and the rest of
		// the crosshair widgets come from the same lookup, so this one check covers them all.
		if (!m_wCrossMCenter)
		{
			if (!m_bBCLoggedCrosshairNull)
			{
				m_bBCLoggedCrosshairNull = true;
				Print("BC: map cursor crosshair skipped - CursorCrosshair widgets not found in this layout", LogLevel.WARNING);
			}
			return;
		}

		super.UpdateCrosshairUI();
	}

	//------------------------------------------------------------------------------------------------
	//! Edge panning drives m_MapEntity.Pan(), which fails downstream in SCR_MapEntity.UpdateViewPort
	//! and FitPanBounds - together 2514 of the 4318 exceptions in one replay, all rooted here rather
	//! than being separate bugs.
	//!
	//! The edge flags that trigger it come from TestEdgePan, which reads m_MapWidget. With no valid
	//! map widget the flags are meaningless, so panning is skipped rather than issuing pans the map
	//! entity cannot service.
	override protected void HandlePan(float timeSlice)
	{
		if (!m_MapWidget)
			return;

		super.HandlePan(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! Also called every frame. Returns the cursor unchanged when the map widget is unavailable,
	//! which leaves the cursor where it was rather than throwing.
	override protected void GetCursorPosition(out int x, out int y)
	{
		if (!m_MapWidget && m_MapEntity)
			m_MapWidget = m_MapEntity.GetMapWidget();

		if (!m_MapWidget)
		{
			if (!m_bBCLoggedCursorPosNull)
			{
				m_bBCLoggedCursorPosNull = true;
				Print("BC: map cursor position skipped - m_MapWidget is null", LogLevel.WARNING);
			}
			return;
		}

		super.GetCursorPosition(x, y);
	}
}
