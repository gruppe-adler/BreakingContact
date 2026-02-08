class GRAD_BC_Gamestate: SCR_InfoDisplayExtended
{
	private RichTextWidget m_text;
	private bool m_bPersistent = false;

	// Progress bar widgets (created programmatically)
	private Widget m_progressContainer;
	private Widget m_progressBg;
	private ProgressBarWidget m_progressBar;

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
	}

	void ShowText(string message)
    {
		m_bPersistent = false;
		m_text.SetText(message);
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		PrintFormat("GRAD_BC_Gamestate: showText called!", LogLevel.VERBOSE);
    	GetGame().GetCallqueue().CallLater(HideLogo, 15000);
    }

	// Show text that stays visible until explicitly hidden
	void ShowPersistentText(string message)
	{
		m_bPersistent = true;
		if (m_text)
			m_text.SetText(message);
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
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

		// Create progress bar widgets on first use
		if (!m_progressContainer)
			CreateProgressBar();

		if (m_progressBar)
		{
			m_progressBar.SetCurrent(progress);
			m_progressContainer.SetVisible(true);
		}

		// Update text with percentage
		int percentage = Math.Round(progress * 100);
		if (m_text)
			m_text.SetText(string.Format("Replay loading... %1%%", percentage));
	}

	// Explicitly hide the gamestate display and progress bar
	void HideText()
	{
		m_bPersistent = false;

		if (m_progressContainer)
			m_progressContainer.SetVisible(false);

		super.Show(false, 1.0, EAnimationCurve.EASE_OUT_QUART);
		Print("GRAD_BC_Gamestate: HideText called", LogLevel.NORMAL);
	}

	private void CreateProgressBar()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_wRoot)
			return;

		// Container for progress bar - positioned below the text
		m_progressContainer = workspace.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR, new Color(0, 0, 0, 0), 0, m_wRoot);
		if (!m_progressContainer)
			return;

		FrameSlot.SetAnchorMin(m_progressContainer, 0.3, 0);
		FrameSlot.SetAnchorMax(m_progressContainer, 0.7, 0);
		FrameSlot.SetOffsets(m_progressContainer, 0, 80, 0, 100);

		// Progress bar background (dark)
		m_progressBg = workspace.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE, Color.FromRGBA(0, 0, 0, 150), 0, m_progressContainer);
		if (m_progressBg)
		{
			FrameSlot.SetAnchorMin(m_progressBg, 0, 0);
			FrameSlot.SetAnchorMax(m_progressBg, 1, 1);
			FrameSlot.SetOffsets(m_progressBg, 0, 0, 0, 0);
		}

		// Progress bar fill (green)
		m_progressBar = ProgressBarWidget.Cast(workspace.CreateWidget(WidgetType.ProgressBarWidgetTypeID, WidgetFlags.VISIBLE, Color.FromRGBA(100, 200, 100, 255), 0, m_progressContainer));
		if (m_progressBar)
		{
			m_progressBar.SetMin(0);
			m_progressBar.SetMax(1);
			m_progressBar.SetCurrent(0);
			FrameSlot.SetAnchorMin(m_progressBar, 0.02, 0.1);
			FrameSlot.SetAnchorMax(m_progressBar, 0.98, 0.9);
			FrameSlot.SetOffsets(m_progressBar, 0, 0, 0, 0);
		}

		Print("GRAD_BC_Gamestate: Progress bar created", LogLevel.NORMAL);
	}

	private void HideLogo()
    {
		if (m_bPersistent)
			return;

		super.Show(false, 3.0, EAnimationCurve.EASE_OUT_QUART);
		PrintFormat("GRAD_BC_Gamestate: hiding m_text", LogLevel.VERBOSE);
    }
}
