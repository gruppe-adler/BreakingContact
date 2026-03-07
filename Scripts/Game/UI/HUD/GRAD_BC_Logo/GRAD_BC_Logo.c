
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
		
		// Ensure logo starts hidden immediately (no animation) so spectators never see it
		m_wRoot.SetVisible(false);
		HideLogo();

		// Subscribe to map close so we can show the logo when the player actually sees the world
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}

	override event void DisplayDestroy(IEntity owner) {
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
		super.DisplayDestroy(owner);
	}

	// Call this instead of ShowLogo() directly.
	// If the map is currently open the logo will be shown 1 second after the player closes it;
	// if the map is already closed (JIP, OPFOR at GAME phase, etc.) it shows immediately.
	void RequestShowLogo()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
		{
			// Map is open — defer display until the player actually closes it
			m_bPendingShow = true;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_Logo: map is open, deferring logo until map close", LogLevel.VERBOSE);
		}
		else
		{
			// Map is already closed — show the logo right away
			ShowLogo();
		}
	}

	protected void OnMapClose(MapConfiguration config)
	{
		if (!m_bPendingShow)
			return;
		m_bPendingShow = false;
		ShowLogo();
	}

	void ShowLogo()
    {
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Logo: showLogo called!", LogLevel.VERBOSE);
		GetGame().GetCallqueue().Remove(HideLogo);
    	GetGame().GetCallqueue().CallLater(HideLogo, 1000);
    }
	
	private void HideLogo()
    {
        // if (m_logo) { m_logo.SetOpacity(0.0); };
		super.Show(false, 3.0, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Logo: hiding m_logo", LogLevel.VERBOSE);
    }
}
