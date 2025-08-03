class GRAD_BC_DisableRadioTruck : ScriptedUserAction
{
	// This scripted user action if triggered runs on all clients and server
	
	private GRAD_BC_RadioTruckComponent m_radioTruckComponent;

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
	    return false; // Action runs on server
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
	    return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		// Only show for BLUFOR players
		if (!IsUserBlufor(user))
			return false;
			
		// Only show if radio truck is not already disabled
		if (m_radioTruckComponent && m_radioTruckComponent.GetIsDisabled())
			return false;
			
		return CanBePerformedScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// Only allow for BLUFOR players
		if (!IsUserBlufor(user))
			return false;
			
		// Only allow if Breaking Contact Manager exists and game has started
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm) return false;
		
		EBreakingContactPhase currentPhase = bcm.GetBreakingContactPhase();
		// Only allow in GAME phase
		if (currentPhase != EBreakingContactPhase.GAME)
			return false;

		// Only allow if radio truck is not already disabled
		if (m_radioTruckComponent && m_radioTruckComponent.GetIsDisabled())
			return false;
			
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsUserBlufor(IEntity user)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
			return false;
			
		string factionKey = character.GetFactionKey();
		return (factionKey == "US");
	}
	
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		Print("BC Debug - PerformAction() DisableRadioTruck", LogLevel.NORMAL);

		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return;
		}
		
		// This runs on server since HasLocalEffectOnlyScript() returns false
		
		// Disable the radio truck permanently
		m_radioTruckComponent.SetIsDisabled(true);
		
		// Notify the player who performed the action
		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.Cast(pUserEntity.FindComponent(GRAD_PlayerComponent));
		if (playerComponent)
		{
			const string title = "Breaking Contact";
			const string message = "Radio truck disabled successfully! BLUFOR wins!";
			const int duration = 10;
			const bool isSilent = false;
			playerComponent.ShowHint(message, title, duration, isSilent);
		}
		
		// Notify Breaking Contact Manager that BLUFOR has won
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
		{
			bcm.SetBluforWin();
			Print("BC Debug - BLUFOR win condition triggered: Radio truck disabled", LogLevel.NORMAL);
		}
		else
		{
			Print("BC Debug - Could not find Breaking Contact Manager", LogLevel.WARNING);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Disable Radio Truck";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_RadioTruckComponent));
	}
}
