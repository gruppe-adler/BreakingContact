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

	[Attribute("7200", UIWidgets.EditBox, "Maximum number of frames to record (7200 = 2 hours at 1fps)")]
	protected int m_iMaxFrames;

	[Attribute("64.0", UIWidgets.EditBox, "Maximum replay memory usage in MB before stopping recording")]
	protected float m_fMaxMemoryUsageMB;

	// Recording state
	protected bool m_bIsRecording = false;
	protected bool m_bIsPlayingBack = false;
	protected ref GRAD_BC_ReplayData m_replayData;
	protected float m_fLastRecordTime = 0;

	// Capacity warning flags
	protected bool m_bWarning90Frames = false;
	protected bool m_bWarning90Memory = false;
	
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
	
	// Vehicles that have been used by players at least once
	protected ref array<RplId> m_usedVehicleIds = {};
	
	// RPC for client communication
	protected RplComponent m_RplComponent;
	
	// Static instance for global access
	protected static GRAD_BC_ReplayManager s_Instance;
	
	// Loading progress tracking
	protected int m_iTotalChunksToSend = 0;
	protected int m_iChunksSent = 0;

	// First frame tracking - loading screen stays until first frame renders
	protected bool m_bFirstFrameDisplayed = false;
	
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

		// Wait for RplComponent to be available before registering
		WaitForVehicleRplComponent(vehicle, 0);
	}

	// Helper to wait for RplComponent initialization
	void WaitForVehicleRplComponent(Vehicle vehicle, int attempt)
	{
		if (!vehicle)
			return;

		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
		{
			if (!m_trackedVehicles)
				m_trackedVehicles = new array<IEntity>();

			if (m_trackedVehicles.Find(vehicle) == -1)
			{
				m_trackedVehicles.Insert(vehicle);
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("GRAD_BC_ReplayManager: Registered vehicle for tracking: %1 (total: %2)", vehicle.GetPrefabData().GetPrefabName(), m_trackedVehicles.Count()), LogLevel.NORMAL);
			}
			else
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("GRAD_BC_ReplayManager: Vehicle already tracked: %1", vehicle.GetPrefabData().GetPrefabName()), LogLevel.VERBOSE);
			}
			return;
		}

		// Retry up to 20 times (2 seconds total if called every 100ms)
		if (attempt < 20)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Waiting for RplComponent on vehicle %1 (attempt %2)", vehicle.GetPrefabData().GetPrefabName(), attempt+1), LogLevel.VERBOSE);
			GetGame().GetCallqueue().CallLater(WaitForVehicleRplComponent, 100, false, vehicle, attempt+1);
		}
		else
		{
			Print(string.Format("GRAD_BC_ReplayManager: ERROR - RplComponent not found for vehicle %1 after 2s, not tracking", vehicle.GetPrefabData().GetPrefabName()), LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		// VERSION IDENTIFIER - This log will change when code updates
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: VERSION 2025-08-20-14:30 - Single-player detection enabled", LogLevel.NORMAL);
		
		// Force initialization on both server and client
		if (!s_Instance)
		{
			s_Instance = this;
			string isServer = "Client";
			if (Replication.IsServer()) { isServer = "Server"; }
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Force initialized via EOnInit on %1", isServer), LogLevel.NORMAL);
		}
		
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// Clean up all CallLater callbacks
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(CheckGameState);
			GetGame().GetCallqueue().Remove(RecordFrame);
			GetGame().GetCallqueue().Remove(UpdatePlayback);
			GetGame().GetCallqueue().Remove(InitializeReplaySystem);
			GetGame().GetCallqueue().Remove(WaitForVehicleRplComponent);
			GetGame().GetCallqueue().Remove(TriggerEndscreen);
			GetGame().GetCallqueue().Remove(SendFrameChunksWithDelay);
			GetGame().GetCallqueue().Remove(SendReplayComplete);
			GetGame().GetCallqueue().Remove(StartPlaybackForAllClients);
			GetGame().GetCallqueue().Remove(OpenMapAndStartLocalPlayback);
			GetGame().GetCallqueue().Remove(StartActualPlayback);
			GetGame().GetCallqueue().Remove(StartClientReplayPlayback);
			GetGame().GetCallqueue().Remove(WaitForReplayMapAndStartPlayback);
			GetGame().GetCallqueue().Remove(WaitForReplayMapAndStartClientPlayback);
		}

		// Unsubscribe from vehicle spawn events
		if (GetGame() && GetGame().GetGameMode())
		{
			SCR_CampaignBuildingManagerComponent buildingManager = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
			if (buildingManager)
				buildingManager.GetOnEntitySpawnedByProvider().Remove(OnVehicleSpawned);
		}

		if (SCR_TrafficEvents.OnTrafficVehicleSpawned)
			SCR_TrafficEvents.OnTrafficVehicleSpawned.Remove(OnTrafficVehicleSpawned);
		if (SCR_TrafficEvents.OnTrafficVehicleDespawned)
			SCR_TrafficEvents.OnTrafficVehicleDespawned.Remove(OnTrafficVehicleDespawned);

		// Clear replay data
		m_replayData = null;
		if (m_trackedVehicles)
			m_trackedVehicles.Clear();
		m_usedVehicleIds.Clear();
		m_pendingProjectiles.Clear();

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
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
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Instance created and initialized on %1", isServer), LogLevel.NORMAL);
		
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!m_RplComponent)
		{
			Print(string.Format("GRAD_BC_ReplayManager: Warning - No RplComponent found on %1", isServer), LogLevel.WARNING);
		}
		else
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: RplComponent found on %1", isServer), LogLevel.NORMAL);
		}
		
		// Only record on server
		if (Replication.IsServer())
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Running on server, setting up recording", LogLevel.NORMAL);
			
			// Delay initialization to ensure world is ready
			GetGame().GetCallqueue().CallLater(InitializeReplaySystem, 100, false);
		}
		else
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Running on client, ready to receive RPCs", LogLevel.NORMAL);
			// Initialize client-side replay manager to receive RPCs
			s_Instance = this;
			
			// Add debug info
			if (m_RplComponent)
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Initializing replay system", LogLevel.NORMAL);
		
		// Initialize replay data now that world should be ready
		m_replayData = GRAD_BC_ReplayData.Create();
		
		// Connect to Breaking Contact Manager events
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Hooked into vehicle spawn event", LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: Building manager not found, cannot track spawned vehicles", LogLevel.WARNING);
		}

		// Subscribe to grad-traffic vehicle spawn/despawn events (if grad-traffic mod is loaded)
		if (SCR_TrafficEvents.OnTrafficVehicleSpawned)
		{
			SCR_TrafficEvents.OnTrafficVehicleSpawned.Insert(OnTrafficVehicleSpawned);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Subscribed to grad-traffic vehicle spawn events", LogLevel.NORMAL);
		}
		if (SCR_TrafficEvents.OnTrafficVehicleDespawned)
		{
			SCR_TrafficEvents.OnTrafficVehicleDespawned.Insert(OnTrafficVehicleDespawned);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Subscribed to grad-traffic vehicle despawn events", LogLevel.NORMAL);
		}
		if (!SCR_TrafficEvents.OnTrafficVehicleSpawned && !SCR_TrafficEvents.OnTrafficVehicleDespawned)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: grad-traffic events not available, traffic vehicles won't be tracked in replay", LogLevel.NORMAL);
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

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Entity spawned: %1", spawnedEntity.GetPrefabData().GetPrefabName()), LogLevel.NORMAL);

		Vehicle vehicle = Vehicle.Cast(spawnedEntity);
		if (vehicle)
		{
			if (!m_trackedVehicles)
				m_trackedVehicles = new array<IEntity>();
			
			if (m_trackedVehicles.Find(vehicle) == -1)
			{
				m_trackedVehicles.Insert(vehicle);
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("GRAD_BC_ReplayManager: Tracked new vehicle: %1 (total: %2)", vehicle.GetPrefabData().GetPrefabName(), m_trackedVehicles.Count()), LogLevel.NORMAL);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Called by grad-traffic when a civilian traffic vehicle is spawned
	// Parameter is IEntity to match SCR_TrafficEvents.OnTrafficVehicleSpawned invoker signature
	void OnTrafficVehicleSpawned(IEntity entity)
	{
		Vehicle vehicle = Vehicle.Cast(entity);
		if (!vehicle)
			return;

		RegisterTrackedVehicle(vehicle);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Registered grad-traffic vehicle: %1", vehicle.GetPrefabData().GetPrefabName()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Called by grad-traffic when a civilian traffic vehicle is despawned
	// Parameter is IEntity to match SCR_TrafficEvents.OnTrafficVehicleDespawned invoker signature
	void OnTrafficVehicleDespawned(IEntity entity)
	{
		Vehicle vehicle = Vehicle.Cast(entity);
		if (!vehicle || !m_trackedVehicles)
			return;

		int idx = m_trackedVehicles.Find(vehicle);
		if (idx != -1)
		{
			m_trackedVehicles.Remove(idx);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Unregistered grad-traffic vehicle (total: %1)", m_trackedVehicles.Count()), LogLevel.NORMAL);
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
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Current phase: %1, Recording: %2", 
				currentPhase, m_bIsRecording), LogLevel.NORMAL);
			
		// Start recording when game begins
		if (!m_bIsRecording && currentPhase == EBreakingContactPhase.GAME)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Game phase detected, starting recording", LogLevel.NORMAL);
			StartRecording();
		}
		
		// Stop recording and start playback when game ends
		if (m_bIsRecording && currentPhase == EBreakingContactPhase.GAMEOVER)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Starting replay recording", LogLevel.NORMAL);
		m_bIsRecording = true;
		m_fLastRecordTime = world.GetWorldTime() / 1000.0; // Convert milliseconds to seconds

		// Reset capacity warning flags
		m_bWarning90Frames = false;
		m_bWarning90Memory = false;

		// Start recording loop
		GetGame().GetCallqueue().CallLater(RecordFrame, m_fRecordingInterval * 1000, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void StopRecording()
	{
		if (!m_bIsRecording)
			return;
			
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Stopping replay recording", LogLevel.NORMAL);
		m_bIsRecording = false;
		GetGame().GetCallqueue().Remove(RecordFrame);
		GetGame().GetCallqueue().Remove(CheckGameState);
		
		// Finalize replay data
		if (m_replayData && m_replayData.frames.Count() > 0)
		{
			// Remove empty frames (frames with no players/markers)
			CleanupEmptyFrames();

			if (m_replayData.frames.Count() > 0)
			{
				// Update startTime to first remaining frame to eliminate pre-game time gap
				// (startTime was set at component init, which can be minutes before GAME phase)
				GRAD_BC_ReplayFrame firstFrame = m_replayData.frames[0];
				float oldStartTime = m_replayData.startTime;
				m_replayData.startTime = firstFrame.timestamp;

				GRAD_BC_ReplayFrame lastFrame = m_replayData.frames[m_replayData.frames.Count() - 1];
				m_replayData.totalDuration = lastFrame.timestamp - m_replayData.startTime;

				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("GRAD_BC_ReplayManager: Recorded %1 frames over %2 seconds (startTime adjusted from %.2f to %.2f)",
						m_replayData.frames.Count(), m_replayData.totalDuration, oldStartTime, m_replayData.startTime), LogLevel.NORMAL);

				// Log final recording statistics
				float finalMemoryMB = CalculateReplaySizeMB();
				float framePercentUsed = m_replayData.frames.Count() / (float)m_iMaxFrames * 100.0;
				float memoryPercentUsed = finalMemoryMB / m_fMaxMemoryUsageMB * 100.0;
				Print(string.Format("[GRAD_BC_ReplayManager] Recording stopped: %1 frames (%.1f%%), %.1f MB (%.1f%%)",
					m_replayData.frames.Count(), framePercentUsed, finalMemoryMB, memoryPercentUsed), LogLevel.NORMAL);
			}
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
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Removed %1 empty frames (%2 -> %3)", 
					removedCount, originalCount, cleanedFrames.Count()), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Calculate estimated replay size in MB
	protected float CalculateReplaySizeMB()
	{
		if (!m_replayData || !m_replayData.frames)
			return 0.0;

		// Rough estimate: ~10 KB per frame on average
		// (accounts for player positions, vehicle positions, projectiles, etc.)
		return m_replayData.frames.Count() * 10.0 / 1024.0;
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

		// Check max frame limit
		int currentFrameCount = m_replayData.frames.Count();
		if (currentFrameCount >= m_iMaxFrames)
		{
			Print(string.Format("[GRAD_BC_ReplayManager] Max frame limit reached (%1 frames), stopping recording", m_iMaxFrames), LogLevel.WARNING);
			StopRecording();
			return;
		}

		// Check memory usage limit
		float currentMemoryMB = CalculateReplaySizeMB();
		if (currentMemoryMB >= m_fMaxMemoryUsageMB)
		{
			Print(string.Format("[GRAD_BC_ReplayManager] Max memory limit reached (%.1f MB), stopping recording", currentMemoryMB), LogLevel.WARNING);
			StopRecording();
			return;
		}

		// Warn at 90% capacity for frames
		float framePercentUsed = currentFrameCount / (float)m_iMaxFrames * 100.0;
		if (framePercentUsed >= 90.0 && !m_bWarning90Frames)
		{
			m_bWarning90Frames = true;
			Print(string.Format("[GRAD_BC_ReplayManager] Recording at 90%% frame capacity (%1/%2 frames)", currentFrameCount, m_iMaxFrames), LogLevel.WARNING);
		}

		// Warn at 90% capacity for memory
		float memoryPercentUsed = currentMemoryMB / m_fMaxMemoryUsageMB * 100.0;
		if (memoryPercentUsed >= 90.0 && !m_bWarning90Memory)
		{
			m_bWarning90Memory = true;
			Print(string.Format("[GRAD_BC_ReplayManager] Recording at 90%% memory capacity (%.1f/%.1f MB)", currentMemoryMB, m_fMaxMemoryUsageMB), LogLevel.WARNING);
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
		
		// Add frame to replay data
		m_replayData.frames.Insert(frame);
		m_fLastRecordTime = currentTime;
	}
	
	//------------------------------------------------------------------------------------------------
	void RecordTrackedVehicles(GRAD_BC_ReplayFrame frame)
	{
		if (!m_trackedVehicles)
			return;
			
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Recording %1 tracked vehicles", m_trackedVehicles.Count()), LogLevel.NORMAL);
	
		foreach (IEntity entity : m_trackedVehicles)
		{
			if (!entity)
				continue;
			
			Vehicle vehicle = Vehicle.Cast(entity);
			if (!vehicle)
				continue;
	
			// Record all vehicles - no exceptions
	
			// Now we have a vehicle. Record it.
			vector position = vehicle.GetOrigin();
			vector angles = vehicle.GetYawPitchRoll();
			string vehicleType = vehicle.GetPrefabData().GetPrefabName();
			
			FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
			string factionKey = "";
			if (factionComponent && factionComponent.GetAffiliatedFaction())
			{
				factionKey = factionComponent.GetAffiliatedFaction().GetFactionKey();
			}
			else
			{
				factionKey = "Empty";
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("GRAD_BC_ReplayManager: Vehicle has no FactionAffiliationComponent or AffiliatedFaction, using 'Empty'", LogLevel.NORMAL);
			}
				
			RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
			if (!rpl)
				continue;

			// Check if vehicle is empty
			BaseCompartmentManagerComponent compartmentManager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
			bool isEmpty = true;
			if (compartmentManager)
			{
				array<BaseCompartmentSlot> compartments = {};
				compartmentManager.GetCompartments(compartments);
				isEmpty = true;
				foreach (BaseCompartmentSlot slot : compartments)
				{
					if (slot.GetOccupant())
					{
						isEmpty = false;
						break;
					}
				}
			}

			// Track vehicles that have ever been used by players
			RplId vehicleRplId = rpl.Id();
			bool wasUsed = (m_usedVehicleIds.Find(vehicleRplId) != -1);
			if (!isEmpty && !wasUsed)
			{
				m_usedVehicleIds.Insert(vehicleRplId);
				wasUsed = true;
			}

			// Print(string.Format("GRAD_BC_ReplayManager: Creating snapshot for vehicle: %1", vehicleType), LogLevel.NORMAL);
			
			GRAD_BC_VehicleSnapshot snapshot = GRAD_BC_VehicleSnapshot.Create(
				vehicleRplId,
				vehicleType,
				factionKey,
				position,
				angles,
				isEmpty,
				wasUsed
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

			// vehicle info
			bool isInVehicle = character.IsInVehicle();
			string vehicleType = "";
			RplId vehicleId;
			if (isInVehicle)
			{
				Vehicle vehicle = Vehicle.Cast(character.GetParent());
				if (vehicle)
				{
					vehicleType = vehicle.GetPrefabData().GetPrefabName();
					RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
					if (rpl)
						vehicleId = rpl.Id();
				}
			}
			
			// Determine unit role based on equipment
			string unitRole = DetermineUnitRole(character);
			
			GRAD_BC_PlayerSnapshot snapshot = GRAD_BC_PlayerSnapshot.Create(
				playerId, playerName, factionKey, position, angles, isAlive, isInVehicle, vehicleType, unitRole, vehicleId
			);

			// Debug: Log vehicle state for each player (first 20 only)
			static int vehicleRecordDebugCount = 0;
			vehicleRecordDebugCount++;
			if (vehicleRecordDebugCount <= 20)
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print(string.Format("GRAD_BC_ReplayManager: Recording player %1 (%2) at position [%3, %4, %5], yaw=%6°", 
						playerId, playerName, position[0], position[1], position[2], angles[0]));
			}
			
			frame.players.Insert(snapshot);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Determine unit role using SCR_ECharacterTypeUI from SCR_CharacterUIComponent
	string DetermineUnitRole(SCR_ChimeraCharacter character)
	{
		if (!character)
			return "Rifleman";

		SCR_CharacterUIComponent charUI = SCR_CharacterUIComponent.Cast(character.FindComponent(SCR_CharacterUIComponent));
		if (charUI)
		{
			SCR_ECharacterTypeUI typeUI = charUI.GetCharacterTypes();
			switch (typeUI)
			{
				case SCR_ECharacterTypeUI.RIFLEMAN:
					return "Rifleman";
				case SCR_ECharacterTypeUI.MEDIC:
					return "Medic";
				case SCR_ECharacterTypeUI.ENGINEER:
					return "Engineer";
				case SCR_ECharacterTypeUI.GRENADIER:
					return "Grenadier";
				case SCR_ECharacterTypeUI.MACHINEGUNNER:
					return "MachineGunner";
				case SCR_ECharacterTypeUI.ANTI_TANK:
					return "AntiTank";
				case SCR_ECharacterTypeUI.SHARSHOOTER:
					return "Sharpshooter";
				case SCR_ECharacterTypeUI.PLATOON_SERGEANT:
					return "TeamLeader";
				case SCR_ECharacterTypeUI.CREW_MAN:
					return "Crew";
				case SCR_ECharacterTypeUI.SAPPER:
					return "ExplosiveSpecialist";
				case SCR_ECharacterTypeUI.OFFICER:
					return "Squadleader";
				case SCR_ECharacterTypeUI.PLATOON_LEADER:
					return "Squadleader";
				default:
					return "Rifleman";
			}
		}

		// Fallback if no UI component or unknown type
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
	void StartPlaybackForAllClients()
	{
		if (!Replication.IsServer())
			return;
			
		if (!m_replayData || m_replayData.frames.Count() == 0)
		{
			Print("GRAD_BC_ReplayManager: No replay data to send", LogLevel.WARNING);
			return;
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Starting replay transmission, %1 frames", m_replayData.frames.Count()), LogLevel.NORMAL);
	
	// Check if we have a local player controller (null on dedicated server)
	PlayerController playerController = GetGame().GetPlayerController();
	bool isDedicatedServer = (playerController == null);
	
	if (GRAD_BC_BreakingContactManager.IsDebugMode())
		PrintFormat("GRAD_BC_ReplayManager: HasPlayerController=%1, IsDedicated=%2", playerController != null, isDedicatedServer);
	
	if (!isDedicatedServer)
	{
		// We have a local player - use direct local playback
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Sending replay initialization RPC", LogLevel.NORMAL);
			Rpc(RpcAsk_StartReplayPlayback, m_replayData.totalDuration, m_replayData.missionName, m_replayData.mapName, m_replayData.startTime);
			
		// Send frames in chunks to avoid network limits with delays
		const int chunkSize = 30;
		m_iTotalChunksToSend = Math.Ceil(m_replayData.frames.Count() / 30.0);
		m_iChunksSent = 0;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Starting chunked replay data transmission - %1 total chunks", m_iTotalChunksToSend), LogLevel.NORMAL);
		SendFrameChunksWithDelay(0);
		// NOTE: Endscreen is now scheduled in SendReplayComplete() after all data is sent,
		// so clients have time to receive data and initialize before the timer starts.
		}
	}

//------------------------------------------------------------------------------------------------
void StartLocalReplayPlayback()
{
	if (GRAD_BC_BreakingContactManager.IsDebugMode())
		Print("GRAD_BC_ReplayManager: ========== Starting local single-player replay playback ==========", LogLevel.NORMAL);
		
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: CRITICAL ERROR - No replay data available!", LogLevel.ERROR);
			return;
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Replay has %1 frames, duration: %.2f seconds", 
				m_replayData.frames.Count(), m_replayData.totalDuration), LogLevel.NORMAL);
		
		// Calculate adaptive playback speed to fit within 2 minutes
		CalculateAdaptiveSpeed();

		// Show loading text in gamestate HUD first (map opens later after loading is done)
		GRAD_BC_Gamestate gamestateDisplay = FindGamestateDisplay();
		if (gamestateDisplay)
		{
			gamestateDisplay.ShowPersistentText("Preparing replay...");
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Showing replay preparation text in gamestate HUD", LogLevel.NORMAL);
		}

		// Set up VoN for cross-faction voice
		if (Replication.IsServer())
		{
			SetAllPlayersToGlobalVoN();
		}

		// Initialize playback state
		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			m_fPlaybackStartTime = world.GetWorldTime() / 1000.0;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		m_bFirstFrameDisplayed = false;

		// Delay opening the map so the gamestate loading screen is visible first
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Scheduling map open and playback start in 500ms", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(OpenMapAndStartLocalPlayback, 500, false);

		// NOTE: Endscreen timer is now scheduled in StartActualPlayback() when playback truly begins

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Local playback initialization complete", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void StartActualPlayback()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: ===== StartActualPlayback CALLED =====", LogLevel.NORMAL);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Starting actual playback sequence", LogLevel.NORMAL);
		
		// Verify world is available
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Checking if world is available...", LogLevel.NORMAL);
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("GRAD_BC_ReplayManager: World not available for playback, retrying in 500ms", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(StartActualPlayback, 500, false);
			return;
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: World is available, proceeding...", LogLevel.NORMAL);
		
		// Reset timing
		m_fPlaybackStartTime = world.GetWorldTime() / 1000.0;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Playback start time set to: %.2f", m_fPlaybackStartTime), LogLevel.NORMAL);
		
		// Enable playback flag - CRITICAL for UpdatePlayback to run
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Setting m_bIsPlayingBack to TRUE", LogLevel.NORMAL);
		m_bIsPlayingBack = true;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: m_bIsPlayingBack is now: %1", m_bIsPlayingBack), LogLevel.NORMAL);
		
		// Skip empty frames at start
		SkipEmptyFrames();
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: About to start playback loop", LogLevel.NORMAL);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Starting playback loop", LogLevel.NORMAL);
		
		// Start playback loop
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Calling GetGame().GetCallqueue().CallLater(UpdatePlayback, 100, true)", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(UpdatePlayback, 100, true);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: CallLater executed successfully", LogLevel.NORMAL);

		// Schedule endscreen now that playback has truly started
		if (m_replayData)
		{
			float replayDuration = m_replayData.totalDuration;
			float effectiveDuration = replayDuration / m_fPlaybackSpeed;
			float waitTime = (effectiveDuration + 2.0) * 1000;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Scheduling endscreen in %.1f seconds (%.0fms)", waitTime / 1000, waitTime), LogLevel.NORMAL);
			GetGame().GetCallqueue().CallLater(TriggerEndscreen, waitTime, false);
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: ===== StartActualPlayback COMPLETE =====", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Called after loading completes in local/singleplayer mode to open map and start playback
	void OpenMapAndStartLocalPlayback()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Opening map and starting local playback", LogLevel.NORMAL);

		// Keep loading text visible - it will be hidden when first frame renders
		// Open the map
		OpenMapForLocalPlayback();

		// Wait for map + replay layer to be ready before starting playback
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Waiting for map and replay layer to be ready...", LogLevel.NORMAL);
		WaitForReplayMapAndStartPlayback(0);
	}

	//------------------------------------------------------------------------------------------------
	void OpenMapForLocalPlayback()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Setting up debriefing screen for replay", LogLevel.NORMAL);

		// Clear all live markers before starting replay
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Clearing live transmission and player markers", LogLevel.NORMAL);

		// Clear live transmission markers
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
		{
			GRAD_MapMarkerManager markerMgr = GRAD_MapMarkerManager.Cast(mapEntity.GetMapModule(GRAD_MapMarkerManager));
			if (markerMgr)
			{
				markerMgr.SetReplayMode(true);
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("GRAD_BC_ReplayManager: Enabled replay mode on marker manager", LogLevel.NORMAL);
			}
		}

		// Open map fullscreen for replay visualization
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Map menu opened for replay", LogLevel.NORMAL);

		// Map readiness is now gated by WaitForReplayMapAndStartPlayback() called from the caller
	}
	
	//------------------------------------------------------------------------------------------------
	// Polls until map entity + replay layer are ready, then starts playback
	void WaitForReplayMapAndStartPlayback(int attempt)
	{
		int maxAttempts = 50; // 50 * 200ms = 10 seconds max

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
		{
			if (attempt < maxAttempts)
			{
				Print("GRAD_BC_ReplayManager: Map entity not ready yet, retrying...", LogLevel.WARNING);
				GetGame().GetCallqueue().CallLater(WaitForReplayMapAndStartPlayback, 200, false, attempt + 1);
				return;
			}
			Print("GRAD_BC_ReplayManager: Map entity not ready after max retries, forcing playback start", LogLevel.ERROR);
		}

		GRAD_BC_ReplayMapLayer replayLayer;
		if (mapEntity)
		{
			replayLayer = GRAD_BC_ReplayMapLayer.Cast(mapEntity.GetMapModule(GRAD_BC_ReplayMapLayer));
		}

		if (!replayLayer && attempt < maxAttempts)
		{
			Print("GRAD_BC_ReplayManager: Replay map layer not found yet, retrying...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(WaitForReplayMapAndStartPlayback, 200, false, attempt + 1);
			return;
		}

		if (!replayLayer)
		{
			Print("GRAD_BC_ReplayManager: Replay map layer not ready after max retries, forcing playback start", LogLevel.ERROR);
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Map and replay layer verified ready after %1 attempts", attempt), LogLevel.NORMAL);

		// Set replay mode on the layer BEFORE first frame arrives
		if (replayLayer)
		{
			replayLayer.SetReplayMode(true);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Enabled replay mode on map layer", LogLevel.NORMAL);
		}

		// Now start actual playback
		StartActualPlayback();
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetupReplayVoN()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Server received VoN setup request", LogLevel.NORMAL);
		SetAllPlayersToGlobalVoN();
	}
	
	//------------------------------------------------------------------------------------------------
	void SetAllPlayersToGlobalVoN()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Player %1 moved to global voice room", playerId), LogLevel.NORMAL);
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: %1 players in global voice room for replay", playerIds.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Send frame chunks with delays to ensure proper data arrival on clients
	void SendFrameChunksWithDelay(int currentChunkStart)
	{
		const int chunkSize = 30; // Send 30 frames at a time
		const int delayBetweenChunks = 50; // 50ms delay between chunks
		
		if (currentChunkStart >= m_replayData.frames.Count())
		{
			// All chunks sent, send completion signal after final delay
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: All chunks sent, sending completion RPC in 500ms", LogLevel.NORMAL);
			GetGame().GetCallqueue().CallLater(SendReplayComplete, 500, false);
			return;
		}
		
		int endIndex = Math.Min(currentChunkStart + chunkSize, m_replayData.frames.Count());
		m_iChunksSent++;
		float progress = m_iChunksSent / (float)m_iTotalChunksToSend;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Sending frame chunk %1-%2 (%3/%4 chunks, %.1f%%)", currentChunkStart, endIndex-1, m_iChunksSent, m_iTotalChunksToSend, progress * 100), LogLevel.NORMAL);
		SendFrameChunk(currentChunkStart, endIndex);
		
		// Update client loading progress
		Rpc(RpcAsk_UpdateLoadingProgress, progress);
		
		// Schedule next chunk
		GetGame().GetCallqueue().CallLater(SendFrameChunksWithDelay, delayBetweenChunks, false, currentChunkStart + chunkSize);
	}
	
	//------------------------------------------------------------------------------------------------
	void SendReplayComplete()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Sending completion RPC", LogLevel.NORMAL);
		Rpc(RpcAsk_ReplayDataComplete);

		// Calculate adaptive speed on server so we use the same speed clients will use
		CalculateAdaptiveSpeed();

		// Schedule endscreen AFTER all data is sent to clients
		// Add 15 second buffer for client-side initialization:
		// - 500ms CallLater in RpcAsk_ReplayDataComplete
		// - up to 10s for map/replay layer readiness polling
		// - extra margin for network latency
		if (m_replayData)
		{
			float replayDuration = m_replayData.totalDuration;
			float effectiveDuration = replayDuration / m_fPlaybackSpeed;
			float waitTime = (effectiveDuration + 15.0) * 1000; // 15 second buffer, convert to milliseconds
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: DEDICATED SERVER - Scheduling endscreen in %.1f seconds (%.0fms)", waitTime / 1000, waitTime), LogLevel.NORMAL);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Replay duration: %.2fs at %.1fx speed = %.2fs effective, buffer: 15s, total wait: %.2fs", replayDuration, m_fPlaybackSpeed, effectiveDuration, waitTime / 1000), LogLevel.NORMAL);
			GetGame().GetCallqueue().CallLater(TriggerEndscreen, waitTime, false);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: CallLater scheduled for TriggerEndscreen", LogLevel.NORMAL);
		}
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
		ref array<RplId> playerVehicleIds = {};
		
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
		ref array<bool> vehicleWasUsed = {};
		ref array<bool> vehicleIsEmpty = {};
		
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
				playerVehicleIds.Insert(playerData.vehicleId);
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
				vehicleWasUsed.Insert(vehicleData.wasUsed);
				vehicleIsEmpty.Insert(vehicleData.isEmpty);
			}
		}
		
		// Send player data
		Rpc(RpcAsk_ReceivePlayerChunk, timestamps, playerIds, positions, rotations, factions, inVehicles, playerNames, playerVehicleIds);
		
		// Send projectile data if any
		if (projTimestamps.Count() > 0)
			Rpc(RpcAsk_ReceiveProjectileChunk, projTimestamps, projTypes, projFiringPos, projImpactPos, projVelocities);
		
		// Send transmission data if any
		if (transTimestamps.Count() > 0)
			Rpc(RpcAsk_ReceiveTransmissionChunk, transTimestamps, transPositions, transStates, transProgress);

		// Send vehicle data if any
		if (vehicleTimestamps.Count() > 0)
			Rpc(RpcAsk_ReceiveVehicleChunk, vehicleTimestamps, vehicleIds, vehicleTypes, vehicleFactions, vehiclePositions, vehicleRotations, vehicleWasUsed, vehicleIsEmpty);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_StartReplayPlayback(float totalDuration, string missionName, string mapName, float startTime)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: === RPC RECEIVED === RpcAsk_StartReplayPlayback on %1",isServer), LogLevel.NORMAL);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: RpcAsk_StartReplayPlayback received - Server: %1", isServer), LogLevel.NORMAL);
		
		if (Replication.IsServer())
			return; // Don't run on server
			
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Client initializing replay data", LogLevel.NORMAL);
		
		// Initialize replay data structure
		m_replayData = GRAD_BC_ReplayData.Create();
		m_replayData.totalDuration = totalDuration;
		m_replayData.missionName = missionName;
		m_replayData.mapName = mapName;
		m_replayData.startTime = startTime;
		
		// Show loading progress in gamestate HUD
		GRAD_BC_Gamestate gamestateDisplay = FindGamestateDisplay();
		if (gamestateDisplay)
		{
			gamestateDisplay.ShowPersistentText("Replay loading... 0%");
			gamestateDisplay.UpdateProgress(0);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Showed persistent loading text in gamestate HUD", LogLevel.NORMAL);
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Client replay data initialized - Duration: %1", totalDuration), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReceiveVehicleChunk(array<float> timestamps, array<RplId> vehicleIds, array<string> vehicleTypes, array<string> vehicleFactions, array<vector> vehiclePositions, array<vector> vehicleRotations, array<bool> vehicleWasUsed, array<bool> vehicleIsEmpty)
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
			
			bool wasUsed = false;
			if (vehicleWasUsed && i < vehicleWasUsed.Count())
				wasUsed = vehicleWasUsed[i];

			bool empty = false;
			if (vehicleIsEmpty && i < vehicleIsEmpty.Count())
				empty = vehicleIsEmpty[i];

			GRAD_BC_VehicleSnapshot vehicleData = GRAD_BC_VehicleSnapshot.Create(
				vehicleIds[i],
				vehicleTypes[i],
				vehicleFactions[i],
				vehiclePositions[i],
				vehicleRotations[i],
				empty,
				wasUsed
			);
			
			frame.vehicles.Insert(vehicleData);
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Received vehicle chunk with %1 vehicles", timestamps.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReceivePlayerChunk(array<float> timestamps, array<string> playerIds, array<vector> positions, 
		array<vector> rotations, array<string> factions, array<bool> inVehicles, array<string> playerNames, array<RplId> playerVehicleIds)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
				inVehicles[i],
				"", // vehicleType is not sent, can be derived later if needed
				"Rifleman", // unitRole is not sent, can be derived later if needed
				playerVehicleIds[i]
			);
			
			frame.players.Insert(playerData);
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Received player chunk with %1 players", timestamps.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReceiveProjectileChunk(array<float> projTimestamps, array<string> projTypes, 
		array<vector> projFiringPos, array<vector> projImpactPos, array<vector> projVelocities)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Received projectile chunk with %1 projectiles", projTimestamps.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReceiveTransmissionChunk(array<float> transTimestamps, array<vector> transPositions, 
		array<int> transStates, array<float> transProgress)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Received transmission chunk with %1 transmissions", transTimestamps.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_UpdateLoadingProgress(float progress)
	{
		if (Replication.IsServer())
			return;

		// Update gamestate HUD with loading progress
		GRAD_BC_Gamestate gamestateDisplay = FindGamestateDisplay();
		if (gamestateDisplay)
			gamestateDisplay.UpdateProgress(progress);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcAsk_ReplayDataComplete()
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: === RPC RECEIVED === RpcAsk_ReplayDataComplete on %1", isServer), LogLevel.NORMAL);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: RpcAsk_ReplayDataComplete received - Server: %1", Replication.IsServer()), LogLevel.NORMAL);
		
		if (Replication.IsServer())
			return;
			
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data not available for playback!", LogLevel.ERROR);
			return;
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Client received complete replay data, %1 frames", m_replayData.frames.Count()), LogLevel.NORMAL);

		// Enable replay mode immediately for map layer
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
		{
			GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.Cast(mapEntity.GetMapModule(GRAD_BC_ReplayMapLayer));
			if (replayLayer)
			{
				replayLayer.SetReplayMode(true);
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("GRAD_BC_ReplayManager: Enabled replay mode on map layer", LogLevel.NORMAL);
			}
		}

		// Calculate adaptive playback speed
		CalculateAdaptiveSpeed();
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Calculated adaptive speed: %.2fx", m_fPlaybackSpeed), LogLevel.NORMAL);

		// Keep loading text visible - update message. It will be hidden when first frame renders.
		GRAD_BC_Gamestate gamestateDisplay = FindGamestateDisplay();
		if (gamestateDisplay)
		{
			gamestateDisplay.UpdateText("Starting replay...");
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Updated loading text to 'Starting replay...'", LogLevel.NORMAL);
		}

		// Open map and start playback
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Starting client playback in 500ms", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(StartClientReplayPlayback, 500, false);
	}
	
	//------------------------------------------------------------------------------------------------
	void StartClientReplayPlayback()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: StartClientReplayPlayback called", LogLevel.NORMAL);

		// Open map for replay viewing
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController)
		{
			Print("GRAD_BC_ReplayManager: No player controller found", LogLevel.ERROR);
			return;
		}

		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (ch)
		{
			// Player has a controlled entity - try gadget manager to open map
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Character found, opening map via gadget manager", LogLevel.NORMAL);
			bool mapOpened = false;

			SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(ch.FindComponent(SCR_GadgetManagerComponent));
			if (gadgetManager)
			{
				IEntity mapGadget = gadgetManager.GetGadgetByType(EGadgetType.MAP);
				if (mapGadget)
				{
					gadgetManager.SetGadgetMode(mapGadget, EGadgetMode.IN_HAND, true);
					mapOpened = true;
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print("GRAD_BC_ReplayManager: Map opened via gadget manager", LogLevel.NORMAL);
				}
			}

			if (!mapOpened)
			{
				Print("GRAD_BC_ReplayManager: Gadget manager fallback, opening map via menu manager", LogLevel.WARNING);
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
			}
		}
		else
		{
			// Spectator - no controlled entity - use menu manager approach
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: No controlled character (spectator), opening map via menu manager", LogLevel.NORMAL);
			GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
		}

		// Wait for map + replay layer to be ready before starting playback
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: CLIENT - Waiting for map and replay layer to be ready...", LogLevel.NORMAL);
		WaitForReplayMapAndStartClientPlayback(0);
	}

	//------------------------------------------------------------------------------------------------
	// Polls until map entity + replay layer are ready on client, then starts playback
	void WaitForReplayMapAndStartClientPlayback(int attempt)
	{
		int maxAttempts = 50; // 50 * 200ms = 10 seconds max

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
		{
			if (attempt < maxAttempts)
			{
				GetGame().GetCallqueue().CallLater(WaitForReplayMapAndStartClientPlayback, 200, false, attempt + 1);
				return;
			}
			Print("GRAD_BC_ReplayManager: CLIENT - Map entity not ready after max retries", LogLevel.ERROR);
		}

		GRAD_BC_ReplayMapLayer replayLayer;
		if (mapEntity)
		{
			replayLayer = GRAD_BC_ReplayMapLayer.Cast(mapEntity.GetMapModule(GRAD_BC_ReplayMapLayer));
		}

		if (!replayLayer && attempt < maxAttempts)
		{
			GetGame().GetCallqueue().CallLater(WaitForReplayMapAndStartClientPlayback, 200, false, attempt + 1);
			return;
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: CLIENT - Map and replay layer verified ready after %1 attempts", attempt), LogLevel.NORMAL);

		// Set replay mode on both layers
		if (replayLayer)
		{
			replayLayer.SetReplayMode(true);
		}

		if (mapEntity)
		{
			GRAD_MapMarkerManager markerMgr = GRAD_MapMarkerManager.Cast(mapEntity.GetMapModule(GRAD_MapMarkerManager));
			if (markerMgr)
			{
				markerMgr.SetReplayMode(true);
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("GRAD_BC_ReplayManager: CLIENT - Enabled replay mode on marker manager", LogLevel.NORMAL);
			}
		}

		// Start playback
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("GRAD_BC_ReplayManager: CLIENT - World not available", LogLevel.ERROR);
			return;
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: CLIENT - Setting playback state flags", LogLevel.NORMAL);
		m_bIsPlayingBack = true;
		m_bPlaybackPaused = false;
		m_fPlaybackStartTime = world.GetWorldTime() / 1000.0;
		m_fCurrentPlaybackTime = 0;
		m_iCurrentFrameIndex = 0;
		m_bFirstFrameDisplayed = false;

		// Skip empty frames at start
		SkipEmptyFrames();

		// Reset start time right before loop
		BaseWorld world2 = GetGame().GetWorld();
		if (world2)
		{
			m_fPlaybackStartTime = world2.GetWorldTime() / 1000.0;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: CLIENT - Reset playback start time to %.2f", m_fPlaybackStartTime), LogLevel.NORMAL);
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: CLIENT - Starting playback loop", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(UpdatePlayback, 100, true);

		// NOTE: Endscreen is scheduled server-side in StartPlaybackForAllClients.
		// TriggerEndscreen is server-only so no client-side scheduling needed.
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdatePlayback()
	{
		// Enhanced logging to diagnose playback issues
		if (!m_bIsPlayingBack || !m_replayData || m_bPlaybackPaused)
		{
			static int updatePlaybackSkipCounter = 0;
			updatePlaybackSkipCounter++;
			if (updatePlaybackSkipCounter % 10 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayManager: UpdatePlayback skipped (#%1) - IsPlayingBack: %2, HasData: %3, IsPaused: %4", 
					updatePlaybackSkipCounter, m_bIsPlayingBack, m_replayData != null, m_bPlaybackPaused), LogLevel.WARNING);
				if (m_replayData)
				{
					Print(string.Format("GRAD_BC_ReplayManager: Replay data exists with %1 frames, startTime: %.2f, totalDuration: %.2f", 
						m_replayData.frames.Count(), m_replayData.startTime, m_replayData.totalDuration), LogLevel.WARNING);
				}
			}
			return;
		}
		
		// Debug: Log that UpdatePlayback is running
		static int updatePlaybackCallCounter = 0;
		updatePlaybackCallCounter++;
		if (updatePlaybackCallCounter <= 5 || updatePlaybackCallCounter % 50 == 0)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print(string.Format("GRAD_BC_ReplayManager: Waiting for frame %1 (time: %.2f, current: %.2f)", 
							m_iCurrentFrameIndex, frameTimeFromStart, m_fCurrentPlaybackTime), LogLevel.NORMAL);
				}
				break; // Wait for next update
			}
		}
		
		if (framesProcessedThisUpdate > 0 && (updatePlaybackCallCounter <= 10 || updatePlaybackCallCounter % 25 == 0))
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Processed %1 frames in this update cycle", framesProcessedThisUpdate), LogLevel.NORMAL);
		}
		
		// Check if playback finished
		if (m_fCurrentPlaybackTime >= m_replayData.totalDuration)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("GRAD_BC_ReplayManager: Map entity found, searching for replay layer...", LogLevel.NORMAL);
			}
			
			GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.Cast(mapEntity.GetMapModule(GRAD_BC_ReplayMapLayer));
			if (replayLayer)
			{
				// Debug: Confirm replay layer found
				if (frameLogCounter <= 3)
				{
					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print(string.Format("GRAD_BC_ReplayManager: Replay layer found! Sending frame with %1 players", 
							frame.players.Count()), LogLevel.NORMAL);
				}
				replayLayer.UpdateReplayFrame(frame);

				// Hide loading screen after first frame is successfully sent to map layer
				if (!m_bFirstFrameDisplayed)
				{
					OnFirstFrameDisplayed();
				}
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
	// Called when the first replay frame has been successfully rendered on the map layer.
	// Hides the loading screen since markers are now visible.
	void OnFirstFrameDisplayed()
	{
		m_bFirstFrameDisplayed = true;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: First replay frame rendered, hiding loading screen", LogLevel.NORMAL);

		GRAD_BC_Gamestate gamestateDisplay = FindGamestateDisplay();
		if (gamestateDisplay)
		{
			gamestateDisplay.HideText();
		}
	}

	//------------------------------------------------------------------------------------------------
	void SetPlaybackSpeed(float speed)
	{
		m_fPlaybackSpeed = speed;
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Playback resumed", LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void StopPlayback()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: StopPlayback called", LogLevel.NORMAL);
		
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
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("GRAD_BC_ReplayManager: Disabled replay mode on marker manager", LogLevel.NORMAL);
			}
			
			GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.Cast(mapEntity.GetMapModule(GRAD_BC_ReplayMapLayer));
			if (replayLayer)
			{
				replayLayer.SetReplayMode(false);
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					Print("GRAD_BC_ReplayManager: Disabled replay mode on replay map layer", LogLevel.NORMAL);
			}
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Playback finished, closing map", LogLevel.NORMAL);
		
		// Close the map
		CloseMap();
		
		// Endscreen will be triggered by server after scheduled time
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Endscreen will be triggered by server", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Server automatically triggers endscreen after replay duration
	void TriggerEndscreen()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: ===== TriggerEndscreen CALLED =====", LogLevel.NORMAL);
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: IsServer: %1", Replication.IsServer()), LogLevel.NORMAL);
		
		if (!Replication.IsServer())
		{
			Print("GRAD_BC_ReplayManager: Not server, returning", LogLevel.WARNING);
			return;
		}
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_ReplayManager: Server triggering endscreen NOW", LogLevel.NORMAL);
		
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
		{
			bcm.SetBreakingContactPhase(EBreakingContactPhase.GAMEOVERDONE);
			bcm.ShowPostReplayGameOverScreen();
		}
		else
		{
			Print("GRAD_BC_ReplayManager: ERROR - Could not find BreakingContactManager", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CloseMap()
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			// Spectator mode - close map via menu manager
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: No player entity (spectator), closing map via menu manager", LogLevel.NORMAL);
			GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
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
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_ReplayManager: Successfully queued projectile - %1 at %2 (pending count: %3)", 
				ammoType, position.ToString(), m_pendingProjectiles.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void VerifyMapIsOpen()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_ReplayManager: Map verified as open", LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: Map not open, trying alternative method", LogLevel.WARNING);
			
			// Try using the Breaking Contact player component method
			GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
			if (playerComponent)
			{
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_ReplayManager: Replay duration %.1fs exceeds max %.1fs, using adaptive speed %.2fx", 
					actualDuration, m_fMaxReplayDuration, m_fAdaptiveSpeed), LogLevel.NORMAL);
		}
		else
		{
			m_fAdaptiveSpeed = 1.0;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
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
		if (!m_replayData)
		{
			// No replay data yet - show 0% but don't error
			return 0.0;
		}
		
		if (m_replayData.totalDuration <= 0)
		{
			// Duration not set yet - show 0%
			return 0.0;
		}
			
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
        
    if (GRAD_BC_BreakingContactManager.IsDebugMode())
        Print("GRAD_BC_ReplayManager: Checking for empty frames to skip...", LogLevel.NORMAL);
    
    int startIndex = m_iCurrentFrameIndex;
    
    for (int i = startIndex; i < m_replayData.frames.Count(); i++)
    {
        GRAD_BC_ReplayFrame frame = m_replayData.frames[i];

        // Check for player content — only players with valid positions count as visual content.
        // Vehicles/projectiles/transmissions alone don't constitute meaningful replay content
        // since they're supplementary to player markers.
        bool hasVisualContent = false;

        if (frame.players.Count() > 0)
        {
            foreach (GRAD_BC_PlayerSnapshot player : frame.players)
            {
                // Any real map position will have position.Length() in the thousands
                if (player.position.Length() > 100)
                {
                    hasVisualContent = true;
                    break;
                }
            }
        }

        if (hasVisualContent)
        {
            if (i > startIndex)
            {
                m_iCurrentFrameIndex = i;
                float frameTime = frame.timestamp - m_replayData.startTime;
                m_fCurrentPlaybackTime = frameTime;
                
                float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
                
                // NO TERNARY: Standard if-check for speed safety
                float speed = m_fPlaybackSpeed;
                if (speed == 0)
                {
                    speed = 1.0;
                }
                
                m_fPlaybackStartTime = currentWorldTime - (m_fCurrentPlaybackTime / speed);
                
                if (GRAD_BC_BreakingContactManager.IsDebugMode())
                    Print(string.Format("GRAD_BC_ReplayManager: Skipped %1 empty frames. Starting at frame %2", 
                        i - startIndex, i), LogLevel.NORMAL);
            }
            else
            {
                if (GRAD_BC_BreakingContactManager.IsDebugMode())
                    Print("GRAD_BC_ReplayManager: Content found immediately (Frame " + i + ")", LogLevel.NORMAL);
            }
            return;
        }
    }
    
    Print("GRAD_BC_ReplayManager: Warning - All remaining frames seem empty!", LogLevel.WARNING);
	}
	
	//------------------------------------------------------------------------------------------------
	// Find the GRAD_BC_Gamestate HUD display for showing loading progress
	//------------------------------------------------------------------------------------------------
	GRAD_BC_Gamestate FindGamestateDisplay()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return null;

		SCR_HUDManagerComponent hudMgr = SCR_HUDManagerComponent.Cast(pc.FindComponent(SCR_HUDManagerComponent));
		if (!hudMgr)
			return null;

		array<BaseInfoDisplay> infoDisplays = {};
		hudMgr.GetInfoDisplays(infoDisplays);

		foreach (BaseInfoDisplay baseDisp : infoDisplays)
		{
			GRAD_BC_Gamestate candidate = GRAD_BC_Gamestate.Cast(baseDisp);
			if (candidate)
				return candidate;
		}

		return null;
	}

}
