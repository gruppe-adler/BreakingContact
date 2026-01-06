class GRAD_BC_DisableRadioTruck : ScriptedUserAction
{
	// This scripted user action if triggered runs on all clients and server
	
	private GRAD_BC_RadioTruckComponent m_radioTruckComponent;
	protected float m_fStartTime = -1;
	protected const float ACTION_DURATION = 30.0; // 30 seconds in seconds

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
		// get component in regular intervals due to race condition after spawn
		GRAD_BC_RadioTruckComponent radioComp = GetRadioTruckComponent(GetOwner());
		
		// Only show for BLUFOR players
		if (!IsUserBlufor(user))
			return false;
			
		// Only show if radio truck is not already disabled
		if (radioComp && radioComp.GetIsDisabled())
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

		// Ensure component is loaded
		GRAD_BC_RadioTruckComponent radioComp = GetRadioTruckComponent(GetOwner());

		// Only allow if radio truck is not already disabled
		if (radioComp && radioComp.GetIsDisabled())
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
	override float GetActionProgressScript(float fProgress, float timeSlice)
	{
		if (m_fStartTime < 0)
		{
			m_fStartTime = System.GetTickCount();
			return 0.0;
		}
		
		float elapsed = (System.GetTickCount() - m_fStartTime) / 1000.0; // Convert ms to seconds
		float progress = Math.Clamp(elapsed / ACTION_DURATION, 0.0, 1.0);
		
		return progress;
	}
	
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		Print("BC Debug - PerformAction() DisableRadioTruck", LogLevel.NORMAL);

		// Ensure component is loaded
		GetRadioTruckComponent(pOwnerEntity);

		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return;
		}
		
		// This runs on server since HasLocalEffectOnlyScript() returns false
		// This is called when the action completes (progress reaches 100%)
		
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
		
		// Reset start time for next use
		m_fStartTime = -1;
	}
	
	//------------------------------------------------------------------------------------------------
	private GRAD_BC_RadioTruckComponent GetRadioTruckComponent(IEntity owner)
	{
	    // If we found it previously, return it
	    if (m_radioTruckComponent)
	        return m_radioTruckComponent;
	
	    // If not, try to find it again (this fixes the race condition)
	    IEntity radioTruck = owner.GetParent();
	    if (!radioTruck) {
			Print("BC Debug - no radio truck to be found with GetRadioTruckComponent", LogLevel.WARNING);
	        return null;
		}
	
	    m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
		Print("BC Debug - filling radio truck component in GetRadioTruckComponent", LogLevel.NORMAL);
	    
	    return m_radioTruckComponent;
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
		IEntity radioTruck = pOwnerEntity.GetParent();
		if (!radioTruck)
		{
			Print("BC Debug - no parent for disableradiotruck action", LogLevel.WARNING);
			return;
		}
		m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
	}
}
