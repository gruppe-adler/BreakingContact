enum e_currentTrafficDisplay {
	NONE,
	GUNFIGHT,
	KILLED
}

class GRAD_BC_Traffic: SCR_InfoDisplayExtended
{
	private ImageWidget m_infoImage;
	private e_currentTrafficDisplay m_currentDisplayCached = e_currentTrafficDisplay.NONE;
	
	override event void DisplayInit(IEntity owner) {
		super.DisplayInit(owner);
		Print("GRAD_BC_Traffic: DisplayInit called", LogLevel.NORMAL);
		
		// Hide to not block input in lobby
		FadeOutIfStill(e_currentTrafficDisplay.NONE);
	}
	
	override protected void DisplayStartDraw(IEntity owner) {
		super.DisplayStartDraw(owner);
		
		Print("GRAD_BC_Traffic: DisplayStartDraw called", LogLevel.NORMAL);
		
		m_wRoot = GetGame().GetWorkspace().CreateWidgets("{71FCB653E258569E}UI/Layouts/HUD/GRAD_BC_Traffic/GRAD_BC_Traffic.layout", null);
		
		if (!m_wRoot) {
			Print("GRAD_BC_Traffic: no m_wRoot found", LogLevel.NORMAL);
			return;
		}

		if (m_wRoot) {
        Print("--- DUMPING WIDGET HIERARCHY ---");
        DumpWidgets(m_wRoot, 0);
        
        m_infoImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("GRAD_BC_Traffic_Widget"));
    }

		
		Print("GRAD_BC_Traffic: m_wRoot created successfully", LogLevel.NORMAL);
		
		Widget w = m_wRoot.FindAnyWidget("GRAD_BC_Traffic_Widget");
   		m_infoImage = ImageWidget.Cast(w);
		
		if (!m_infoImage) {
			Print("GRAD_BC_Traffic_Widget: no GRAD_BC_Traffic_Widget found", LogLevel.NORMAL);
			return;

		}
		
		Print("GRAD_BC_Traffic: m_infoImage found and initialized", LogLevel.NORMAL);
		
		// Ensure the display starts hidden
		Show(false, 0.0, EAnimationCurve.LINEAR);
		Print("GRAD_BC_Traffic: Display hidden by default", LogLevel.NORMAL);
		
		// Disable input on the root widget to prevent blocking lobby/map interactions
		if (m_wRoot)
			m_wRoot.SetFlags(WidgetFlags.NOFOCUS);
		
		Print("GRAD_BC_Traffic: DisplayStartDraw completed successfully", LogLevel.NORMAL);
	}
	
	
	// Helper to see exactly what names the engine is seeing
	void DumpWidgets(Widget w, int indent) {
		if (!w) return;
		string space = "";
		for(int i=0; i<indent; i++) space += "  ";
		
		Print(space + "- : " + w.GetName() + " [Type: " + w.GetTypeName() + "]");
		
		Widget child = w.GetChildren();
		while (child) {
			DumpWidgets(child, indent + 1);
			child = child.GetSibling();
		}
	}

	void showTrafficHint(e_currentTrafficDisplay currentHint, vector eventLocation) {
		
		if (!m_infoImage) {
			Print("GRAD_BC_Traffic: TransmissionStarted: m_infoImage is missing", LogLevel.NORMAL);
			return;
		}

		Print("GRAD_BC_Traffic: m_infoImage is valid, proceeding with hint display", LogLevel.NORMAL);

		// Only show for valid states
		bool shouldShow = false;
		switch (currentHint)
		{
			case e_currentTrafficDisplay.KILLED:
			case e_currentTrafficDisplay.GUNFIGHT:
				shouldShow = true;
				break;
			
			case e_currentTrafficDisplay.NONE:
				shouldShow = false;
				break;
			
			default:
				shouldShow = false;
				break;
		}
		if (!shouldShow) {
			super.Show(false, 0.0, EAnimationCurve.LINEAR);
			return;
		}

		string markerLabel;
		switch (currentHint)
			{
				case e_currentTrafficDisplay.GUNFIGHT: {
					m_currentDisplayCached = e_currentTrafficDisplay.GUNFIGHT;
				m_infoImage.LoadImageTexture(0, "{36041F3E1CA58B61}UI/Textures/Tasks/TaskIcons/64/Icon_Task_Neutralize.edds");	
					GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
					if (playerComponent == null)
						return;
					
					vector location = playerComponent.GetOwner().GetOrigin();
					
					// Play transmission established sound
					Print("BC Traffic UI: Civ in gunfight", LogLevel.NORMAL);
					AudioSystem.PlayEvent("{1C5FE7EFA950B78D}sounds/BC_beep.acp", "beep", location);
					
					break;
				}

				case e_currentTrafficDisplay.KILLED: {
					m_currentDisplayCached = e_currentTrafficDisplay.KILLED;
				m_infoImage.LoadImageTexture(0, "{1CD3939995A5E6E8}UI/Textures/InventoryIcons/Medical/bleeding_icon_inner_UI.edds");	
					GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
					if (playerComponent == null)
						return;
					
					vector location = playerComponent.GetOwner().GetOrigin();
					
					// Play transmission established sound
					Print("BC Traffic UI: Civ had been killed", LogLevel.NORMAL);
					AudioSystem.PlayEvent("{1C5FE7EFA950B78D}sounds/BC_beep.acp", "beep", location);
					
					break;
				}

				
				default: {
					Print(string.Format("BC Traffic UI: No hint", currentHint), LogLevel.ERROR);
					markerLabel = "UNKNOWN";
					break;
				}
			}
			
		// Create map marker for all players
		CreateTrafficMapMarker(eventLocation, markerLabel);
		
		super.Show(true, 0.5, EAnimationCurve.EASE_OUT_QUART);
		
		
		
		// After 15 s, attempt to hide (fade out)
		GetGame().GetCallqueue().CallLater(
            FadeOutIfStill,       // name of our helper method
            15000,                // 15 sec
            false,                // not a loop
            m_currentDisplayCached // pass the state enum as a parameter
        );
	}
	
	void FadeOutIfStill(e_currentTrafficDisplay stateToCheck)
    {
        if (m_currentDisplayCached == stateToCheck)
        {
            super.Show(false, 0.5, EAnimationCurve.EASE_IN_QUART);
            PrintFormat("GRAD_BC_Traffic: Fading out state %1", stateToCheck, LogLevel.DEBUG);
        }
        else
        {
            // Something else replaced it in the meantime; do nothing.
            PrintFormat(
                "GRAD_BC_Traffic: Skipping fade‐out for %1 because current is %2",
                stateToCheck,
                m_currentDisplayCached,
                LogLevel.DEBUG
            );
        }
    }
	
	protected void CreateTrafficMapMarker(vector position, string label)
	{
		// Get the map entity to access the traffic marker layer
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
		{
			Print("GRAD_BC_Traffic: No map entity found, cannot create marker", LogLevel.WARNING);
			return;
		}
		
		// Get the traffic marker layer
		GRAD_BC_TrafficMarkerLayer trafficLayer = GRAD_BC_TrafficMarkerLayer.Cast(mapEntity.GetMapModule(GRAD_BC_TrafficMarkerLayer));
		if (!trafficLayer)
		{
			Print("GRAD_BC_Traffic: Traffic marker layer not found", LogLevel.WARNING);
			return;
		}
		
		// Add the marker
		trafficLayer.AddTrafficMarker(position, label);
		Print(string.Format("GRAD_BC_Traffic: Created map marker at %1 with label '%2'", position, label), LogLevel.NORMAL);
	}
}

