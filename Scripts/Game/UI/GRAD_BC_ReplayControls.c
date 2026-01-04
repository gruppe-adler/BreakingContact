class GRAD_BC_ReplayControls : Managed
{
	protected Widget m_wRoot;
	protected ButtonWidget m_wPlayPauseButton;
	protected ButtonWidget m_wSlowButton;
	protected ButtonWidget m_wNormalButton;
	protected ButtonWidget m_wFastButton;
	protected ButtonWidget m_wCloseButton;
	protected TextWidget m_wTimeDisplay;
	
	protected GRAD_BC_ReplayManager m_ReplayManager;
	protected bool m_bIsPaused = false;
	
	//------------------------------------------------------------------------------------------------
	void DisplayInit(IEntity owner)
	{
		Print("GRAD_BC_ReplayControls: DisplayInit called", LogLevel.NORMAL);
		
		m_ReplayManager = GRAD_BC_ReplayManager.GetInstance();
		if (!m_ReplayManager)
		{
			Print("GRAD_BC_ReplayControls: No replay manager found", LogLevel.ERROR);
			return;
		}
		
		Print("GRAD_BC_ReplayControls: Replay manager found, creating UI", LogLevel.NORMAL);
		
		// Create the widget
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			Print("GRAD_BC_ReplayControls: No workspace found!", LogLevel.ERROR);
			return;
		}
		
		Print("GRAD_BC_ReplayControls: Workspace found, creating widget", LogLevel.NORMAL);
			
		// Try simple layout first (more reliable)
		m_wRoot = workspace.CreateWidgets("UI/Layouts/HUD/GRAD_BC_ReplayControls.layout");
		if (!m_wRoot)
		{
			Print("GRAD_BC_ReplayControls: Simple layout failed - trying complex layout", LogLevel.ERROR);
			// Try complex layout as fallback
			m_wRoot = workspace.CreateWidgets("UI/Layouts/HUD/GRAD_BC_ReplayControls/GRAD_BC_ReplayControls.layout");
			if (!m_wRoot)
			{
				Print("GRAD_BC_ReplayControls: Fallback layout also failed", LogLevel.ERROR);
				return;
			}
		}
		
		Print("GRAD_BC_ReplayControls: Widget created successfully, finding child widgets", LogLevel.NORMAL);
		
		// Find child widgets
		m_wPlayPauseButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("PlayPauseButton"));
		m_wSlowButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("SlowButton"));
		m_wNormalButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("NormalButton"));
		m_wFastButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("FastButton"));
		m_wCloseButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("CloseButton"));
		m_wTimeDisplay = TextWidget.Cast(m_wRoot.FindAnyWidget("TimeDisplay"));
		
		// Debug: Check which widgets were found
		Print(string.Format("GRAD_BC_ReplayControls: Found widgets - PlayPause: %1, Slow: %2, Normal: %3, Fast: %4, Close: %5, TimeDisplay: %6", 
			m_wPlayPauseButton != null, m_wSlowButton != null, m_wNormalButton != null, 
			m_wFastButton != null, m_wCloseButton != null, m_wTimeDisplay != null), LogLevel.NORMAL);
		
		// Start update timer for input checking and display updates
		GetGame().GetCallqueue().CallLater(UpdateControls, 100, true);
		
		Print("GRAD_BC_ReplayControls: UI initialized successfully", LogLevel.NORMAL);
		
		// Verify the UI is actually visible
		if (m_wRoot && m_wRoot.IsVisible())
		{
			Print("GRAD_BC_ReplayControls: UI is visible and ready", LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_ReplayControls: WARNING - UI may not be visible", LogLevel.WARNING);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void Cleanup()
	{
		GetGame().GetCallqueue().Remove(UpdateControls);
		
		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdateControls()
	{
		if (!m_wTimeDisplay || !m_ReplayManager)
			return;
			
		// Update time display
		float currentTime = m_ReplayManager.GetCurrentPlaybackTime();
		float totalTime = m_ReplayManager.GetTotalDuration();
		
		string currentTimeStr = FormatTime(currentTime);
		string totalTimeStr = FormatTime(totalTime);
		
		m_wTimeDisplay.SetText(string.Format("Replay: %1 / %2 (Speed: %.1fx)", currentTimeStr, totalTimeStr, m_ReplayManager.GetPlaybackSpeed()));
		
		// Check for button clicks using input manager
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;
			
		// Check mouse clicks on buttons
		if (inputManager.IsUsingMouseAndKeyboard())
		{
			// Simple click detection - could be improved with proper UI event handling
			if (inputManager.GetActionTriggered("MenuSelect"))
			{
				CheckButtonClicks();
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CheckButtonClicks()
	{
		// This is a simplified approach - in a full implementation you'd want proper UI event handling
		// For now, we'll use keyboard shortcuts as a fallback
		
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;
			
		// Keyboard shortcuts for replay control
		if (inputManager.GetActionTriggered("VONDirectSpeaking")) // Space key
		{
			OnPlayPauseClicked();
		}
		else if (inputManager.GetActionValue("CharacterMoveFastModifier")) // Shift key held
		{
			if (inputManager.GetActionTriggered("CharacterMoveForward")) // W key
				OnFastClicked();
			else if (inputManager.GetActionTriggered("CharacterMoveBackward")) // S key  
				OnSlowClicked();
		}
		else if (inputManager.GetActionTriggered("MenuBack")) // Escape key
		{
			OnCloseClicked();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	string FormatTime(float timeInSeconds)
	{
		int minutes = Math.Floor(timeInSeconds / 60);
		int seconds = Math.Floor(timeInSeconds - (minutes * 60)); // Avoid modulo operator
		return string.Format("%02d:%02d", minutes, seconds);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnPlayPauseClicked()
	{
		if (!m_ReplayManager)
			return;
			
		if (m_bIsPaused)
		{
			m_ReplayManager.ResumePlayback();
			// Try to update button text if possible
			if (m_wPlayPauseButton)
			{
				TextWidget buttonText = TextWidget.Cast(m_wPlayPauseButton.FindAnyWidget("Text"));
				if (buttonText)
					buttonText.SetText("PAUSE");
			}
			m_bIsPaused = false;
			Print("GRAD_BC_ReplayControls: Playback resumed", LogLevel.NORMAL);
		}
		else
		{
			m_ReplayManager.PausePlayback();
			// Try to update button text if possible  
			if (m_wPlayPauseButton)
			{
				TextWidget buttonText = TextWidget.Cast(m_wPlayPauseButton.FindAnyWidget("Text"));
				if (buttonText)
					buttonText.SetText("PLAY");
			}
			m_bIsPaused = true;
			Print("GRAD_BC_ReplayControls: Playback paused", LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnSlowClicked()
	{
		if (m_ReplayManager)
			m_ReplayManager.SetPlaybackSpeed(0.5);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnNormalClicked()
	{
		if (m_ReplayManager)
			m_ReplayManager.SetPlaybackSpeed(1.0);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnFastClicked()
	{
		if (m_ReplayManager)
			m_ReplayManager.SetPlaybackSpeed(2.0);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnCloseClicked()
	{
		if (m_ReplayManager)
			m_ReplayManager.StopPlayback();
		Cleanup();
	}
}
