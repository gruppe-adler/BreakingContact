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
		Print(string.Format("BC DestroyAction - PerformAction called. IsServer=%1, transmissionComp=%2, ownerEntity=%3", Replication.IsServer(), m_transmissionComponent, pOwnerEntity), LogLevel.WARNING);

		if (!m_transmissionComponent)
		{
			Print("BC DestroyAction - ABORT: m_transmissionComponent is null", LogLevel.ERROR);
			return;
		}

		vector currentPos = pOwnerEntity.GetOrigin();
		vector currentAngles = pOwnerEntity.GetYawPitchRoll();
		Print(string.Format("BC DestroyAction - pos=%1 angles=%2", currentPos.ToString(), currentAngles.ToString()), LogLevel.WARNING);

		if (Replication.IsServer())
		{
			Print("BC DestroyAction - Running server-side logic", LogLevel.WARNING);
			m_transmissionComponent.SetTransmissionState(ETransmissionState.DISABLED);
			m_transmissionComponent.SetTransmissionActive(false);

			GRAD_BC_BreakingContactManager bcManager = GRAD_BC_BreakingContactManager.GetInstance();
			if (bcManager)
			{
				bcManager.StopRadioTruckTransmission();
				bcManager.RegisterDestroyedTransmissionPosition(currentPos);
				bcManager.RegisterDisabledTransmissionComponent(m_transmissionComponent);
			}
			else
			{
				Print("BC DestroyAction - Could not find Breaking Contact Manager", LogLevel.ERROR);
			}
		}
		else
		{
			Print("BC DestroyAction - Running client-side logic", LogLevel.WARNING);
		}

		// Play sabotage sound at antenna position (3D, heard by nearby players)
		AudioSystem.PlayEvent("{5D22B0B2ED6D503A}sounds/BC_antennaimpact.acp", "BC_AntennaImpact", currentPos);

		// Hide on all machines — ClearFlags is local and doesn't replicate
		HideAntennaModel(pOwnerEntity);

		Print("BC DestroyAction - Calling SpawnAntennaDebrisLocal", LogLevel.WARNING);
		SpawnAntennaDebrisLocal(currentPos, currentAngles);
	}
	
	//------------------------------------------------------------------------------------------------
	// Hide the antenna model by setting its visibility to false
	private void HideAntennaModel(IEntity antennaEntity)
	{
		if (!antennaEntity)
		{
			Print("BC DestroyAction - Cannot hide antenna: entity is null", LogLevel.ERROR);
			return;
		}

		// Hide root and all children — root may be a proxy so iterate children too
		antennaEntity.ClearFlags(EntityFlags.VISIBLE | EntityFlags.ACTIVE, true);
		Print(string.Format("BC DestroyAction - Root flags after ClearFlags=%1", antennaEntity.GetFlags()), LogLevel.WARNING);

		IEntity child = antennaEntity.GetChildren();
		int count = 0;
		while (child)
		{
			child.ClearFlags(EntityFlags.VISIBLE | EntityFlags.ACTIVE, true);
			Print(string.Format("BC DestroyAction - Child[%1] flags=%2 entity=%3", count, child.GetFlags(), child), LogLevel.WARNING);
			count++;
			child = child.GetSibling();
		}
		Print(string.Format("BC DestroyAction - Hidden root + %1 children", count), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	// Spawns debris visuals locally — called on every machine via the CanBroadcast user action broadcast
	private void SpawnAntennaDebrisLocal(vector centerPosition, vector angles)
	{
		Print(string.Format("BC DestroyAction - SpawnAntennaDebrisLocal called at pos=%1", centerPosition.ToString()), LogLevel.WARNING);

		// Play blow-up sound at debris spawn position (3D, heard by nearby players)
		AudioSystem.PlayEvent("{5D22B0B2ED6D503A}sounds/BC_antennaimpact.acp", "BC_AntennaImpact", centerPosition);

		// Spawn the antenna foot at the antenna's exact position — no scatter, no velocity
		ResourceName footPrefab = "{B212F613254FFE72}Prefabs/Props/Military/Antennas/Dst/Antenna_USSR_02_dst_01.et";
		vector footMat[4];
		Math3D.MatrixIdentity4(footMat);
		Math3D.AnglesToMatrix(angles, footMat);
		footMat[3] = centerPosition;
		EntitySpawnParams footSpawnParams = new EntitySpawnParams();
		footSpawnParams.Transform = footMat;
		IEntity footEntity = GetGame().SpawnEntityPrefab(Resource.Load(footPrefab), GetGame().GetWorld(), footSpawnParams);
		Print(string.Format("BC DestroyAction - Foot spawned=%1 at pos=%2", footEntity != null, centerPosition.ToString()), LogLevel.WARNING);

		// Debris pieces with their height offsets matching the assembled antenna structure:
		// _dbr_01: ground level (0.0), _dbr_02: 1.7m up, _dbr_03: 2.6m up
		array<ResourceName> debrisPrefabs = {
			"{3ADAD3D18B763111}Prefabs/Props/Military/Antennas/Dst/Antenna_USSR_02_dst_01_dbr_01.et",
			"{568EE73137B2BE3F}Prefabs/Props/Military/Antennas/Dst/Antenna_USSR_02_dst_01_dbr_02.et",
			"{B312ABC83B572954}Prefabs/Props/Military/Antennas/Dst/Antenna_USSR_02_dst_01_dbr_03.et"
		};
		array<float> debrisHeightOffsets = { 0.0, 1.7, 2.6 };

		int debrisCount = debrisPrefabs.Count();
		for (int i = 0; i < debrisCount; i++)
		{
			ResourceName prefab = debrisPrefabs[i];

			vector debrisPos = centerPosition + Vector(0, debrisHeightOffsets[i], 0);

			vector debrisMat[4];
			Math3D.MatrixIdentity4(debrisMat);
			vector debrisAngles = Vector(
				Math.RandomFloatInclusive(0.0, 360.0),
				Math.RandomFloatInclusive(-30.0, 30.0),
				Math.RandomFloatInclusive(-30.0, 30.0)
			);
			Math3D.AnglesToMatrix(debrisAngles, debrisMat);
			debrisMat[3] = debrisPos;

			EntitySpawnParams spawnParams = new EntitySpawnParams();
			spawnParams.Transform = debrisMat;
			IEntity debrisEntity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
			Print(string.Format("BC DestroyAction - Debris[%1] spawned=%2 at pos=%3", i, debrisEntity != null, debrisPos.ToString()), LogLevel.WARNING);

			if (debrisEntity)
			{
				Physics phys = debrisEntity.GetPhysics();
				if (phys)
				{
					vector linearVel = Vector(
						Math.RandomFloatInclusive(-3.0, 3.0),
						Math.RandomFloatInclusive(1.0, 5.0),
						Math.RandomFloatInclusive(-3.0, 3.0)
					);
					vector angularVel = Vector(
						Math.RandomFloatInclusive(-180.0, 180.0),
						Math.RandomFloatInclusive(-180.0, 180.0),
						Math.RandomFloatInclusive(-180.0, 180.0)
					);
					phys.SetVelocity(linearVel);
					phys.SetAngularVelocity(angularVel * Math.DEG2RAD);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_transmissionComponent = GRAD_BC_TransmissionComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_TransmissionComponent));
		
		m_RplComponent = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
	}
}