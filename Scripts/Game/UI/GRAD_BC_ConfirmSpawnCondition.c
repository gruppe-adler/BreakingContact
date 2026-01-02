//------------------------------------------------------------------------------------------------
//! Condition for showing the confirm spawn button
//! Only shows for USSR faction and while choosing spawn
[BaseContainerProps()]
class GRAD_BC_ConfirmSpawnCondition : SCR_AvailableActionCondition
{
	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		// Check if player component exists
		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
		if (!playerComponent)
			return false;
		
		// Only show if player is choosing spawn
		if (!playerComponent.IsChoosingSpawn())
			return false;
		
		// Get player controller
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController)
			return false;
		
		// Get character
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!ch)
			return false;
		
		// Only show for USSR faction (not BLUFOR)
		string factionKey = ch.GetFactionKey();
		if (factionKey != "USSR")
			return false;
		
		// Only enable button if spawn position calculation is complete
		if (!playerComponent.IsSpawnPositionReady())
		{
			// Still return true to show the button, but it will be visually disabled
			// and the ConfirmSpawn() method will block execution
			return true;
		}
		
		return true;
	}
}
