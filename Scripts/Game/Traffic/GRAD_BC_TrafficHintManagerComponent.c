[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Manager for civilian death events and UI triggers.")]
class GRAD_BC_TrafficHintManagerComponentClass : ScriptComponentClass {}

class GRAD_BC_TrafficHintManagerComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		// Only run on the client/player who owns this component
		if (GetGame().GetPlayerController() != owner)
			return;

		// Subscribe to the global traffic event
		SCR_TrafficEvents.OnCivilianEvent.Insert(OnCivilianEvent);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// Cleanup subscription
		SCR_TrafficEvents.OnCivilianEvent.Remove(OnCivilianEvent);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCivilianEvent(vector location, string eventtype)
	{

		Print(string.Format("GRAD_BC_Manager: Civilian gunfight or death detected at %1", location), LogLevel.NORMAL);

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
					Print(string.Format("BC TrafficHintManagerComponent: No hint", eventtype), LogLevel.ERROR);
					break;
				}
		};

		GRAD_BC_Traffic display = GRAD_BC_Traffic.Cast(hudManager.FindInfoDisplay(GRAD_BC_Traffic));
		if (display)
		{
			display.showTransmissionHint(displayType);
		}
	}
}