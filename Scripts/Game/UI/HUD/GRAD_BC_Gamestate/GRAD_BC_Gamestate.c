class GRAD_BC_Gamestate: SCR_InfoDisplayExtended
{
	private RichTextWidget m_text;
	private bool m_bPersistent = false;

	// Match outcome line kept above the loading text for the whole replay load, so players
	// know why the replay is running while they wait for it.
	private string m_sHeadline;

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
	}

	// Set (or clear, with an empty string) the outcome line shown above every subsequent message.
	void SetHeadline(string headline)
	{
		m_sHeadline = headline;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_Gamestate: SetHeadline: %1", headline), LogLevel.NORMAL);
	}

	// Compose the headline and the current message into the single text widget.
	private void ApplyText(string message)
	{
		if (!m_text)
			return;

		if (m_sHeadline.IsEmpty())
		{
			m_text.SetText(message);
			return;
		}

		// Outcome line on top in the accent colour, loading text below it at a smaller size so
		// the reason for the replay reads as the headline rather than competing with the progress.
		m_text.SetText(string.Format(
			"<color rgba='255,214,102,255'>%1</color><br/><size scale='0.65'>%2</size>",
			m_sHeadline, message));
	}

	void ShowText(string message)
    {
		m_bPersistent = false;
		// Transient phase messages are unrelated to the replay outcome line - drop it.
		m_sHeadline = string.Empty;
		ApplyText(message);
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
		ApplyText(message);
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_Gamestate: ShowPersistentText: %1", message), LogLevel.NORMAL);
	}

	// Update text without changing visibility or resetting timers
	void UpdateText(string message)
	{
		ApplyText(message);
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
		ApplyText(string.Format("Replay loading... %1%%", percentage));
	}

	// Explicitly hide the gamestate display and progress bar
	void HideText()
	{
		m_bPersistent = false;
		m_sHeadline = string.Empty;

		if (m_progressContainer)
			m_progressContainer.SetVisible(false);

		super.Show(false, 1.0, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_Gamestate: HideText called", LogLevel.NORMAL);
	}

	private void HideLogo()
    {
		if (m_bPersistent)
			return;

		super.Show(false, 3.0, EAnimationCurve.EASE_OUT_QUART);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("GRAD_BC_Gamestate: hiding m_text", LogLevel.VERBOSE);
    }
}
