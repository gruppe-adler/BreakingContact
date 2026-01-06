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
		
		// Don't allow if radio truck is disabled
		if (m_radioTruckComponent && m_radioTruckComponent.GetIsDisabled())
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
		
		if(m_radioTruckComponent.GetTransmissionActive()) {
			m_radioTruckComponent.SetTransmissionActive(false);
			ToggleLampState(pOwnerEntity.GetParent(), false); // Turn off lamps
		}
		else {
			m_radioTruckComponent.SetTransmissionActive(true);
			ToggleLampState(pOwnerEntity.GetParent(), true); // Turn on lamps
		}
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
		} else
		{
			outName = "Start Radio Transmission";
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
	}

	void InitComponents(IEntity pOwnerEntity)
	{
		IEntity radioTruck = pOwnerEntity.GetParent();
		if (!radioTruck)
		{
			Print("BC Debug - no parent for action, retrying...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
			return;
		}

		if (!m_radioTruckComponent)
		{
			m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
			if (!m_radioTruckComponent)
			{
				Print("BC Debug - no radio truck component on parent of action, retrying...", LogLevel.WARNING);
				GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
				return;
			}
		}

		// Check if the SlotManager exists yet (usually yes)
		SlotManagerComponent slotManager = SlotManagerComponent.Cast(radioTruck.FindComponent(SlotManagerComponent));
		if (!slotManager)
		{
			Print("BC Debug - no slot manager on parent of action, retrying...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
			return;
		}

		// Try to get the slot
		EntitySlotInfo slot = slotManager.GetSlotByName("lamp_on");

		// 3. Safety Check: Is the slot there AND is the entity attached?
		if (slot && slot.GetAttachedEntity())
		{
			// FOUND IT! Hide it immediately.
			// (Assuming you want it off by default)
			ToggleLampState(radioTruck, false);
			Print("Lamps initialized and hidden.");
		}
		else
		{
			// NOT FOUND YET.
			// The attachment hasn't spawned. Try again in another 100ms.
			Print("Lamp not ready yet, retrying...");
			GetGame().GetCallqueue().CallLater(InitComponents, 100, false, pOwnerEntity);
		}
	}
	

	void ToggleLampState(IEntity vehicle, bool state)
	{
		SlotManagerComponent slotManager = SlotManagerComponent.Cast(vehicle.FindComponent(SlotManagerComponent));
		
		/*
		array<EntitySlotInfo> allSlots = {};
		slotManager.GetSlotInfos(allSlots);
		
		Print("------------- DEBUG SLOT NAMES -------------");
		foreach (EntitySlotInfo slot : allSlots)
		{
		    string name = slot.GetSourceName();
		    IEntity attached = slot.GetAttachedEntity();
		    string attachedName = "Nothing";
		    if (attached) attachedName = attached.GetName();
		    
		    Print("Slot SourceName: '" + name + "' | Attached Entity: " + attachedName);
		}
		Print("--------------------------------------------");
		*/
		
		if (slotManager)
		{
		    // Try to find the slot info directly by string name
		    EntitySlotInfo slotInfoOn = slotManager.GetSlotByName("lamp_on");
			EntitySlotInfo slotInfoOff = slotManager.GetSlotByName("lamp_off");
		
		    if (slotInfoOn && slotInfoOff)
		    {
		        IEntity lamp_on = slotInfoOn.GetAttachedEntity();
				IEntity lamp_off = slotInfoOff.GetAttachedEntity();
		        if (lamp_on && lamp_off && state)
		        {
		            // Toggle visibility logic here
		            lamp_on.SetFlags(EntityFlags.VISIBLE | EntityFlags.ACTIVE, true);
					lamp_off.ClearFlags(EntityFlags.VISIBLE | EntityFlags.ACTIVE, true);
		            Print("Success: Lamp turned on");
		        }
		        else if (lamp_on && lamp_off && !state)
		        {
					lamp_off.SetFlags(EntityFlags.VISIBLE | EntityFlags.ACTIVE, true);
					lamp_on.ClearFlags(EntityFlags.VISIBLE | EntityFlags.ACTIVE, true);
		             Print("Success: Lamp turned off");
		        }
		    }
		    else
		    {
		        Print("Error: Could not find any slot named 'lamp_on' or 'lamp_off'", LogLevel.ERROR);
		    }
		}

		
	}
}