//------------------------------------------------------------------------------------------------
//! Condition for showing the confirm spawn button
//! Only shows for USSR faction and while choosing spawn
[BaseContainerProps()]
class GRAD_BC_ConfirmSpawnCondition : SCR_AvailableActionCondition
{
	protected static int s_iLogThrottle = 0;

	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		// Log active input contexts every ~120 frames to avoid spam
		s_iLogThrottle++;
		if (s_iLogThrottle >= 120)
		{
			s_iLogThrottle = 0;
			PlayerController playerController = GetGame().GetPlayerController();
			if (playerController)
			{
				ActionManager actionManager = playerController.GetActionManager();
				if (actionManager)
				{
					bool spectatorCtx = actionManager.IsContextActive("SpectatorContext");
					bool mapCtx = actionManager.IsContextActive("MapContext");
					bool characterCtx = actionManager.IsContextActive("CharacterContext");
					Print(string.Format("BC Debug - ConfirmSpawnCondition contexts: SpectatorContext=%1 MapContext=%2 CharacterContext=%3", spectatorCtx, mapCtx, characterCtx), LogLevel.NORMAL);
				}
			}
		}

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
