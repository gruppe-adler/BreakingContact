[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Manager for civilian death events and UI triggers.")]
class GRAD_BC_TrafficHintManagerComponentClass : ScriptComponentClass {}

class GRAD_BC_TrafficHintManagerComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		// Enable frame updates so we can check every frame
   		 SetEventMask(owner, EntityEvent.FRAME);
	}
	
	override void EOnFrame(IEntity owner, float timeSlice)
	{
	    PlayerController pc = GetGame().GetPlayerController();
	    
	    // Check if the local PC is now this owner
	    if (pc && pc == owner)
	    {
	        SCR_TrafficEvents.OnCivilianEvent.Insert(OnCivilianEvent);
	        Print("GRAD_BC_TrafficHintManagerComponent: Local PlayerController found via frame poll!");
	        
	        // IMPORTANT: Turn off the frame event once we are subscribed
	        ClearEventMask(owner, EntityEvent.FRAME);
	    }
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// Cleanup subscription
		SCR_TrafficEvents.OnCivilianEvent.Remove(OnCivilianEvent);
    		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCivilianEvent(vector location, string eventtype)
	{

		Print(string.Format("GRAD_BC_TrafficHintManagerComponent: Civilian gunfight or death detected at %1", location), LogLevel.NORMAL);

		// Find the UI display via the HUD Manager
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.Cast(GetGame().GetPlayerController().FindComponent(SCR_HUDManagerComponent));
		if (!hudManager)
			return;
		
		e_currentTrafficDisplay displayType = e_currentTrafficDisplay.NONE;
		
		switch (eventtype)
			{
				case "killed": { displayType = e_currentTrafficDisplay.KILLED; break; };
				case "gunfight": { displayType = e_currentTrafficDisplay.GUNFIGHT; break; };
				default: {
					displayType = e_currentTrafficDisplay.NONE;
					Print(string.Format("GRAD_BC_TrafficHintManagerComponent: No hint", eventtype), LogLevel.ERROR);
					break;
				}
		};

		GRAD_BC_Traffic display = GRAD_BC_Traffic.Cast(hudManager.FindInfoDisplay(GRAD_BC_Traffic));
		if (display)
		{
			display.showTrafficHint(displayType);
		} else {
			Print(string.Format("GRAD_BC_TrafficHintManagerComponent: No display", eventtype), LogLevel.ERROR);
		}
	}
}