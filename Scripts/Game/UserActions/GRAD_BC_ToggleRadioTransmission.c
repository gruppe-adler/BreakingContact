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
		return CanBePerformedScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm) return false;
		EBreakingContactPhase currentPhase = bcm.GetBreakingContactPhase();

		// Only allow in GAME phase (adjust if your "started" phase is different)
		if (currentPhase != EBreakingContactPhase.GAME)
			return false;

		// If radio truck is currently transmitting, allow stopping it
		if (m_radioTruckComponent && m_radioTruckComponent.GetTransmissionActive()) {
			// Get the currently active transmission to check its state
			IEntity radioTruck = m_radioTruckComponent.GetOwner();
			if (radioTruck) {
				GRAD_BC_TransmissionComponent activeTPC = bcm.GetNearestTransmissionPoint(radioTruck.GetOrigin(), true);
				if (activeTPC && activeTPC.GetTransmissionState() == ETransmissionState.DONE) {
					return false; // Don't allow stopping a DONE transmission
				}
			}
			return true; // Allow stopping active transmission
		}
		
		// If radio truck is not transmitting, check distance to existing transmission points
		if (m_radioTruckComponent) {
			IEntity radioTruck = m_radioTruckComponent.GetOwner();
			if (radioTruck) {
				// Check if there's any transmission point within 1000m
				vector truckPos = radioTruck.GetOrigin();
				array<GRAD_BC_TransmissionComponent> allTransmissions = bcm.GetTransmissionPoints();
				if (allTransmissions) {
					for (int i = 0; i < allTransmissions.Count(); i++) {
						GRAD_BC_TransmissionComponent tpc = allTransmissions[i];
						if (tpc) {
							vector transmissionPos = tpc.GetOwner().GetOrigin();
							float distance = vector.Distance(truckPos, transmissionPos);
							ETransmissionState state = tpc.GetTransmissionState();
							
							// Only prevent starting new transmission if within 1000m of DONE or DISABLED transmission points
							// Allow starting near INTERRUPTED (state 2) or OFF (state 0) transmission points
							if (distance <= 1000.0 && (state == ETransmissionState.DONE || state == ETransmissionState.DISABLED)) {
								Print(string.Format("BC Debug - Cannot start transmission: Too close to DONE/DISABLED transmission point (distance: %1m, state: %2)", distance, state), LogLevel.NORMAL);
								return false;
							}
						}
					}
				}
			}
		}
		
		// Allow starting new transmission if no nearby transmission points
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		Print("BC Debug - PerformAction() ToggleRadioTransmission", LogLevel.NORMAL);

		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return;
		}
		
		if(m_radioTruckComponent.GetTransmissionActive())
			m_radioTruckComponent.SetTransmissionActive(false);
		else
			m_radioTruckComponent.SetTransmissionActive(true);
	}
	
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return false;
		}
		
		if (m_radioTruckComponent.GetTransmissionActive())
		{
			outName = "Stop Radio Transmission";
		} else
		{
			outName = "Start Radio Transmission";
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_RadioTruckComponent));
	}
}