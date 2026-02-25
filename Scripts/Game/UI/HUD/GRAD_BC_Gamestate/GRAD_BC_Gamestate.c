class GRAD_BC_Gamestate: SCR_InfoDisplayExtended
{
	private RichTextWidget m_text;
	private bool m_bPersistent = false;
	private bool m_bTextRequested = false;	// only show when explicitly requested via ShowText/ShowPersistentText()

	// Progress bar widgets (defined in layout)
	private Widget m_progressContainer;
	private ImageWidget m_progressFill;

	override event void DisplayInit(IEntity owner) {
		super.DisplayInit(owner);

		m_wRoot = GetGame().GetWorkspace().CreateWidgets("{A36BC6B57CB2DF15}UI/Layouts/HUD/GRAD_BC_Gamestate/GRAD_BC_Gamestate.layout", null);

		if (!m_wRoot) {
			PrintFormat("GRAD_BC_Gamestate: no m_wRoot found", LogLevel.ERROR);
			return;
		}

		Widget w = m_wRoot.FindAnyWidget("GRAD_BC_Gamestate_text");
   		m_text = RichTextWidget.Cast(w);

		if (!m_text) {
			PrintFormat("GRAD_BC_Gamestate: no m_text found", LogLevel.ERROR);
			return;
		}
		
		m_text.SetText("");

		m_progressContainer = m_wRoot.FindAnyWidget("ProgressContainer");
		m_progressFill = ImageWidget.Cast(m_wRoot.FindAnyWidget("ProgressFill"));

		m_wRoot.SetVisible(false);
		m_wRoot.SetOpacity(0);
	}

	override void Show(bool show, float speed = UIConstants.FADE_RATE_INSTANT, EAnimationCurve curve = EAnimationCurve.LINEAR) {
		if (show && !m_bTextRequested)
			return;
		super.Show(show, speed, curve);
	}

	void ShowText(string message)
    {
		m_bPersistent = false;
		m_bTextRequested = true;
		m_text.SetText(message);
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Gamestate: showText called!", LogLevel.VERBOSE);
		GetGame().GetCallqueue().Remove(HideLogo);
    	GetGame().GetCallqueue().CallLater(HideLogo, 15000);
    }

	// Show text that stays visible until explicitly hidden
	void ShowPersistentText(string message)
	{
		m_bPersistent = true;
		m_bTextRequested = true;
		if (m_text)
			m_text.SetText(message);
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_Gamestate: ShowPersistentText: %1", message), LogLevel.NORMAL);
	}

	// Update text without changing visibility or resetting timers
	void UpdateText(string message)
	{
		if (!m_text)
			return;
		m_text.SetText(message);
	}

	// Show and update the progress bar
	void UpdateProgress(float progress)
	{
		if (!m_wRoot)
			return;

		if (m_progressContainer)
			m_progressContainer.SetVisible(true);

		if (m_progressFill)
		{
			// Adjust right anchor to represent progress (left anchor stays at 0.02)
			float rightAnchor = Math.Clamp(0.02 + (0.96 * progress), 0.02, 0.98);
			FrameSlot.SetAnchorMax(m_progressFill, rightAnchor, 0.9);
		}

		// Update text with percentage
		int percentage = Math.Round(progress * 100);
		if (m_text)
			m_text.SetText(string.Format("Replay loading... %1%%", percentage));
	}

	// Explicitly hide the gamestate display and progress bar
	void HideText(bool firstHide)
	{
		m_bPersistent = false;
		m_bTextRequested = false;

		if (m_progressContainer)
			m_progressContainer.SetVisible(false);
		
		float duration = 1.0;
		if (firstHide) { duration = 0.0 };

		super.Show(false, duration, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_Gamestate: HideText called", LogLevel.NORMAL);
	}

	private void HideLogo()
    {
		if (m_bPersistent)
			return;

		m_bTextRequested = false;
		super.Show(false, 3.0, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Gamestate: hiding m_text", LogLevel.VERBOSE);
    }
}
