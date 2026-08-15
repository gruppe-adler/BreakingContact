
class GRAD_BC_Logo: SCR_InfoDisplayExtended
{
	private ImageWidget m_logo;
	private bool m_bPendingShow;

	override event void DisplayInit(IEntity owner) {
		super.DisplayInit(owner);
		
		m_wRoot = GetGame().GetWorkspace().CreateWidgets("{9A969E139E27E0F5}UI/Layouts/HUD/GRAD_BC_Logo/GRAD_BC_Logo.layout", null);

		if (!m_wRoot) {
			PrintFormat("GRAD_BC_Logo: no m_wRoot found", LogLevel.ERROR);
			return;
		}
		
		Widget w = m_wRoot.FindAnyWidget("GRAD_BC_Logo_Widget");
   		m_logo = ImageWidget.Cast(w);
		
		if (!m_logo) {
			PrintFormat("GRAD_BC_Logo: no GRAD_BC_Logo_Widget found", LogLevel.ERROR);
			return;
		}
		
		// Start hidden instantly — Show(false, speed) would re-show then fade, so use SetVisible directly
		m_wRoot.SetVisible(false);
		super.Show(false, UIConstants.FADE_RATE_INSTANT);

		// Subscribe to map close so we can show the logo when the player actually sees the world
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}

	override event void DisplayStopDraw(IEntity owner) {
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
		GetGame().GetCallqueue().Remove(HideLogo);
		GetGame().GetCallqueue().Remove(PendingShowTimeout);
		m_bPendingShow = false;
		super.DisplayStopDraw(owner);
	}

	//! Call this on every phase change (including JIP catch-up). The logo is purely phase-driven:
	//! visible for LOADING/PREPTIME/OPFOR/BLUFOR, hidden for GAME (it obstructs gameplay) and
	//! GAMEOVER. If the map is currently open, showing is deferred until the player closes it so
	//! the logo doesn't appear behind the map.
	void SetPhaseVisibility(EBreakingContactPhase phase)
	{
		bool shouldShow = (phase != EBreakingContactPhase.GAME && phase != EBreakingContactPhase.GAMEOVER && phase != EBreakingContactPhase.GAMEOVERDONE);

		if (!shouldShow)
		{
			m_bPendingShow = false;
			GetGame().GetCallqueue().Remove(PendingShowTimeout);
			HideLogo();
			return;
		}

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
		{
			// Map is open — defer display until the player actually closes it.
			m_bPendingShow = true;

			// JIP SAFETY NET. The deferred path relies on OnMapClose() firing, but for a
			// join-in-progress player the map can close before DisplayInit() has subscribed to
			// GetOnMapClose(), or the phase sync can arrive after the map already closed. In
			// either case OnMapClose() never runs and m_bPendingShow stays true forever.
			//
			// So arm an unconditional fallback: if the map has not closed within 15s, show the
			// logo anyway. Cancelled in OnMapClose().
			GetGame().GetCallqueue().Remove(PendingShowTimeout);
			GetGame().GetCallqueue().CallLater(PendingShowTimeout, 15000);

			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_Logo: map is open, deferring logo until map close", LogLevel.VERBOSE);
		}
		else
		{
			// Map is closed — show the logo right away
			ShowLogo();
		}
	}

	protected void OnMapClose(MapConfiguration config)
	{
		if (!m_bPendingShow)
			return;
		m_bPendingShow = false;
		GetGame().GetCallqueue().Remove(PendingShowTimeout);
		ShowLogo();
	}

	//! Fires when a deferred show never got its OnMapClose(). Shows the logo now.
	private void PendingShowTimeout()
	{
		if (!m_bPendingShow)
			return;

		m_bPendingShow = false;

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_Logo: pending show timed out (no OnMapClose), showing now", LogLevel.WARNING);

		ShowLogo();
	}

	void ShowLogo()
    {
		m_bPendingShow = false;
		GetGame().GetCallqueue().Remove(PendingShowTimeout);
		GetGame().GetCallqueue().Remove(HideLogo);

		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Logo: showLogo called!", LogLevel.VERBOSE);
    }

	//! Cancel any pending/scheduled show so an external hide (e.g. HideUIForSpectators) is not
	//! undone a moment later by a deferred OnMapClose or the pending-show timeout.
	void CancelPendingShow()
	{
		m_bPendingShow = false;
		GetGame().GetCallqueue().Remove(PendingShowTimeout);
	}

	private void HideLogo()
    {
		GetGame().GetCallqueue().Remove(HideLogo);
		super.Show(false, 0.5, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Logo: hiding m_logo", LogLevel.VERBOSE);
    }
}
