class GRAD_BC_ToggleRadioTransmission : ScriptedUserAction
{
	// This scripted user action if triggered runs on all clients and server
	
	private GRAD_BC_RadioTruckComponent m_radioTruckComponent;

	// comment from discord:
	// if HasLocalEffectOnly returns true, it will be executing only on the client where the action has been trigerred 
	// if HasLocalEffectOnly returns false, then it will be exeucted only on the client where the action has been trigerred and server --> perhaps wrong
	// if HasLocalEffectOnly returns false and CanBroadcast returns true, then it will be exeucted on client where the action has been trigerred and server and everybody else.    
	
	// comment from discord:
	// if HasLocalEffectOnlyScript() TRUE: actions script run only locally.
	// if FALSE:  "CanBeShownScript()" and "CanBePerformedScript()" run locally on client but "PerformAction()" run on server
	    
	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
	    return false;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
	    return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm) return false;

		if (bcm.GetBreakingContactPhase() != EBreakingContactPhase.GAME)
			return false;

		if (!m_radioTruckComponent)
			return false;

		// Hide entirely if disabled
		if (m_radioTruckComponent.GetIsDisabled())
			return false;

		// Always show while transmitting (so player can stop it)
		if (m_radioTruckComponent.GetTransmissionActive())
			return true;

		// Show "Start Transmission" even when blocked by cooldown, so players understand why
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Returns true if the current position is within 1000 m of a DONE or DISABLED transmission point.
	protected bool IsBlockedByCooldown(GRAD_BC_BreakingContactManager bcm)
	{
		if (!m_radioTruckComponent) return false;

		IEntity radioTruck = m_radioTruckComponent.GetOwner();
		if (!radioTruck) return false;

		vector truckPos = radioTruck.GetOrigin();
		array<GRAD_BC_TransmissionComponent> allTransmissions = bcm.GetTransmissionPoints();
		if (!allTransmissions) return false;

		for (int i = 0; i < allTransmissions.Count(); i++)
		{
			GRAD_BC_TransmissionComponent tpc = allTransmissions[i];
			if (!tpc) continue;

			ETransmissionState state = tpc.GetTransmissionState();
			if (state != ETransmissionState.DONE && state != ETransmissionState.DISABLED)
				continue;

			float distance = vector.Distance(truckPos, tpc.GetOwner().GetOrigin());
			if (distance <= 1000.0)
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("BC Debug - Start blocked: within 1000m of DONE/DISABLED point (dist: %1m, state: %2)", distance, state), LogLevel.NORMAL);
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm) return false;
		EBreakingContactPhase currentPhase = bcm.GetBreakingContactPhase();

		// Only allow in GAME phase
		if (currentPhase != EBreakingContactPhase.GAME)
			return false;

		// Don't allow if radio truck is disabled
		if (m_radioTruckComponent && m_radioTruckComponent.GetIsDisabled())
			return false;

		// If radio truck is currently transmitting, allow stopping it
		if (m_radioTruckComponent && m_radioTruckComponent.GetTransmissionActive())
		{
			// Don't allow stopping a DONE transmission
			IEntity radioTruck = m_radioTruckComponent.GetOwner();
			if (radioTruck)
			{
				GRAD_BC_TransmissionComponent activeTPC = bcm.GetNearestTransmissionPoint(radioTruck.GetOrigin(), true);
				if (activeTPC && activeTPC.GetTransmissionState() == ETransmissionState.DONE)
					return false;
			}
			return true;
		}

		// Block starting if within cooldown area - action is shown but greyed out
		if (IsBlockedByCooldown(bcm))
			return false;

		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - PerformAction() ToggleRadioTransmission", LogLevel.NORMAL);

		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return;
		}

		// Toggle transmission state - this now handles antenna animation and lamp state
		// via replication in GRAD_BC_RadioTruckComponent.SetTransmissionActive()
		if (m_radioTruckComponent.GetTransmissionActive())
		{
			m_radioTruckComponent.SetTransmissionActive(false);
		}
		else
		{
			m_radioTruckComponent.SetTransmissionActive(true);
		}

		// Request replication sync to ensure clients get the updated state quickly
		// This helps the action label update faster on the client that performed the action
		m_radioTruckComponent.SyncVariables();
	}
	
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return false;
		}

		if (m_radioTruckComponent.GetIsDisabled())
		{
			outName = "Radio Truck Disabled";
			return true;
		}

		if (m_radioTruckComponent.GetTransmissionActive())
		{
			outName = "Stop Radio Transmission";
			return true;
		}

		// Show reason when blocked by area cooldown
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm && IsBlockedByCooldown(bcm))
		{
			outName = "Start Radio Transmission (Area Cooldown)";
			return true;
		}

		outName = "Start Radio Transmission";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	private int m_iInitRetryCount = 0;
	private const int MAX_INIT_RETRIES = 50;

	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
	}

	void InitComponents(IEntity pOwnerEntity)
	{
		IEntity radioTruck = pOwnerEntity.GetParent();
		if (!radioTruck)
		{
			m_iInitRetryCount++;
			if (m_iInitRetryCount < MAX_INIT_RETRIES)
			{
				Print("BC Debug - no parent for action, retrying...", LogLevel.WARNING);
				GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
			}
			else
			{
				Print("BC Debug - ToggleRadioTransmission: Max init retries reached, giving up", LogLevel.WARNING);
			}
			return;
		}

		if (!m_radioTruckComponent)
		{
			m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
			if (!m_radioTruckComponent)
			{
				m_iInitRetryCount++;
				if (m_iInitRetryCount < MAX_INIT_RETRIES)
				{
					Print("BC Debug - no radio truck component on parent of action, retrying...", LogLevel.WARNING);
					GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
				}
				else
				{
					Print("BC Debug - ToggleRadioTransmission: Max init retries reached, giving up", LogLevel.WARNING);
				}
				return;
			}
		}

		// Check if the SlotManager exists yet (usually yes)
		SlotManagerComponent slotManager = SlotManagerComponent.Cast(radioTruck.FindComponent(SlotManagerComponent));
		if (!slotManager)
		{
			m_iInitRetryCount++;
			if (m_iInitRetryCount < MAX_INIT_RETRIES)
			{
				Print("BC Debug - no slot manager on parent of action, retrying...", LogLevel.WARNING);
				GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
			}
			else
			{
				Print("BC Debug - ToggleRadioTransmission: Max init retries reached, giving up", LogLevel.WARNING);
			}
			return;
		}

		// Try to get the slot
		EntitySlotInfo slot = slotManager.GetSlotByName("lamp_on");

		// Safety Check: Is the slot there AND is the entity attached?
		if (slot && slot.GetAttachedEntity())
		{
			// Initialize lamp state to off by default using the component's method
			// This ensures consistent state management through replication
			m_radioTruckComponent.ApplyLampState(false);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BC Debug - Lamps initialized and hidden via component.");
		}
		else
		{
			m_iInitRetryCount++;
			if (m_iInitRetryCount < MAX_INIT_RETRIES)
			{
				// NOT FOUND YET.
				// The attachment hasn't spawned. Try again in another 100ms.
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("BC Debug - Lamp not ready yet, retrying...");
				GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
			}
			else
			{
				Print("BC Debug - ToggleRadioTransmission: Max init retries reached, giving up", LogLevel.WARNING);
			}
		}
	}
}