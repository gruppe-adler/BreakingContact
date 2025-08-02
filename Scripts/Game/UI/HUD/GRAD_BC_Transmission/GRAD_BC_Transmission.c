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
		PrintFormat("GRAD_BC_Transmission: Hiding hint", LogLevel.ERROR);
	}
	
	void showTransmissionHint(string faction, ETransmissionState state) {
		if (!m_infoImage) {
			PrintFormat("GRAD_BC_Transmission: TransmissionStarted: m_infoImage is missing", LogLevel.ERROR);
			return;
		}

		// Only show for valid states
		bool shouldShow = false;
		switch (state)
		{
			case ETransmissionState.TRANSMITTING:
			case ETransmissionState.INTERRUPTED:
			case ETransmissionState.DISABLED:
			case ETransmissionState.DONE:
				shouldShow = true;
				break;
			default:
				shouldShow = false;
				break;
		}
		if (!shouldShow) {
			super.Show(false, 0.0, EAnimationCurve.LINEAR);
			return;
		}

		switch (state)
			{
				case ETransmissionState.TRANSMITTING: {
					m_currentDisplayCached = e_currentDisplay.STARTED;
					if (faction == "US") {
						m_infoImage.LoadImageTexture(0, "{B8C70A7749D318C2}UI/Transmission/us_established.edds");	
					} else {
						m_infoImage.LoadImageTexture(0, "{3B1DCBDCE5DA9CEB}UI/Transmission/rus_established.edds");	
					}
					
					// Play transmission established sound
					SCR_UISoundEntity.SoundEvent("{3550A48D94380CC2}sounds/beep2.wav");
					
					break;
				}

				case ETransmissionState.INTERRUPTED: {
					m_currentDisplayCached = e_currentDisplay.INTERRUPTED;
					if (faction == "US") {
						m_infoImage.LoadImageTexture(0, "{2AE03288555F4237}UI/Transmission/us_cutoff.edds");	
					} else {
						m_infoImage.LoadImageTexture(0, "{85D0D3AA68675C00}UI/Transmission/rus_cutoff.edds");	
					}
				
					// Play transmission established sound
					SCR_UISoundEntity.SoundEvent("{2F13E941825B650F}sounds/signal_lost.wav");
				
					break;
				}

				case ETransmissionState.DISABLED: {
					m_currentDisplayCached = e_currentDisplay.INTERRUPTED;
					if (faction == "US") {
						m_infoImage.LoadImageTexture(0, "{2AE03288555F4237}UI/Transmission/us_cutoff.edds");	
					} else {
						m_infoImage.LoadImageTexture(0, "{85D0D3AA68675C00}UI/Transmission/rus_cutoff.edds");	
					}
					break;
				}

				case ETransmissionState.DONE: {
					m_currentDisplayCached = e_currentDisplay.DONE;
					if (faction == "US") {
						m_infoImage.LoadImageTexture(0, "{92B573238A373130}UI/Transmission/us_done.edds");	
					} else {
						m_infoImage.LoadImageTexture(0, "{8BE8C2B40DACD244}UI/Transmission/rus_done.edds");	
					}
					break;
				}

				default: {
					Print(string.Format("Breaking Contact TPC - No known ETransmissionState %1", state), LogLevel.ERROR);
					break;
				}
			}
			
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

