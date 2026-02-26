class GRAD_BC_DestroyRadioTransmission : ScriptedUserAction
{
	// This scripted user action if triggered runs on all clients and server
	// But in code execution is filtered on performing user und server
	
	private GRAD_BC_TransmissionComponent m_transmissionComponent;
	
	private RplComponent m_RplComponent;

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
	    return false; // Changed to false so action runs on server
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
		
		// If we have a linked transmission component, hide the action when it's disabled or done
		if (m_transmissionComponent) {
			ETransmissionState state = m_transmissionComponent.GetTransmissionState();
			if (state == ETransmissionState.DISABLED || state == ETransmissionState.DONE)
				return false;
		}
		
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_transmissionComponent)
			return false;
		
		// Only allow for BLUFOR players
		if (!IsUserBlufor(user))
			return false;
		
		// Don't allow destroying while being dragged
		IEntity ownerEntity = m_transmissionComponent.GetOwner();
		if (ownerEntity)
		{
			GRAD_BC_DraggableComponent draggable = GRAD_BC_DraggableComponent.Cast(ownerEntity.FindComponent(GRAD_BC_DraggableComponent));
			if (draggable && draggable.IsDragged())
				return false;
		}
		
		bool canBeDestroyed = (
			m_transmissionComponent.GetTransmissionState() == ETransmissionState.TRANSMITTING ||
			m_transmissionComponent.GetTransmissionState() == ETransmissionState.INTERRUPTED
		);
		// Check if transmission is in TRANSMITTING or INTERRUPTED state (finished transmissions cannot be destroyed)
		return canBeDestroyed;
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
		if (!m_transmissionComponent)
		{
			Print("BC Debug - m_transmissionComponent is null", LogLevel.ERROR);
			return;
		}

		// This now only runs on server since HasLocalEffectOnlyScript() returns false
		// Get the current position and rotation before destroying
		vector currentPos = pOwnerEntity.GetOrigin();
		vector currentAngles = pOwnerEntity.GetYawPitchRoll();
		
		// Set the transmission component to DISABLED state instead of destroying it
		// This keeps the marker visible during cooldown
		m_transmissionComponent.SetTransmissionState(ETransmissionState.DISABLED);

		// Stop any active transmission on the antenna component itself
		m_transmissionComponent.SetTransmissionActive(false);

		// Also stop the radio truck's own transmission state so the truck action
		// label updates from "Stop Radio Transmission" to "Start Radio Transmission"
		GRAD_BC_BreakingContactManager bcManager = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcManager)
		{
			bcManager.StopRadioTruckTransmission();

			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("BC Debug - Registering destroyed transmission at position %1", currentPos.ToString()), LogLevel.NORMAL);
			bcManager.RegisterDestroyedTransmissionPosition(currentPos);

			// Also register the disabled component for re-enabling after cooldown
			bcManager.RegisterDisabledTransmissionComponent(m_transmissionComponent);
		}
		else
		{
			Print("BC Debug - Could not find Breaking Contact Manager", LogLevel.WARNING);
		}
		
		// Hide the antenna model instead of destroying the entity
		HideAntennaModel(pOwnerEntity);
		
		// Spawn debris pieces around the destroyed antenna (clients will spawn visuals via broadcast RPC)
		SpawnAntennaDebris(currentPos, currentAngles);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - Antenna disabled and hidden on server", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Hide the antenna model by setting its visibility to false
	private void HideAntennaModel(IEntity antennaEntity)
	{
		if (!antennaEntity)
		{
			Print("BC Debug - Cannot hide antenna: entity is null", LogLevel.ERROR);
			return;
		}
		
		// Hide the visual representation of the antenna
		antennaEntity.ClearFlags(EntityFlags.VISIBLE, false);
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - Antenna model hidden", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnAntennaDebris(vector centerPosition, vector angles)
	{
		// Delegate to the transmission component which has RplComponent support for broadcasting RPCs
		if (m_transmissionComponent)
			m_transmissionComponent.BroadcastSpawnDebris(centerPosition, angles);
	}
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_transmissionComponent = GRAD_BC_TransmissionComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_TransmissionComponent));
		
		m_RplComponent = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
	}
}