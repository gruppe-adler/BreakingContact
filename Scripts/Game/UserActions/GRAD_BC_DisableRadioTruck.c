class GRAD_BC_DisableRadioTruck : SCR_ScriptedUserAction
{
	// This scripted user action if triggered runs on all clients and server

	private GRAD_BC_RadioTruckComponent m_radioTruckComponent;
	protected bool m_bActionCompleted = false;

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
		if (!bcm)
			return false;

		EBreakingContactPhase currentPhase = bcm.GetBreakingContactPhase();
		// Only allow in GAME phase
		if (currentPhase != EBreakingContactPhase.GAME)
			return false;

		// Ensure component is loaded
		GRAD_BC_RadioTruckComponent radioComp = GetRadioTruckComponent(GetOwner());

		// Only allow if radio truck is not already disabled
		if (radioComp && radioComp.GetIsDisabled())
			return false;

		if (!radioComp)
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
	override void OnActionStart(IEntity pUserEntity)
	{
		m_bActionCompleted = false;
		Print("BC Debug - DisableRadioTruck: OnActionStart called", LogLevel.NORMAL);
		super.OnActionStart(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		m_bActionCompleted = false;
		Print("BC Debug - DisableRadioTruck: OnActionCanceled called", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformContinuousAction(IEntity pOwnerEntity, IEntity pUserEntity, float timeSlice)
	{
		// LoopActionUpdate returns true when the action duration is complete
		if (!LoopActionUpdate(timeSlice))
			return;

		// Only execute once
		if (m_bActionCompleted)
			return;

		m_bActionCompleted = true;
		PerformAction(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		Print("BC Debug - PerformAction() DisableRadioTruck - Action Complete!", LogLevel.NORMAL);

		// Ensure component is loaded
		GetRadioTruckComponent(pOwnerEntity);

		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return;
		}

		// Check if we're on the server - only server should execute the actual logic
		// With CanBroadcastScript() = true, this runs on both server AND clients
		IEntity radioTruck = pOwnerEntity.GetParent();
		if (!radioTruck)
		{
			Print("BC Debug - DisableRadioTruck: No parent entity (radio truck) found", LogLevel.ERROR);
			return;
		}

		RplComponent rpl = RplComponent.Cast(radioTruck.FindComponent(RplComponent));
		if (!rpl)
		{
			Print("BC Debug - DisableRadioTruck: No RplComponent found on radio truck", LogLevel.ERROR);
			return;
		}

		if (!rpl.IsMaster())
		{
			Print("BC Debug - DisableRadioTruck: Skipping execution on client, only server handles this", LogLevel.NORMAL);
			return;
		}

		// Prevent multiple executions - check if already disabled
		if (m_radioTruckComponent.GetIsDisabled())
		{
			Print("BC Debug - DisableRadioTruck: Already disabled, skipping", LogLevel.NORMAL);
			return;
		}

		Print("BC Debug - DisableRadioTruck: Executing on server (IsMaster=true)", LogLevel.NORMAL);

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
		// Use deferred initialization to handle race condition where parent isn't attached yet
		GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	void InitComponents(IEntity pOwnerEntity)
	{
		IEntity radioTruck = pOwnerEntity.GetParent();
		if (!radioTruck)
		{
			Print("BC Debug - no parent for disableradiotruck action, retrying...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
			return;
		}

		m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
		if (!m_radioTruckComponent)
		{
			Print("BC Debug - no radio truck component on parent of disableradiotruck action, retrying...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
			return;
		}

		Print("BC Debug - DisableRadioTruck action initialized successfully", LogLevel.NORMAL);
	}
}
