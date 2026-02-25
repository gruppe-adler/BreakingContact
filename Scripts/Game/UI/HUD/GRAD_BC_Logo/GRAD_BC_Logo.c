
class GRAD_BC_Logo: SCR_InfoDisplayExtended
{
	private ImageWidget m_logo;
	private bool m_bLogoRequested = false;	// only show when explicitly requested via ShowLogo()

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

		// Hide at widget level immediately - before any base class Show() calls can make it visible
		m_wRoot.SetVisible(false);
		m_wRoot.SetOpacity(0);
	}

	override void Show(bool show, float speed = UIConstants.FADE_RATE_INSTANT, EAnimationCurve curve = EAnimationCurve.LINEAR) {
		// Block the base class from showing the logo unless explicitly requested via ShowLogo()
		if (show && !m_bLogoRequested)
			return;
		super.Show(show, speed, curve);
	}

	void ShowLogo()
    {
		m_bLogoRequested = true;
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Logo: showLogo called!", LogLevel.VERBOSE);
		GetGame().GetCallqueue().Remove(HideLogo);
    	GetGame().GetCallqueue().CallLater(HideLogo, 15000, false, false);
    }

	private void HideLogo(bool firstHide)
    {
		m_bLogoRequested = false;
        float duration = 3.0;
		if (firstHide) { duration = 0.0 };
		super.Show(false, duration, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Logo: hiding m_logo", LogLevel.VERBOSE);
    }
}
