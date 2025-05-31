enum e_currentDisplay {
	NONE,
	STARTED,
	INTERRUPTED,
	DONE
}

class GRAD_BC_Transmission: SCR_InfoDisplayExtended
{
	private ImageWidget m_infoImage;
	private e_currentDisplay m_currentDisplayCached = e_currentDisplay.NONE;
	
	override event void DisplayInit(IEntity owner) {
		super.DisplayInit(owner);
		
		m_wRoot = GetGame().GetWorkspace().CreateWidgets("{D73BB17AFDB687C2}UI/Layouts/HUD/GRAD_BC_Transmission/GRAD_BC_Transmission.layout", null);
		
		if (!m_wRoot) {
			PrintFormat("GRAD_BC_Transmission: no m_wRoot found", LogLevel.ERROR);
			return;
		}
		
		Widget w = m_wRoot.FindAnyWidget("GRAD_BC_Transmission_Widget");
   		m_infoImage = ImageWidget.Cast(w);
		
		if (!m_infoImage) {
			PrintFormat("GRAD_BC_Transmission_Widget: no GRAD_BC_Transmission_Widget found", LogLevel.ERROR);
			return;
		}
		
		super.Show(false, 0.0, EAnimationCurve.LINEAR);
	}
	
	void TransmissionStarted() {
		if (!m_infoImage) {
			PrintFormat("GRAD_BC_Transmission: TransmissionStarted → m_infoImage is missing", LogLevel.ERROR);
			return;
		}
		
		// Change to the “started” texture
		m_infoImage.LoadImageTexture(0, "{B8C70A7749D318C2}UI/Transmission/us_established.edds");		
		m_currentDisplayCached = e_currentDisplay.STARTED;
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		
		// After 15 s, attempt to hide (fade out)
		GetGame().GetCallqueue().CallLater(
            FadeOutIfStill,       // name of our helper method
            15000,                // 15 sec
            false,                // not a loop
            m_currentDisplayCached // pass the state enum as a parameter
        );
	}
	
	void TransmissionInterrupted() {
		if (!m_infoImage) {
			PrintFormat("GRAD_BC_Transmission: TransmissionInterrupted → m_infoImage is missing", LogLevel.ERROR);
			return;
		}
		
		m_infoImage.LoadImageTexture(0, "{2AE03288555F4237}UI/Transmission/us_cutoff.edds");
		m_currentDisplayCached = e_currentDisplay.INTERRUPTED;
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		
		// After 15 s, attempt to hide (fade out)
		GetGame().GetCallqueue().CallLater(
            FadeOutIfStill,       // name of our helper method
            15000,                // 15 sec
            false,                // not a loop
            m_currentDisplayCached // pass the state enum as a parameter
        );
	}
	
	void TransmissionDone() {
		if (!m_infoImage) {
			PrintFormat("GRAD_BC_Transmission: TransmissionDone → m_infoImage is missing", LogLevel.ERROR);
			return;
		}
		
		m_infoImage.LoadImageTexture(0, "{92B573238A373130}UI/Transmission/us_done.edds");
		m_currentDisplayCached = e_currentDisplay.DONE;
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		
		// After 15 s, attempt to hide (fade out)
		GetGame().GetCallqueue().CallLater(
            FadeOutIfStill,       // name of our helper method
            15000,                // 15 sec
            false,                // not a loop
            m_currentDisplayCached // pass the state enum as a parameter
        );
	}
	
	void FadeOutIfStill(e_currentDisplay stateToCheck)
    {
        if (m_currentDisplayCached == stateToCheck)
        {
            super.Show(false, 0.5, EAnimationCurve.EASE_IN_QUART);
            PrintFormat("GRAD_BC_Transmission: Fading out state %1", stateToCheck, LogLevel.DEBUG);
        }
        else
        {
            // Something else replaced it in the meantime; do nothing.
            PrintFormat(
                "GRAD_BC_Transmission: Skipping fade‐out for %1 because current is %2",
                stateToCheck,
                m_currentDisplayCached,
                LogLevel.DEBUG
            );
        }
    }
}

