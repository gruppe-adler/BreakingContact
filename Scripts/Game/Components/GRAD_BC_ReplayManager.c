[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Manages replay recording and playback for Breaking Contact gamemode")]
class GRAD_BC_ReplayManagerClass : ScriptComponentClass
{
}

// Replay data structures moved to GRAD_BC_ReplayData.c

// Main replay manager component
class GRAD_BC_ReplayManager : ScriptComponent
{
	[Attribute("1.0", UIWidgets.EditBox, "Recording interval in seconds")]
	protected float m_fRecordingInterval;
	
	[Attribute("1", UIWidgets.CheckBox, "Record projectiles")]
	protected bool m_bRecordProjectiles;
	
	[Attribute("500.0", UIWidgets.EditBox, "Max projectile recording distance")]
	protected float m_fMaxProjectileDistance;
	
	// Recording state
	protected bool m_bIsRecording = false;
	protected bool m_bIsPlayingBack = false;
	protected ref GRAD_BC_ReplayData m_replayData;
	protected float m_fLastRecordTime = 0;
	
	// Playback state
	protected float m_fPlaybackStartTime = 0;
	protected float m_fCurrentPlaybackTime = 0;
	protected int m_iCurrentFrameIndex = 0;
	protected bool m_bPlaybackPaused = false;
	protected float m_fPlaybackSpeed = 5.0; // 1.0 = normal speed, 0.5 = half speed, 2.0 = double speed, 5.0 = 5x speed
	
	// Adaptive replay settings - max 2 minutes replay time
	protected float m_fAdaptiveSpeed = 1.0; // Calculated speed to fit in max duration
	protected float m_fMaxReplayDuration = 60.0; // 2 minutes max replay time
	
	// Projectile data pending recording
	protected ref array<ref GRAD_BC_ProjectileData> m_pendingProjectiles = {};
	
	// Tracked vehicles
	protected ref array<IEntity> m_trackedVehicles;
	
	// RPC for client communication
	protected RplComponent m_RplComponent;
	
	// Static instance for global access
	protected static GRAD_BC_ReplayManager s_Instance;
	
	//------------------------------------------------------------------------------------------------
	static GRAD_BC_ReplayManager GetInstance()
	{
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	// Public method to register a vehicle for replay tracking
	void RegisterTrackedVehicle(Vehicle vehicle)
	{
		if (!vehicle)
			return;

		if (!m_trackedVehicles)
			m_trackedVehicles = new array<IEntity>();

		if (m_trackedVehicles.Find(vehicle) == -1)
		{
			m_trackedVehicles.Insert(vehicle);
			Print(string.Format("GRAD_BC_ReplayManager: Registered vehicle for tracking: %1 (total: %2)", vehicle.GetPrefabData().GetPrefabName(), m_trackedVehicles.Count()), LogLevel.NORMAL);
		}
		else
		{
			Print(string.Format("GRAD_BC_ReplayManager: Vehicle already tracked: %1", vehicle.GetPrefabData().GetPrefabName()), LogLevel.VERBOSE);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		// VERSION IDENTIFIER - This log will change when code updates
		Print("GRAD_BC_ReplayManager: VERSION 2025-08-20-14:30 - Single-player detection enabled", LogLevel.NORMAL);
		
		// Force initialization on both server and client
		if (!s_Instance)
		{
			s_Instance = this;
			string isServer = "Client";
			if (Replication.IsServer()) { isServer = "Server"; }
			Print(string.Format("GRAD_BC_ReplayManager: Force initialized via EOnInit on %1", isServer), LogLevel.NORMAL);
		}
		
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		// Always initialize on both server and client
		super.OnPostInit(owner);
		
		s_Instance = this;
		
		m_trackedVehicles = new array<IEntity>();
		
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		Print(string.Format("GRAD_BC_ReplayManager: Instance created and initialized on %1", isServer), LogLevel.NORMAL);
		
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!m_RplComponent)
		{
			Print(string.Format("GRAD_BC_ReplayManager: Warning - No RplComponent found on %1", isServer), LogLevel.WARNING);
		}
		else
		{
			Print(string.Format("GRAD_BC_ReplayManager: RplComponent found on %1", isServer), LogLevel.NORMAL);
		}
		
		// Only record on server
		if (Replication.IsServer())
		{
			Print("GRAD_BC_ReplayManager: Running on server, setting up recording", LogLevel.NORMAL);
			
			// Delay initialization to ensure world is ready
			GetGame().GetCallqueue().CallLater(InitializeReplaySystem, 100, false);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: Running on client, ready to receive RPCs", LogLevel.NORMAL);
			// Initialize client-side replay manager to receive RPCs
			s_Instance = this;
			
			// Add debug info
			if (m_RplComponent)
			{
				Print("GRAD_BC_ReplayManager: Client has RplComponent, RPCs should work", LogLevel.NORMAL);
			}
			else
			{
				Print("GRAD_BC_ReplayManager: Client missing RplComponent, RPCs will not work!", LogLevel.ERROR);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void InitializeReplaySystem()
	{
		Print("GRAD_BC_ReplayManager: Initializing replay system", LogLevel.NORMAL);
		
		// Initialize replay data now that world should be ready
		m_replayData = GRAD_BC_ReplayData.Create();
		
		// Connect to Breaking Contact Manager events
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
		{
			Print("GRAD_BC_ReplayManager: BCM found, starting game state monitoring", LogLevel.NORMAL);
			// Start recording when game phase begins
			GetGame().GetCallqueue().CallLater(CheckGameState, 1000, true);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: BCM not found during initialization", LogLevel.WARNING);
		}
		
		SCR_CampaignBuildingManagerComponent buildingManager = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
		if (buildingManager)
		{
			buildingManager.GetOnEntitySpawnedByProvider().Insert(OnVehicleSpawned);
			Print("GRAD_BC_ReplayManager: Hooked into vehicle spawn event", LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: Building manager not found, cannot track spawned vehicles", LogLevel.WARNING);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OnVehicleSpawned(int prefabID, SCR_EditableEntityComponent editableEntity, int playerId, SCR_CampaignBuildingProviderComponent provider)
	{
		if (!editableEntity)
			return;
	
		IEntity spawnedEntity = editableEntity.GetOwner();
		if (!spawnedEntity)
			return;

		Print(string.Format("GRAD_BC_ReplayManager: Entity spawned: %1", spawnedEntity.GetPrefabData().GetPrefabName()), LogLevel.NORMAL);

		Vehicle vehicle = Vehicle.Cast(spawnedEntity);
		if (vehicle)
		{
			if (!m_trackedVehicles)
				m_trackedVehicles = new array<IEntity>();
			
			if (m_trackedVehicles.Find(vehicle) == -1)
			{
				m_trackedVehicles.Insert(vehicle);
				Print(string.Format("GRAD_BC_ReplayManager: Tracked new vehicle: %1 (total: %2)", vehicle.GetPrefabData().GetPrefabName(), m_trackedVehicles.Count()), LogLevel.NORMAL);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CheckGameState()
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
		{
			Print("GRAD_BC_ReplayManager: BCM instance not found", LogLevel.WARNING);
			return;
		}
			
		EBreakingContactPhase currentPhase = bcm.GetBreakingContactPhase();
		Print(string.Format("GRAD_BC_ReplayManager: Current phase: %1, Recording: %2", 
			currentPhase, m_bIsRecording), LogLevel.NORMAL);
			
		// Start recording when game begins
		if (!m_bIsRecording && currentPhase == EBreakingContactPhase.GAME)
		{
			Print("GRAD_BC_ReplayManager: Game phase detected, starting recording", LogLevel.NORMAL);
			StartRecording();
		}
		
		// Stop recording and start playback when game ends
		if (m_bIsRecording && currentPhase == EBreakingContactPhase.GAMEOVER)
		{
			Print("GRAD_BC_ReplayManager: GAMEOVER phase detected, stopping recording", LogLevel.NORMAL);
			StopRecording();
			GetGame().GetCallqueue().CallLater(StartPlaybackForAllClients, 500, false); // Reduced from 3000ms to 500ms
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void StartRecording()
	{
		if (m_bIsRecording || !Replication.IsServer())
			return;
			
		// Ensure replay data is initialized
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data not initialized, creating now", LogLevel.WARNING);
			m_replayData = GRAD_BC_ReplayData.Create();
		}
		
		// Ensure world is available
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("GRAD_BC_ReplayManager: World not available, cannot start recording", LogLevel.ERROR);
			return;
		}
			
		Print("GRAD_BC_ReplayManager: Starting replay recording", LogLevel.NORMAL);
		m_bIsRecording = true;
		m_fLastRecordTime = world.GetWorldTime() / 1000.0; // Convert milliseconds to seconds
		
		// Start recording loop
		GetGame().GetCallqueue().CallLater(RecordFrame, m_fRecordingInterval * 1000, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void StopRecording()
	{
		if (!m_bIsRecording)
			return;
			
		Print("GRAD_BC_ReplayManager: Stopping replay recording", LogLevel.NORMAL);
		m_bIsRecording = false;
		GetGame().GetCallqueue().Remove(RecordFrame);
		
		// Finalize replay data
		if (m_replayData && m_replayData.frames.Count() > 0)
		{
			// Remove empty frames (frames with no players/markers)
			CleanupEmptyFrames();
			
			GRAD_BC_ReplayFrame lastFrame = m_replayData.frames[m_replayData.frames.Count() - 1];
			m_replayData.totalDuration = lastFrame.timestamp - m_replayData.startTime;
			
			Print(string.Format("GRAD_BC_ReplayManager: Recorded %1 frames over %2 seconds", 
				m_replayData.frames.Count(), m_replayData.totalDuration), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CleanupEmptyFrames()
	{
		if (!m_replayData || m_replayData.frames.Count() == 0)
			return;
		
		int originalCount = m_replayData.frames.Count();
		array<ref GRAD_BC_ReplayFrame> cleanedFrames = new array<ref GRAD_BC_ReplayFrame>();
		
		foreach (GRAD_BC_ReplayFrame frame : m_replayData.frames)
		{
			// Keep frame if it has any data
			if (frame.players.Count() > 0 || frame.projectiles.Count() > 0 || frame.transmissions.Count() > 0 || frame.vehicles.Count() > 0)
			{
				cleanedFrames.Insert(frame);
			}
		}
		
		// Replace with cleaned frames
		m_replayData.frames = cleanedFrames;
		
		int removedCount = originalCount - cleanedFrames.Count();
		if (removedCount > 0)
		{
			Print(string.Format("GRAD_BC_ReplayManager: Removed %1 empty frames (%2 -> %3)", 
				removedCount, originalCount, cleanedFrames.Count()), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void RecordFrame()
	{
		if (!m_bIsRecording || !Replication.IsServer())
			return;
			
		// Ensure replay data exists
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data missing during recording, stopping", LogLevel.ERROR);
			StopRecording();
			return;
		}
		
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("GRAD_BC_ReplayManager: World not available during recording", LogLevel.ERROR);
			return;
		}
		
		float currentTime = world.GetWorldTime() / 1000.0; // Convert milliseconds to seconds
		GRAD_BC_ReplayFrame frame = GRAD_BC_ReplayFrame.Create(currentTime);
		
		// Record player positions
		RecordPlayers(frame);
		
		RecordTrackedVehicles(frame);
		
		// Record projectiles if enabled
		if (m_bRecordProjectiles)
		{
			RecordProjectiles(frame);
		}
		
		// Record transmission states
		RecordTransmissions(frame);
		
		// Record radio truck
		RecordRadioTruck(frame);
		
		// Add frame to replay data
		m_replayData.frames.Insert(frame);
		m_fLastRecordTime = currentTime;
	}
	
	//------------------------------------------------------------------------------------------------
	void RecordTrackedVehicles(GRAD_BC_ReplayFrame frame)
	{
		if (!m_trackedVehicles)
			return;
			
		Print(string.Format("GRAD_BC_ReplayManager: Recording %1 tracked vehicles", m_trackedVehicles.Count()), LogLevel.NORMAL);
			
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		IEntity radioTruck = null;
		if (bcm)
			radioTruck = bcm.GetRadioTruck();
	
		foreach (IEntity entity : m_trackedVehicles)
		{
			if (!entity)
				continue;
			
			Vehicle vehicle = Vehicle.Cast(entity);
			if (!vehicle)
				continue;
			
			if (vehicle == radioTruck)
			{
				Print(string.Format("GRAD_BC_ReplayManager: Skipping radio truck, already handled"), LogLevel.NORMAL);
				continue;
			}
	
			// Check if a player is in this vehicle. If so, it's already recorded by RecordPlayers.
			bool playerInThisVehicle = false;
			PlayerManager playerManager = GetGame().GetPlayerManager();
			if (playerManager)
			{
				array<int> playerIds = {};
				playerManager.GetPlayers(playerIds);
				foreach (int playerId : playerIds)
				{
					IEntity controlledChar = playerManager.GetPlayerControlledEntity(playerId);
					if (!controlledChar) continue;
					
					SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(controlledChar);
					if (character)
					{
						Print(string.Format("[ReplayDebug] Checking playerId=%1, character=%2, IsInVehicle=%3", playerId, character, character.IsInVehicle()), LogLevel.NORMAL);
						if (character.IsInVehicle())
						{
							IEntity parent = character.GetParent();
							Print(string.Format("[ReplayDebug]   Parent=%1, Vehicle=%2", parent, vehicle), LogLevel.NORMAL);
							if (parent && parent == vehicle)
							{
								Print(string.Format("[ReplayDebug]   Player %1 is in this vehicle!", playerId), LogLevel.NORMAL);
								playerInThisVehicle = true;
								break;
							}
						}
					}
				}
			}
			if (playerInThisVehicle)
			{
				Print(string.Format("GRAD_BC_ReplayManager: Skipping vehicle %1, player is inside", vehicle.GetPrefabData().GetPrefabName()), LogLevel.NORMAL);
				continue;
			}
	
			// Now we have an empty vehicle. Record it.
			vector position = vehicle.GetOrigin();
			vector angles = vehicle.GetYawPitchRoll();
			string vehicleType = vehicle.GetPrefabData().GetPrefabName();
			
			FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
			string factionKey = "";
			if (factionComponent)
				factionKey = factionComponent.GetAffiliatedFaction().GetFactionKey();
				
			RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
			if (!rpl)
				continue;

			Print(string.Format("GRAD_BC_ReplayManager: Creating snapshot for empty vehicle: %1", vehicleType), LogLevel.NORMAL);

			GRAD_BC_VehicleSnapshot snapshot = GRAD_BC_VehicleSnapshot.Create(
				rpl.Id(), 
				vehicleType, 
				factionKey,
				position, 
				angles,
				!playerInThisVehicle
			);
			
			frame.vehicles.Insert(snapshot);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void RecordPlayers(GRAD_BC_ReplayFrame frame)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;
			
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		
		foreach (int playerId : playerIds)
		{
			IEntity controlledEntity = playerManager.GetPlayerControlledEntity(playerId);
			if (!controlledEntity)
				continue;
				
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(controlledEntity);
			if (!character)
				continue;
				
			string playerName = playerManager.GetPlayerName(playerId);
			string factionKey = character.GetFactionKey();
			vector position = controlledEntity.GetOrigin();
			vector angles = controlledEntity.GetYawPitchRoll();
			bool isAlive = !character.GetCharacterController().IsDead();
			
			// Use ChimeraCharacter's native IsInVehicle() function
			bool isInVehicle = character.IsInVehicle();
			string vehicleType = "";
			
			// Get vehicle details if in vehicle
			if (isInVehicle)
			{
				CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
				if (compartmentAccess)
				{
					BaseCompartmentSlot compartment = compartmentAccess.GetCompartment();
					if (compartment)
					{
						Vehicle vehicle = Vehicle.Cast(compartment.GetOwner());
						if (vehicle)
						{
							vehicleType = vehicle.GetPrefabData().GetPrefabName();
							// Use vehicle position/rotation instead of character
							position = vehicle.GetOrigin();
							angles = vehicle.GetYawPitchRoll();
							
							Print(string.Format("GRAD_BC_ReplayManager: Player %1 is in vehicle: %2", playerName, vehicleType), LogLevel.NORMAL);
						}
					}
				}
			}
			
			// Determine unit role based on equipment
			string unitRole = DetermineUnitRole(character);
			
			GRAD_BC_PlayerSnapshot snapshot = GRAD_BC_PlayerSnapshot.Create(
				playerId, playerName, factionKey, position, angles, isAlive, isInVehicle, vehicleType, unitRole
			);

			// Debug: Log vehicle state for each player (first 20 only)
			static int vehicleRecordDebugCount = 0;
			vehicleRecordDebugCount++;
			if (vehicleRecordDebugCount <= 20)
			{
				Print(string.Format(
					"GRAD_BC_ReplayManager: PlayerRecord id=%1 name=%2 isInVehicle=%3 vehicleType='%4' unitRole='%5'", 
					playerId, playerName, isInVehicle, vehicleType, unitRole
				), LogLevel.NORMAL);
			}
			
			// Debug: Log recording positions and angles for first few frames
			static int recordLogCount = 0;
			recordLogCount++;
			if (recordLogCount <= 10)
			{
				Print(string.Format("GRAD_BC_ReplayManager: Recording player %1 (%2) at position [%3, %4, %5], yaw=%6°", 
					playerId, playerName, position[0], position[1], position[2], angles[0]));
			}
			
			frame.players.Insert(snapshot);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Determine unit role based on equipment and components
	string DetermineUnitRole(SCR_ChimeraCharacter character)
	{
		if (!character)
			return "Rifleman";
			
		// Check for medic role - has medical supplies
		InventoryStorageManagerComponent inventoryManager = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (inventoryManager)
		{
			array<IEntity> items = {};
			inventoryManager.GetItems(items);
			foreach (IEntity item : items)
			{
				// Check for medical items
				SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
				if (consumable)
				{
					SCR_ConsumableEffectHealthItems healthEffect = SCR_ConsumableEffectHealthItems.Cast(consumable.GetConsumableEffect());
					if (healthEffect)
						return "Medic";
				}
			}
		}
		
		// Check weapon type using CharacterControllerComponent
		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller)
		{
			BaseWeaponManagerComponent weaponManager = controller.GetWeaponManagerComponent();
			if (weaponManager)
			{
				BaseWeaponComponent currentWeapon = weaponManager.GetCurrentWeapon();
				if (currentWeapon)
				{
					// Get weapon entity to check type
					IEntity weaponEntity = currentWeapon.GetOwner();
					if (weaponEntity)
					{
						string weaponName = weaponEntity.GetPrefabData().GetPrefabName();
						weaponName.ToLower();
						
						// Check weapon type based on name patterns
						if (weaponName.Contains("m249") || weaponName.Contains("mg") || weaponName.Contains("pkm") || weaponName.Contains("machinegun"))
							return "MachineGunner";
						if (weaponName.Contains("m72") || weaponName.Contains("rpg") || weaponName.Contains("at4") || weaponName.Contains("law") || weaponName.Contains("rocket"))
							return "AntiTank";
						if (weaponName.Contains("m21") || weaponName.Contains("svd") || weaponName.Contains("sniper"))
							return "Sharpshooter";
						if (weaponName.Contains("m203") || weaponName.Contains("gp25") || weaponName.Contains("grenade"))
							return "Grenadier";
					}
				}
			}
		}
		
		// Default to rifleman
		return "Rifleman";
	}
	
	//------------------------------------------------------------------------------------------------
	void RecordProjectiles(GRAD_BC_ReplayFrame frame)
	{
		// Process pending projectiles fired since last frame
		for (int i = 0; i < m_pendingProjectiles.Count(); i++)
		{
			GRAD_BC_ProjectileData projData = m_pendingProjectiles[i];
			
			// Calculate projectile position at current frame time using ballistics
			float deltaTime = frame.timestamp - projData.fireTime;
			if (deltaTime < 0) deltaTime = 0; // Safety check
			
			// Calculate current position and velocity
			vector currentPos = projData.position + (projData.velocity * deltaTime);
			currentPos[1] = currentPos[1] - (9.81 * deltaTime * deltaTime * 0.5);
			
			vector currentVel = projData.velocity;
			currentVel[1] = currentVel[1] - (9.81 * deltaTime);
			
			// Create projectile snapshot with firing position and current/impact position
			GRAD_BC_ProjectileSnapshot snapshot = GRAD_BC_ProjectileSnapshot.Create(
				projData.ammoType, 
				projData.position, // firing position
				currentPos, // current/impact position
				currentVel,
				10.0 - deltaTime // Approximate TTL
			);
			
			frame.projectiles.Insert(snapshot);
		}
		
		// Clear pending projectiles after processing
		m_pendingProjectiles.Clear();
		
	}
	
	//------------------------------------------------------------------------------------------------
	void RecordTransmissions(GRAD_BC_ReplayFrame frame)
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
			return;
			
		// Get all transmission points from the game mode
		array<GRAD_BC_TransmissionComponent> transmissionPoints = bcm.GetTransmissionPoints();
		if (!transmissionPoints || transmissionPoints.Count() == 0)
			return;
			
		foreach (GRAD_BC_TransmissionComponent transmission : transmissionPoints)
		{
			if (!transmission)
				continue;
				
			IEntity owner = transmission.GetOwner();
			if (!owner)
				continue;
				
			vector position = owner.GetOrigin();
			ETransmissionState state = transmission.GetTransmissionState();
			float progress = transmission.GetTransmissionDuration(); // actually returns progress
			
			GRAD_BC_TransmissionSnapshot snapshot = GRAD_BC_TransmissionSnapshot.Create(
				position,
				state,
				progress
			);
			
			frame.transmissions.Insert(snapshot);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void RecordRadioTruck(GRAD_BC_ReplayFrame frame)
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
			return;
			
		IEntity radioTruck = bcm.GetRadioTruck();
		if (!radioTruck)
			return;
			
		// Get radio truck component to check if transmitting
		GRAD_BC_RadioTruckComponent rtc = GRAD_BC_RadioTruckComponent.Cast(radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
		bool isActive = false;
		bool isDestroyed = false;
		
		if (rtc)
		{
			isActive = rtc.GetTransmissionActive();
			isDestroyed = rtc.GetIsDisabled();
		}
		
		// Get position and orientation
		vector position = radioTruck.GetOrigin();
		vector angles = radioTruck.GetAngles();
		
		GRAD_BC_RadioTruckSnapshot snapshot = GRAD_BC_RadioTruckSnapshot.Create(
			position,
			angles,
			isActive,
			isDestroyed
		);
		
		frame.radioTrucks.Insert(snapshot);
	}
	
	//------------------------------------------------------------------------------------------------
	void StartPlaybackForAllClients()
	{
		if (!Replication.IsServer())
			return;
			
		if (!m_replayData || m_replayData.frames.Count() == 0)
		{
			Print("GRAD_BC_ReplayManager: No replay data to send", LogLevel.WARNING);
			return;
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: Starting replay transmission, %1 frames", m_replayData.frames.Count()), LogLevel.NORMAL);
	
	// Check if we have a local player controller (null on dedicated server)
	PlayerController playerController = GetGame().GetPlayerController();
	bool isDedicatedServer = (playerController == null);
	
	PrintFormat("GRAD_BC_ReplayManager: HasPlayerController=%1, IsDedicated=%2", playerController != null, isDedicatedServer);
	
	if (!isDedicatedServer)
	{
		// We have a local player - use direct local playback
		Print("GRAD_BC_ReplayManager: Local player detected, starting direct local playback", LogLevel.NORMAL);
		StartLocalReplayPlayback();
	}
	else
	{
		// No local player - dedicated server - use RPC to send replay to clients
			
			// Check if we have RPC component for multiplayer
			if (!m_RplComponent)
			{
				Print("GRAD_BC_ReplayManager: No RPC component available for transmission", LogLevel.ERROR);
				return;
			}
			
			// Send basic replay info first
			Print("GRAD_BC_ReplayManager: Sending replay initialization RPC", LogLevel.NORMAL);
			Rpc(RpcAsk_StartReplayPlayback, m_replayData.totalDuration, m_replayData.missionName, m_replayData.mapName, m_replayData.startTime);
			
			// Send frames in chunks to avoid network limits
			const int chunkSize = 10; // Send 10 frames at a time
			for (int i = 0; i < m_replayData.frames.Count(); i += chunkSize)
			{
				int endIndex = Math.Min(i + chunkSize, m_replayData.frames.Count());
				Print(string.Format("GRAD_BC_ReplayManager: Sending frame chunk %1-%2", i, endIndex-1), LogLevel.NORMAL);
				SendFrameChunk(i, endIndex);
			}
			
			// Send completion signal
			Print("GRAD_BC_ReplayManager: Sending completion RPC", LogLevel.NORMAL);
			Rpc(RpcAsk_ReplayDataComplete);
			
			// Schedule automatic endscreen broadcast after replay duration
			float replayDuration = m_replayData.totalDuration;
			float waitTime = (replayDuration + 2.0) * 1000; // Add 2 second buffer, convert to milliseconds
			Print(string.Format("GRAD_BC_ReplayManager: DEDICATED SERVER - Scheduling endscreen in %.1f seconds (%.0fms)", waitTime / 1000, waitTime), LogLevel.NORMAL);
			Print(string.Format("GRAD_BC_ReplayManager: Replay duration: %.2fs, buffer: 2s, total wait: %.2fs", replayDuration, waitTime / 1000), LogLevel.NORMAL);
			GetGame().GetCallqueue().CallLater(TriggerEndscreen, waitTime, false);
			Print("GRAD_BC_ReplayManager: CallLater scheduled for TriggerEndscreen", LogLevel.NORMAL);
		}
	}

//------------------------------------------------------------------------------------------------
void StartLocalReplayPlayback()
{
	Print("GRAD_BC_ReplayManager: ========== Starting local single-player replay playback ==========", LogLevel.NORMAL);
		
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: CRITICAL ERROR - No replay data available!", LogLevel.ERROR);
			return;
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: Replay has %1 frames, duration: %.2f seconds", 
			m_replayData.frames.Count(), m_replayData.totalDuration), LogLevel.NORMAL);
		
		// Calculate adaptive playback speed to fit within 2 minutes
		CalculateAdaptiveSpeed();
		
		// Open debriefing screen with global VoN (cross-faction voice)
		Print("GRAD_BC_ReplayManager: Scheduling debriefing screen open in 100ms", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(OpenMapForLocalPlayback, 100, false);
		
		// Initialize playback state
		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			m_fPlaybackStartTime = world.GetWorldTime() / 1000.0;
			Print(string.Format("GRAD_BC_ReplayManager: Playback start time set to %.2f", m_fPlaybackStartTime), LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: WARNING - World not available yet, will initialize timing later", LogLevel.WARNING);
			m_fPlaybackStartTime = 0;
		}
		
		m_fCurrentPlaybackTime = 0;
		m_iCurrentFrameIndex = 0;
		m_bPlaybackPaused = false;
		
		// Start playback after map opens
		Print("GRAD_BC_ReplayManager: Scheduling actual playback start in 800ms", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(StartActualPlayback, 800, false); // Reduced from 2000ms to 800ms
		
		// Schedule automatic endscreen for local mode
		float replayDuration = m_replayData.totalDuration;
		float waitTime = (replayDuration + 2.0) * 1000;
		Print(string.Format("GRAD_BC_ReplayManager: LOCAL MODE - Scheduling endscreen in %.1f seconds (%.0fms)", waitTime / 1000, waitTime), LogLevel.NORMAL);
		Print(string.Format("GRAD_BC_ReplayManager: Replay duration: %.2fs, buffer: 2s, total wait: %.2fs", replayDuration, waitTime / 1000), LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(TriggerEndscreen, waitTime, false);
		Print("GRAD_BC_ReplayManager: CallLater scheduled for TriggerEndscreen", LogLevel.NORMAL);
		
		Print("GRAD_BC_ReplayManager: Local playback initialization complete", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void StartActualPlayback()
	{
		Print("GRAD_BC_ReplayManager: ===== StartActualPlayback CALLED =====", LogLevel.NORMAL);
		Print("GRAD_BC_ReplayManager: Starting actual playback sequence", LogLevel.NORMAL);
		
		// Verify world is available
		Print("GRAD_BC_ReplayManager: Checking if world is available...", LogLevel.NORMAL);
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("GRAD_BC_ReplayManager: World not available for playback, retrying in 500ms", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(StartActualPlayback, 500, false);
			return;
		}
		
		Print("GRAD_BC_ReplayManager: World is available, proceeding...", LogLevel.NORMAL);
		
		// Reset timing
		m_fPlaybackStartTime = world.GetWorldTime() / 1000.0;
		Print(string.Format("GRAD_BC_ReplayManager: Playback start time set to: %.2f", m_fPlaybackStartTime), LogLevel.NORMAL);
		
		// Enable playback flag - CRITICAL for UpdatePlayback to run
		Print("GRAD_BC_ReplayManager: Setting m_bIsPlayingBack to TRUE", LogLevel.NORMAL);
		m_bIsPlayingBack = true;
		Print(string.Format("GRAD_BC_ReplayManager: m_bIsPlayingBack is now: %1", m_bIsPlayingBack), LogLevel.NORMAL);
		
		// Skip empty frames at start
		SkipEmptyFrames();
		
		Print("GRAD_BC_ReplayManager: About to start playback loop", LogLevel.NORMAL);
		Print("GRAD_BC_ReplayManager: Starting playback loop", LogLevel.NORMAL);
		
		// Start playback loop
		Print("GRAD_BC_ReplayManager: Calling GetGame().GetCallqueue().CallLater(UpdatePlayback, 100, true)", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(UpdatePlayback, 100, true);
		Print("GRAD_BC_ReplayManager: CallLater executed successfully", LogLevel.NORMAL);
		
		// Show replay controls
		Print("GRAD_BC_ReplayManager: About to show replay controls", LogLevel.NORMAL);
		ShowReplayControls();
		Print("GRAD_BC_ReplayManager: ===== StartActualPlayback COMPLETE =====", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void OpenMapForLocalPlayback()
	{
		Print("GRAD_BC_ReplayManager: Setting up debriefing screen for replay", LogLevel.NORMAL);
		
		// Clear all live markers before starting replay
		Print("GRAD_BC_ReplayManager: Clearing live transmission and player markers", LogLevel.NORMAL);
		
		// Clear live transmission markers
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
		{
			GRAD_MapMarkerManager markerMgr = GRAD_MapMarkerManager.Cast(mapEntity.GetMapModule(GRAD_MapMarkerManager));
			if (markerMgr)
			{
				markerMgr.SetReplayMode(true);
				Print("GRAD_BC_ReplayManager: Enabled replay mode on marker manager", LogLevel.NORMAL);
			}
			
			// Note: GRAD_IconMarkerUI is per-player component, not a map module
			// Player icon markers will be hidden when players are removed/hidden for replay
		}
		
		// Trigger server-side VoN setup via RPC
		if (Replication.IsServer())
		{
			Print("GRAD_BC_ReplayManager: Server detected, setting up VoN directly", LogLevel.NORMAL);
			SetAllPlayersToGlobalVoN();
		}
		else
		{
			Print("GRAD_BC_ReplayManager: Client detected, requesting server to setup VoN", LogLevel.NORMAL);
			Rpc(RpcAsk_SetupReplayVoN);
		}
		
		// Open map fullscreen for replay visualization
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
		Print("GRAD_BC_ReplayManager: Map menu opened for replay", LogLevel.NORMAL);
		
		// Verify map is ready for replay markers
		GetGame().GetCallqueue().CallLater(VerifyReplayMapReady, 200, false);
	}
	
	//------------------------------------------------------------------------------------------------
	void VerifyReplayMapReady()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
		{
			Print("GRAD_BC_ReplayManager: Map entity not ready yet, retrying...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(VerifyReplayMapReady, 200, false);
			return;
		}
		
		GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.Cast(mapEntity.GetMapModule(GRAD_BC_ReplayMapLayer));
		if (!replayLayer)
		{
			Print("GRAD_BC_ReplayManager: Replay map layer not found yet, retrying...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(VerifyReplayMapReady, 200, false);
			return;
		}
		
		Print("GRAD_BC_ReplayManager: Map and replay layer verified ready for playback", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetupReplayVoN()
	{
		Print("GRAD_BC_ReplayManager: Server received VoN setup request", LogLevel.NORMAL);
		SetAllPlayersToGlobalVoN();
	}
	
	//------------------------------------------------------------------------------------------------
	void SetAllPlayersToGlobalVoN()
	{
		Print("GRAD_BC_ReplayManager: Moving all players to global VoN room", LogLevel.NORMAL);
		
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);
		
		// Try to get VoN manager from PSCore
		PS_VoNRoomsManager vonManager = PS_VoNRoomsManager.GetInstance();
		if (!vonManager)
		{
			Print("GRAD_BC_ReplayManager: VoN manager not found - cross-faction voice may not work", LogLevel.WARNING);
			return;
		}
		
		// Move each player to global room (empty strings = everyone can hear)
		foreach (int playerId : playerIds)
		{
			vonManager.MoveToRoom(playerId, "", ""); // Empty faction + room = global
			Print(string.Format("GRAD_BC_ReplayManager: Player %1 moved to global voice room", playerId), LogLevel.NORMAL);
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: %1 players in global voice room for replay", playerIds.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void SendFrameChunk(int startIndex, int endIndex)
	{
		// Convert frame data to simple arrays for transmission
		ref array<float> timestamps = {};
		ref array<string> playerIds = {};
		ref array<vector> positions = {};
		ref array<vector> rotations = {};
		ref array<string> factions = {};
		ref array<bool> inVehicles = {};
		ref array<string> playerNames = {};
		
		// Projectile data arrays
		ref array<float> projTimestamps = {};
		ref array<string> projTypes = {};
		ref array<vector> projFiringPos = {};
		ref array<vector> projImpactPos = {};
		ref array<vector> projVelocities = {};
		
		// Transmission data arrays
		ref array<float> transTimestamps = {};
		ref array<vector> transPositions = {};
		ref array<int> transStates = {};
		ref array<float> transProgress = {};

		// Vehicle data arrays
		ref array<float> vehicleTimestamps = {};
		ref array<RplId> vehicleIds = {};
		ref array<string> vehicleTypes = {};
		ref array<string> vehicleFactions = {};
		ref array<vector> vehiclePositions = {};
		ref array<vector> vehicleRotations = {};
		
		for (int i = startIndex; i < endIndex; i++)
		{
			GRAD_BC_ReplayFrame frame = m_replayData.frames[i];
			
			foreach (GRAD_BC_PlayerSnapshot playerData : frame.players)
			{
				timestamps.Insert(frame.timestamp);
				playerIds.Insert(string.Format("%1", playerData.playerId));
				positions.Insert(playerData.position);
				rotations.Insert(playerData.angles);
				factions.Insert(playerData.factionKey);
				inVehicles.Insert(playerData.isInVehicle);
				playerNames.Insert(playerData.playerName);
			}
			
			// Add projectile data
			foreach (GRAD_BC_ProjectileSnapshot projData : frame.projectiles)
			{
				projTimestamps.Insert(frame.timestamp);
				projTypes.Insert(projData.projectileType);
				projFiringPos.Insert(projData.position);
				projImpactPos.Insert(projData.impactPosition);
				projVelocities.Insert(projData.velocity);
			}
			
			// Add transmission data
			foreach (GRAD_BC_TransmissionSnapshot transData : frame.transmissions)
			{
				transTimestamps.Insert(frame.timestamp);
				transPositions.Insert(transData.position);
				transStates.Insert(transData.state);
				transProgress.Insert(transData.progress);
			}
			
			// Add vehicle data
			foreach (GRAD_BC_VehicleSnapshot vehicleData : frame.vehicles)
			{
				vehicleTimestamps.Insert(frame.timestamp);
				vehicleIds.Insert(vehicleData.entityId);
				vehicleTypes.Insert(vehicleData.vehicleType);
				vehicleFactions.Insert(vehicleData.factionKey);
				vehiclePositions.Insert(vehicleData.position);
				vehicleRotations.Insert(vehicleData.angles);
			}
		}
		
		// Send player data
		Rpc(RpcAsk_ReceivePlayerChunk, timestamps, playerIds, positions, rotations, factions, inVehicles, playerNames);
		
		// Send projectile data if any
		if (projTimestamps.Count() > 0)
			Rpc(RpcAsk_ReceiveProjectileChunk, projTimestamps, projTypes, projFiringPos, projImpactPos, projVelocities);
		
		// Send transmission data if any
		if (transTimestamps.Count() > 0)
			Rpc(RpcAsk_ReceiveTransmissionChunk, transTimestamps, transPositions, transStates, transProgress);

		// Send vehicle data if any
		if (vehicleTimestamps.Count() > 0)
			Rpc(RpcAsk_ReceiveVehicleChunk, vehicleTimestamps, vehicleIds, vehicleTypes, vehicleFactions, vehiclePositions, vehicleRotations);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_StartReplayPlayback(float totalDuration, string missionName, string mapName, float startTime)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		Print(string.Format("GRAD_BC_ReplayManager: === RPC RECEIVED === RpcAsk_StartReplayPlayback on %1",isServer), LogLevel.NORMAL);
		Print(string.Format("GRAD_BC_ReplayManager: RpcAsk_StartReplayPlayback received - Server: %1", isServer), LogLevel.NORMAL);
		
		if (Replication.IsServer())
			return; // Don't run on server
			
		Print("GRAD_BC_ReplayManager: Client initializing replay data", LogLevel.NORMAL);
		
		// Initialize replay data structure
		m_replayData = GRAD_BC_ReplayData.Create();
		m_replayData.totalDuration = totalDuration;
		m_replayData.missionName = missionName;
		m_replayData.mapName = mapName;
		m_replayData.startTime = startTime;
		
		Print(string.Format("GRAD_BC_ReplayManager: Client replay data initialized - Duration: %1", totalDuration), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReceiveVehicleChunk(array<float> timestamps, array<RplId> vehicleIds, array<string> vehicleTypes, array<string> vehicleFactions, array<vector> vehiclePositions, array<vector> vehicleRotations)
	{
		if (Replication.IsServer())
			return;
			
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data not initialized!", LogLevel.ERROR);
			return;
		}
		
		for (int i = 0; i < timestamps.Count(); i++)
		{
			float timestamp = timestamps[i];
			
			GRAD_BC_ReplayFrame frame = null;
			for (int j = 0; j < m_replayData.frames.Count(); j++)
			{
				if (m_replayData.frames[j].timestamp == timestamp)
				{
					frame = m_replayData.frames[j];
					break;
				}
			}
			
			if (!frame)
			{
				frame = new GRAD_BC_ReplayFrame();
				frame.timestamp = timestamp;
				m_replayData.frames.Insert(frame);
			}
			
			GRAD_BC_VehicleSnapshot vehicleData = GRAD_BC_VehicleSnapshot.Create(
				vehicleIds[i],
				vehicleTypes[i],
				vehicleFactions[i],
				vehiclePositions[i],
				vehicleRotations[i],
				false
			);
			
			frame.vehicles.Insert(vehicleData);
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: Received vehicle chunk with %1 vehicles", timestamps.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReceivePlayerChunk(array<float> timestamps, array<string> playerIds, array<vector> positions, 
		array<vector> rotations, array<string> factions, array<bool> inVehicles, array<string> playerNames)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		Print(string.Format("GRAD_BC_ReplayManager: === RPC RECEIVED === RpcAsk_ReceivePlayerChunk on %1", isServer), LogLevel.NORMAL);
		
		if (Replication.IsServer())
			return;
			
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data not initialized!", LogLevel.ERROR);
			return;
		}
		
		// Process player data
		for (int i = 0; i < timestamps.Count(); i++)
		{
			float timestamp = timestamps[i];
			
			// Find or create frame for this timestamp
			GRAD_BC_ReplayFrame frame = null;
			for (int j = 0; j < m_replayData.frames.Count(); j++)
			{
				if (m_replayData.frames[j].timestamp == timestamp)
				{
					frame = m_replayData.frames[j];
					break;
				}
			}
			
			if (!frame)
			{
				frame = new GRAD_BC_ReplayFrame();
				frame.timestamp = timestamp;
				m_replayData.frames.Insert(frame);
			}
			
			GRAD_BC_PlayerSnapshot playerData = GRAD_BC_PlayerSnapshot.Create(
				playerIds[i].ToInt(),
				playerNames[i],
				factions[i],
				positions[i],
				rotations[i],
				true,
				inVehicles[i]
			);
			
			frame.players.Insert(playerData);
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: Received player chunk with %1 players", timestamps.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReceiveProjectileChunk(array<float> projTimestamps, array<string> projTypes, 
		array<vector> projFiringPos, array<vector> projImpactPos, array<vector> projVelocities)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		Print(string.Format("GRAD_BC_ReplayManager: === RPC RECEIVED === RpcAsk_ReceiveProjectileChunk on %1", isServer), LogLevel.NORMAL);
		
		if (Replication.IsServer())
			return;
			
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data not initialized!", LogLevel.ERROR);
			return;
		}
		
		// Process projectile data
		for (int i = 0; i < projTimestamps.Count(); i++)
		{
			float timestamp = projTimestamps[i];
			
			// Find or create frame for this timestamp
			GRAD_BC_ReplayFrame frame = null;
			for (int j = 0; j < m_replayData.frames.Count(); j++)
			{
				if (m_replayData.frames[j].timestamp == timestamp)
				{
					frame = m_replayData.frames[j];
					break;
				}
			}
			
			if (!frame)
			{
				frame = new GRAD_BC_ReplayFrame();
				frame.timestamp = timestamp;
				m_replayData.frames.Insert(frame);
			}
			
			GRAD_BC_ProjectileSnapshot projData = GRAD_BC_ProjectileSnapshot.Create(
				projTypes[i],
				projFiringPos[i],
				projImpactPos[i],
				projVelocities[i],
				10.0
			);
			
			frame.projectiles.Insert(projData);
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: Received projectile chunk with %1 projectiles", projTimestamps.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReceiveTransmissionChunk(array<float> transTimestamps, array<vector> transPositions, 
		array<int> transStates, array<float> transProgress)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		Print(string.Format("GRAD_BC_ReplayManager: === RPC RECEIVED === RpcAsk_ReceiveTransmissionChunk on %1", isServer), LogLevel.NORMAL);
		
		if (Replication.IsServer())
			return;
			
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data not initialized!", LogLevel.ERROR);
			return;
		}
		
		// Process transmission data
		for (int i = 0; i < transTimestamps.Count(); i++)
		{
			float timestamp = transTimestamps[i];
			
			// Find or create frame for this timestamp
			GRAD_BC_ReplayFrame frame = null;
			for (int j = 0; j < m_replayData.frames.Count(); j++)
			{
				if (m_replayData.frames[j].timestamp == timestamp)
				{
					frame = m_replayData.frames[j];
					break;
				}
			}
			
			if (!frame)
			{
				frame = new GRAD_BC_ReplayFrame();
				frame.timestamp = timestamp;
				m_replayData.frames.Insert(frame);
			}
			
			GRAD_BC_TransmissionSnapshot transData = GRAD_BC_TransmissionSnapshot.Create(
				transPositions[i],
				transStates[i],
				transProgress[i]
			);
			
			frame.transmissions.Insert(transData);
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: Received transmission chunk with %1 transmissions", transTimestamps.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReplayDataComplete()
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		Print(string.Format("GRAD_BC_ReplayManager: === RPC RECEIVED === RpcAsk_ReplayDataComplete on %1", isServer), LogLevel.NORMAL);
		Print(string.Format("GRAD_BC_ReplayManager: RpcAsk_ReplayDataComplete received - Server: %1", Replication.IsServer()), LogLevel.NORMAL);
		
		if (Replication.IsServer())
			return;
			
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data not available for playback!", LogLevel.ERROR);
			return;
		}
			
		Print(string.Format("GRAD_BC_ReplayManager: Client received complete replay data, %1 frames", m_replayData.frames.Count()), LogLevel.NORMAL);
		
		// Open map and start playback
		Print("GRAD_BC_ReplayManager: Starting client playback in 500ms", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(StartClientReplayPlayback, 500, false);
	}
	
	//------------------------------------------------------------------------------------------------
	void StartClientReplayPlayback()
	{
		Print("GRAD_BC_ReplayManager: StartClientReplayPlayback called", LogLevel.NORMAL);
		
		// Open map for replay viewing using gadget manager approach
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController)
		{
			Print("GRAD_BC_ReplayManager: No player controller found", LogLevel.ERROR);
			return;
		}
		
		Print("GRAD_BC_ReplayManager: Player controller found, getting character", LogLevel.NORMAL);
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!ch)
		{
			Print("GRAD_BC_ReplayManager: No controlled character found", LogLevel.ERROR);
			return;
		}
		
		Print("GRAD_BC_ReplayManager: Character found, getting gadget manager", LogLevel.NORMAL);
		
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(ch.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager)
		{
			Print("GRAD_BC_ReplayManager: No gadget manager found", LogLevel.ERROR);
			return;
		}
		
		Print("GRAD_BC_ReplayManager: Gadget manager found, getting map gadget", LogLevel.NORMAL);
		
		IEntity mapEntity = gadgetManager.GetGadgetByType(EGadgetType.MAP);
		if (!mapEntity)
		{
			Print("GRAD_BC_ReplayManager: No map gadget found", LogLevel.ERROR);
			return;
		}
		
		Print("GRAD_BC_ReplayManager: Map gadget found, opening map", LogLevel.NORMAL);
		gadgetManager.SetGadgetMode(mapEntity, EGadgetMode.IN_HAND, true);
		Print("GRAD_BC_ReplayManager: Map opened successfully", LogLevel.NORMAL);
		
		// Verify world is available before starting playback
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("GRAD_BC_ReplayManager: World not available in StartClientReplayPlayback", LogLevel.ERROR);
			return;
		}
		
		// Start playback
		m_bIsPlayingBack = true;
		m_fPlaybackStartTime = world.GetWorldTime() / 1000.0; // Convert milliseconds to seconds
		m_fCurrentPlaybackTime = 0;
		m_iCurrentFrameIndex = 0;
		
		// Disable live markers
		// Note: SCR_MapEntity must be available before disabling markers
		/*
		SCR_MapEntity mapEntity2 = SCR_MapEntity.GetMapInstance();
		if (mapEntity2)
		{
			GRAD_MapMarkerManager markerMgr = GRAD_MapMarkerManager.Cast(mapEntity2.GetMapModule(GRAD_MapMarkerManager));
			if (markerMgr)
			{
				markerMgr.SetReplayMode(true);
				Print("GRAD_BC_ReplayManager: Enabled replay mode on marker manager", LogLevel.NORMAL);
			}
		}
		*/
		
		// Skip empty frames at start
		SkipEmptyFrames();
		
		Print("GRAD_BC_ReplayManager: Starting playback loop", LogLevel.NORMAL);
		// Start playback loop
		GetGame().GetCallqueue().CallLater(UpdatePlayback, 100, true);
		
		// Show replay controls
		Print("GRAD_BC_ReplayManager: Scheduling UI creation", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(ShowReplayControls, 500, false);
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdatePlayback()
	{
		// Enhanced logging to diagnose playback issues
		if (!m_bIsPlayingBack || !m_replayData || m_bPlaybackPaused)
		{
			static int updatePlaybackSkipCounter = 0;
			updatePlaybackSkipCounter++;
			if (updatePlaybackSkipCounter % 50 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayManager: UpdatePlayback skipped - IsPlayingBack: %1, HasData: %2, IsPaused: %3", 
					m_bIsPlayingBack, m_replayData != null, m_bPlaybackPaused), LogLevel.WARNING);
				if (m_replayData)
				{
					Print(string.Format("GRAD_BC_ReplayManager: Replay data exists with %1 frames", m_replayData.frames.Count()), LogLevel.WARNING);
				}
			}
			return;
		}
		
		// Debug: Log that UpdatePlayback is running
		static int updatePlaybackCallCounter = 0;
		updatePlaybackCallCounter++;
		if (updatePlaybackCallCounter <= 5 || updatePlaybackCallCounter % 50 == 0)
		{
			Print(string.Format("GRAD_BC_ReplayManager: UpdatePlayback called (#%1) - Current frame: %2/%3", 
				updatePlaybackCallCounter, m_iCurrentFrameIndex, m_replayData.frames.Count()), LogLevel.NORMAL);
		}
			
		// Calculate elapsed time since playback started (in seconds)
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("GRAD_BC_ReplayManager: World not available in UpdatePlayback, stopping playback", LogLevel.ERROR);
			m_bIsPlayingBack = false;
			GetGame().GetCallqueue().Remove(UpdatePlayback);
			return;
		}
		
		float currentWorldTime = world.GetWorldTime() / 1000.0; // Convert to seconds
		m_fCurrentPlaybackTime = (currentWorldTime - m_fPlaybackStartTime) * m_fPlaybackSpeed;
		
		if (updatePlaybackCallCounter <= 5 || updatePlaybackCallCounter % 50 == 0)
		{
			Print(string.Format("GRAD_BC_ReplayManager: Playback time: %.2f, Speed: %.2f, Frame: %3/%4", 
				m_fCurrentPlaybackTime, m_fPlaybackSpeed, m_iCurrentFrameIndex, m_replayData.frames.Count()), LogLevel.NORMAL);
		}
		
		// Find current frame to display
		int framesProcessedThisUpdate = 0;
		while (m_iCurrentFrameIndex < m_replayData.frames.Count())
		{
			GRAD_BC_ReplayFrame frame = m_replayData.frames[m_iCurrentFrameIndex];
			framesProcessedThisUpdate++;
			float frameTimeFromStart = frame.timestamp - m_replayData.startTime;
			
			if (frameTimeFromStart <= m_fCurrentPlaybackTime)
			{
				// Display this frame
				if (updatePlaybackCallCounter <= 10 || m_iCurrentFrameIndex % 20 == 0)
				{
					Print(string.Format("GRAD_BC_ReplayManager: Displaying frame %1 (time: %.2f vs %.2f)", 
						m_iCurrentFrameIndex, frameTimeFromStart, m_fCurrentPlaybackTime), LogLevel.NORMAL);
				}
				DisplayReplayFrame(frame);
				m_iCurrentFrameIndex++;
			}
			else
			{
				if (updatePlaybackCallCounter <= 5)
				{
					Print(string.Format("GRAD_BC_ReplayManager: Waiting for frame %1 (time: %.2f, current: %.2f)", 
						m_iCurrentFrameIndex, frameTimeFromStart, m_fCurrentPlaybackTime), LogLevel.NORMAL);
				}
				break; // Wait for next update
			}
		}
		
		if (framesProcessedThisUpdate > 0 && (updatePlaybackCallCounter <= 10 || updatePlaybackCallCounter % 25 == 0))
		{
			Print(string.Format("GRAD_BC_ReplayManager: Processed %1 frames in this update cycle", framesProcessedThisUpdate), LogLevel.NORMAL);
		}
		
		// Check if playback finished
		if (m_fCurrentPlaybackTime >= m_replayData.totalDuration)
		{
			Print("GRAD_BC_ReplayManager: Playback finished, stopping", LogLevel.NORMAL);
			StopPlayback();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void DisplayReplayFrame(GRAD_BC_ReplayFrame frame)
	{
		// Reduce log frequency for frame display
		static int frameLogCounter = 0;
		frameLogCounter++;
		
		if (frameLogCounter % 10 == 0) // Only log every 10th frame
		{
			Print(string.Format("GRAD_BC_ReplayManager: Displaying frame at %1s: %2 players, %3 projectiles", 
				frame.timestamp, frame.players.Count(), frame.projectiles.Count()), LogLevel.NORMAL);
		}
			
		// Update map layer with current frame
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
		{
			// Debug: Log map entity found
			if (frameLogCounter <= 3)
			{
				Print("GRAD_BC_ReplayManager: Map entity found, searching for replay layer...", LogLevel.NORMAL);
			}
			
			GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.Cast(mapEntity.GetMapModule(GRAD_BC_ReplayMapLayer));
			if (replayLayer)
			{
				// Debug: Confirm replay layer found
				if (frameLogCounter <= 3)
				{
					Print(string.Format("GRAD_BC_ReplayManager: Replay layer found! Sending frame with %1 players", 
						frame.players.Count()), LogLevel.NORMAL);
				}
				replayLayer.UpdateReplayFrame(frame);
			}
			else
			{
				// Only log warning periodically to reduce spam
				if (frameLogCounter % 20 == 0)
				{
					Print("GRAD_BC_ReplayManager: WARNING - Replay layer not found on map entity", LogLevel.WARNING);
				}
				static int warningCounter = 0;
				warningCounter++;
				if (warningCounter % 20 == 0)
				{
					Print(string.Format("GRAD_BC_ReplayManager: No replay map layer found in map entity (warning #%1)", 
						warningCounter), LogLevel.WARNING);
				}
			}
		}
		else
		{
			static int noMapCounter = 0;
			noMapCounter++;
			if (noMapCounter % 20 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayManager: No map entity found (warning #%1)", 
					noMapCounter), LogLevel.WARNING);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void ShowReplayControls()
	{
		Print("GRAD_BC_ReplayManager: Showing replay controls", LogLevel.NORMAL);
		
		// Replay controls disabled - using only progress bar on map for now
		// The controls layout has visibility issues, progress bar works fine
		/*
		// Create and show replay controls UI
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
		{
			Print("GRAD_BC_ReplayManager: Workspace found, creating replay controls", LogLevel.NORMAL);
			GRAD_BC_ReplayControls replayControls = new GRAD_BC_ReplayControls();
			replayControls.DisplayInit(null);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: ERROR - No workspace found! Retrying in 1 second...", LogLevel.ERROR);
			// Retry after a short delay
			GetGame().GetCallqueue().CallLater(ShowReplayControls, 1000, false);
		}
		*/
	}
	
	//------------------------------------------------------------------------------------------------
	void SetPlaybackSpeed(float speed)
	{
		m_fPlaybackSpeed = speed;
		Print(string.Format("GRAD_BC_ReplayManager: Playback speed set to %.1fx", speed), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	float GetPlaybackSpeed()
	{
		return m_fPlaybackSpeed;
	}
	
	//------------------------------------------------------------------------------------------------
	float GetCurrentPlaybackTime()
	{
		return m_fCurrentPlaybackTime;
	}
	
	//------------------------------------------------------------------------------------------------
	float GetTotalDuration()
	{
		if (m_replayData)
			return m_replayData.totalDuration;
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	void PausePlayback()
	{
		m_bPlaybackPaused = true;
		Print("GRAD_BC_ReplayManager: Playback paused", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void ResumePlayback()
	{
		if (m_bPlaybackPaused)
		{
			m_bPlaybackPaused = false;
			// Adjust start time to account for pause duration
			float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
			m_fPlaybackStartTime = currentWorldTime - (m_fCurrentPlaybackTime / m_fPlaybackSpeed);
			Print("GRAD_BC_ReplayManager: Playback resumed", LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void StopPlayback()
	{
		m_bIsPlayingBack = false;
		m_bPlaybackPaused = false;
		GetGame().GetCallqueue().Remove(UpdatePlayback);
		
		// Re-enable live markers
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
		{
			GRAD_MapMarkerManager markerMgr = GRAD_MapMarkerManager.Cast(mapEntity.GetMapModule(GRAD_MapMarkerManager));
			if (markerMgr)
			{
				markerMgr.SetReplayMode(false);
				Print("GRAD_BC_ReplayManager: Disabled replay mode on marker manager", LogLevel.NORMAL);
			}
		}
		
		Print("GRAD_BC_ReplayManager: Playback finished, closing map", LogLevel.NORMAL);
		
		// Close the map
		CloseMap();
		
		// Note: Server will automatically trigger endscreen after scheduled time
		Print("GRAD_BC_ReplayManager: Waiting for server to trigger endscreen", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Server automatically triggers endscreen after replay duration
	void TriggerEndscreen()
	{
		Print("GRAD_BC_ReplayManager: ===== TriggerEndscreen CALLED =====", LogLevel.NORMAL);
		Print(string.Format("GRAD_BC_ReplayManager: IsServer: %1", Replication.IsServer()), LogLevel.NORMAL);
		
		if (!Replication.IsServer())
		{
			Print("GRAD_BC_ReplayManager: Not server, returning", LogLevel.WARNING);
			return;
		}
		
		Print("GRAD_BC_ReplayManager: Server triggering endscreen NOW", LogLevel.NORMAL);
		
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
		{
			bcm.SetBreakingContactPhase(EBreakingContactPhase.GAMEOVERDONE);
			bcm.ShowPostReplayGameOverScreen();
			
			// In local/singleplayer, also trigger the RPC handler directly
			// because broadcast RPCs don't loop back to the server in some cases
			PlayerController pc = GetGame().GetPlayerController();
			if (pc)
			{
				Print("GRAD_BC_ReplayManager: Local player detected, calling endscreen RPC handler directly", LogLevel.NORMAL);
				bcm.RpcDo_ShowGameOverScreen();
			}
		}
		else
		{
			Print("GRAD_BC_ReplayManager: ERROR - Could not find BreakingContactManager", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CloseMap()
	{
		Print("GRAD_BC_ReplayManager: Attempting to close map", LogLevel.NORMAL);
		
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
		{
			Print("GRAD_BC_ReplayManager: No player controller found to close map", LogLevel.WARNING);
			return;
		}
		
		IEntity playerEntity = playerController.GetControlledEntity();
		if (!playerEntity)
		{
			Print("GRAD_BC_ReplayManager: No player entity found to close map", LogLevel.WARNING);
			return;
		}
		
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(playerEntity.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager)
		{
			Print("GRAD_BC_ReplayManager: No gadget manager found to close map", LogLevel.WARNING);
			return;
		}
		
		IEntity mapGadget = gadgetManager.GetGadgetByType(EGadgetType.MAP);
		if (!mapGadget)
		{
			Print("GRAD_BC_ReplayManager: No map gadget found", LogLevel.WARNING);
			return;
		}
		
		// Put map back into inventory
		gadgetManager.SetGadgetMode(mapGadget, EGadgetMode.IN_SLOT);
		Print("GRAD_BC_ReplayManager: Map closed successfully", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void SeekToTime(float timeSeconds)
	{
		if (!m_replayData)
			return;
			
		m_fCurrentPlaybackTime = Math.Clamp(timeSeconds, 0, m_replayData.totalDuration);
		m_fPlaybackStartTime = GetGame().GetWorld().GetWorldTime() - m_fCurrentPlaybackTime;
		
		// Find corresponding frame index
		m_iCurrentFrameIndex = 0;
		for (int i = 0; i < m_replayData.frames.Count(); i++)
		{
			GRAD_BC_ReplayFrame frame = m_replayData.frames[i];
			float frameTime = frame.timestamp - m_replayData.startTime;
			
			if (frameTime <= m_fCurrentPlaybackTime)
			{
				m_iCurrentFrameIndex = i;
			}
			else
			{
				break;
			}
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: Seeked to time %1s (frame %2)", 
			timeSeconds, m_iCurrentFrameIndex), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Public interface methods
	bool IsRecording() 
	{ 
		return m_bIsRecording; 
	}
	
	bool IsPlayingBack() 
	{ 
		return m_bIsPlayingBack; 
	}
	
	bool IsPlaybackPaused() 
	{ 
		return m_bPlaybackPaused; 
	}
	
	float GetPlaybackTime() 
	{ 
		return m_fCurrentPlaybackTime; 
	}
	
	//------------------------------------------------------------------------------------------------
	// New method for recording projectile firing events
	void RecordProjectileFired(vector position, vector velocity, string ammoType)
	{
		Print(string.Format("GRAD_BC_ReplayManager: RecordProjectileFired called - Recording=%1, RecordProj=%2, AmmoType=%3", 
			m_bIsRecording, m_bRecordProjectiles, ammoType), LogLevel.NORMAL);
		
		if (!m_bIsRecording)
		{
			Print("GRAD_BC_ReplayManager: Not recording - ignoring projectile", LogLevel.WARNING);
			return;
		}
		
		if (!m_bRecordProjectiles)
		{
			Print("GRAD_BC_ReplayManager: Projectile recording disabled - ignoring projectile", LogLevel.WARNING);
			return;
		}
			
		// Store the projectile data for the next recording frame
		GRAD_BC_ProjectileData projData = new GRAD_BC_ProjectileData();
		projData.position = position;
		projData.velocity = velocity;
		projData.ammoType = ammoType;
		projData.fireTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // Convert to seconds
		
		m_pendingProjectiles.Insert(projData);
		
		Print(string.Format("GRAD_BC_ReplayManager: Successfully queued projectile - %1 at %2 (pending count: %3)", 
			ammoType, position.ToString(), m_pendingProjectiles.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void VerifyMapIsOpen()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
		{
			Print("GRAD_BC_ReplayManager: Map verified as open", LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: Map not open, trying alternative method", LogLevel.WARNING);
			
			// Try using the Breaking Contact player component method
			GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
			if (playerComponent)
			{
				Print("GRAD_BC_ReplayManager: Using Breaking Contact player component to open map", LogLevel.NORMAL);
				playerComponent.ToggleMap(true);
			}
			else
			{
				Print("GRAD_BC_ReplayManager: No player component available", LogLevel.ERROR);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// DEBUG: Manual test function to trigger replay UI for testing
	void DebugStartTestReplay()
	{
		Print("GRAD_BC_ReplayManager: === DEBUG TEST REPLAY STARTED ===", LogLevel.NORMAL);
		
		// Create minimal test data
		if (!m_replayData)
		{
			m_replayData = GRAD_BC_ReplayData.Create();
			m_replayData.totalDuration = 60.0; // 1 minute test replay
			m_replayData.missionName = "Test Replay";
			m_replayData.mapName = "Test Map";
			
			// Create a few test frames
			for (int i = 0; i < 10; i++)
			{
				GRAD_BC_ReplayFrame frame = GRAD_BC_ReplayFrame.Create(i * 6.0); // Every 6 seconds
				m_replayData.frames.Insert(frame);
			}
		}
		
		// Force start playback
		m_bIsPlayingBack = true;
		m_fPlaybackStartTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
		m_fCurrentPlaybackTime = 0;
		m_iCurrentFrameIndex = 0;
		m_bPlaybackPaused = false;
		
		Print("GRAD_BC_ReplayManager: Test replay data created, showing controls", LogLevel.NORMAL);
		ShowReplayControls();
		
		// Start the playback update loop
		GetGame().GetCallqueue().CallLater(UpdatePlayback, 100, true);
		
		Print("GRAD_BC_ReplayManager: === DEBUG TEST REPLAY INITIALIZATION COMPLETE ===", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Calculate adaptive playback speed to fit replay into max duration
	void CalculateAdaptiveSpeed()
	{
		if (!m_replayData)
		{
			m_fAdaptiveSpeed = 1.0;
			return;
		}
		
		float actualDuration = m_replayData.totalDuration;
		
		// If replay is longer than max duration, speed it up
		if (actualDuration > m_fMaxReplayDuration)
		{
			m_fAdaptiveSpeed = actualDuration / m_fMaxReplayDuration;
			Print(string.Format("GRAD_BC_ReplayManager: Replay duration %.1fs exceeds max %.1fs, using adaptive speed %.2fx", 
				actualDuration, m_fMaxReplayDuration, m_fAdaptiveSpeed), LogLevel.NORMAL);
		}
		else
		{
			m_fAdaptiveSpeed = 1.0;
			Print(string.Format("GRAD_BC_ReplayManager: Replay duration %.1fs fits within max %.1fs, using normal speed", 
				actualDuration, m_fMaxReplayDuration), LogLevel.NORMAL);
		}
		
		// Set the playback speed to the adaptive speed
		m_fPlaybackSpeed = m_fAdaptiveSpeed;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get replay progress as percentage (0.0 to 1.0) for progress bar
	float GetReplayProgress()
	{
		if (!m_replayData || m_replayData.totalDuration <= 0)
			return 0.0;
			
		return Math.Clamp(m_fCurrentPlaybackTime / m_replayData.totalDuration, 0.0, 1.0);
	}
	
	//------------------------------------------------------------------------------------------------
	// Get current replay time in seconds
	float GetCurrentReplayTime()
	{
		return m_fCurrentPlaybackTime;
	}
	
	//------------------------------------------------------------------------------------------------
	// Get total replay duration in seconds
	float GetTotalReplayDuration()
	{
		if (!m_replayData)
			return 0.0;
			
		return m_replayData.totalDuration;
	}
	
	//------------------------------------------------------------------------------------------------
	// Skip empty frames at the start of replay
	void SkipEmptyFrames()
	{
		if (!m_replayData || m_replayData.frames.Count() == 0)
			return;
			
		Print("GRAD_BC_ReplayManager: Checking for empty frames to skip...", LogLevel.NORMAL);
		
		int skippedCount = 0;
		int startIndex = m_iCurrentFrameIndex;
		
		// Look ahead for the first frame with interesting content
		for (int i = startIndex; i < m_replayData.frames.Count(); i++)
		{
			GRAD_BC_ReplayFrame frame = m_replayData.frames[i];
			
			// Check if frame has any content
			bool hasContent = frame.players.Count() > 0 || 
							  frame.projectiles.Count() > 0 || 
							  frame.transmissions.Count() > 0;
							  
			if (hasContent)
			{
				// Found content!
				if (i > startIndex)
				{
					// Update playback state to this frame
					m_iCurrentFrameIndex = i;
					float frameTime = frame.timestamp - m_replayData.startTime;
					m_fCurrentPlaybackTime = frameTime;
					
					// Adjust start time so playback continues from here
					float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
					m_fPlaybackStartTime = currentWorldTime - (m_fCurrentPlaybackTime / m_fPlaybackSpeed);
					
					Print(string.Format("GRAD_BC_ReplayManager: Skipped %1 empty frames. Starting at frame %2 (time: %.2f)", 
						i - startIndex, i, frameTime), LogLevel.NORMAL);
				}
				else
				{
					Print("GRAD_BC_ReplayManager: No empty frames to skip.", LogLevel.NORMAL);
				}
				return;
			}
		}
		
		Print("GRAD_BC_ReplayManager: Warning - All remaining frames seem empty!", LogLevel.WARNING);
	}
}
