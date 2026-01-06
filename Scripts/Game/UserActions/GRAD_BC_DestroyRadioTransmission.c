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
		
		// Stop any active transmission
		m_transmissionComponent.SetTransmissionActive(false);
		
		// Register destroyed position for additional spawn prevention
		GRAD_BC_BreakingContactManager bcManager = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcManager)
		{
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
		
		// Spawn debris pieces around the destroyed antenna
		SpawnAntennaDebris(currentPos, currentAngles);
		
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
		
		Print("BC Debug - Antenna model hidden", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnDestroyedAntenna(vector position, vector angles)
	{
		// For now, skip spawning destroyed antenna visual since .xob files can't be spawned directly
		// The antenna destruction effect will be the disappearance of the original antenna
		// and the spawning of debris pieces
		Print("BC Debug - Antenna destroyed (visual destruction skipped - .xob files cannot be spawned as entities)", LogLevel.NORMAL);
		
		// Spawn debris pieces around the destroyed antenna position
		SpawnAntennaDebris(position, angles);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SpawnAntennaDebris(vector centerPosition, vector angles)
	{
		// Use SCR_DebrisSmallEntity for proper debris spawning
		array<string> debrisModels = {
			"{1B6E5E82B4E9805A}Assets/Props/Military/Antennas/Antenna_USSR_02/Dst/Antenna_USSR_02_dst_01.xob",
			"{4EA38F4B91808AA0}Assets/Props/Military/Antennas/Antenna_USSR_02/Dst/Antenna_USSR_02_dst_01_dbr_01.xob",
			"{D7BD36D28C67BB30}Assets/Props/Military/Antennas/Antenna_USSR_02/Dst/Antenna_USSR_02_dst_01_dbr_02.xob",
			"{A0B75E5A78C55440}Assets/Props/Military/Antennas/Antenna_USSR_02/Dst/Antenna_USSR_02_dst_01_dbr_03.xob"
		};
		
		// Spawn 4-6 debris pieces around the antenna
		int debrisCount = debrisModels.Count();
		for (int i = 0; i < debrisCount; i++)
		{
			// Random model selection
			string modelPath = debrisModels[i];
			
			// Calculate random position around the antenna
			float angle = Math.RandomFloatInclusive(0.0, 360.0);
			float distance = Math.RandomFloatInclusive(2.0, 8.0);
			
			float radians = angle * Math.DEG2RAD;
			float offsetX = distance * Math.Cos(radians);
			float offsetZ = distance * Math.Sin(radians);
			
			vector debrisPos = centerPosition + Vector(offsetX, 0, offsetZ);
			debrisPos[1] = GetGame().GetWorld().GetSurfaceY(debrisPos[0], debrisPos[2]) + 0.5; // Lift slightly above ground
			
			// Create transformation matrix for debris
			vector debrisMat[4];
			Math3D.MatrixIdentity4(debrisMat);
			debrisMat[3] = debrisPos;
			
			// Random rotation
			vector debrisAngles = Vector(
				Math.RandomFloatInclusive(0.0, 360.0),  // Yaw
				Math.RandomFloatInclusive(-30.0, 30.0), // Pitch
				Math.RandomFloatInclusive(-30.0, 30.0)  // Roll
			);
			Math3D.AnglesToMatrix(debrisAngles, debrisMat);
			debrisMat[3] = debrisPos; // Restore position after rotation
			
			// Random velocities for realistic physics
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
			
			// Debris parameters
			float debrisMass = Math.RandomFloatInclusive(2.0, 8.0);
			const float debrisLifetime = 30.0; // 30 seconds lifetime
			const float debrisMaxDist = 500.0; // Visible up to 500m
			const int debrisPriority = 3; // Medium priority
			
			// Spawn the debris using the proper system
			SCR_DebrisSmallEntity debris = SCR_DebrisSmallEntity.SpawnDebris(
				GetGame().GetWorld(),
				debrisMat,
				modelPath,
				debrisMass,
				debrisLifetime,
				debrisMaxDist,
				debrisPriority,
				linearVel,
				angularVel,
				"", // No material remap
				false, // Not static (dynamic physics)
				SCR_EMaterialSoundTypeDebris.METAL_LIGHT, // Metal sound type
				true // Exterior source
			);
			
			if (debris)
			{
				Print(string.Format("BC Debug - Debris piece %1 spawned successfully at %2", i, debrisPos.ToString()), LogLevel.NORMAL);
			}
			else
			{
				Print(string.Format("BC Debug - Failed to spawn debris piece %1", i), LogLevel.WARNING);
			}
		}
		
		Print(string.Format("BC Debug - Antenna destruction complete with %1 debris pieces", debrisCount), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_transmissionComponent = GRAD_BC_TransmissionComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_TransmissionComponent));
		
		m_RplComponent = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
	}
}