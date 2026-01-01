[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Manages replay recording and playback for Breaking Contact gamemode")]
class GRAD_BC_ReplayManagerClass : ScriptComponentClass
{
}

// Replay data structures
class GRAD_BC_ReplayFrame : Managed
{
	float timestamp;
	ref array<ref GRAD_BC_PlayerSnapshot> players = {};
	ref array<ref GRAD_BC_ProjectileSnapshot> projectiles = {};
	
	static GRAD_BC_ReplayFrame Create(float time)
	{
		GRAD_BC_ReplayFrame frame = new GRAD_BC_ReplayFrame();
		frame.timestamp = time; // Time should already be in seconds
		return frame;
	}
}

class GRAD_BC_PlayerSnapshot : Managed
{
	int playerId;
	string playerName;
	string factionKey;
	vector position;
	vector angles;
	bool isAlive;
	bool isInVehicle;
	string vehicleType; // for vehicles
	string unitRole; // detected role based on equipment
	
	static GRAD_BC_PlayerSnapshot Create(int id, string name, string faction, vector pos, vector ang, bool alive, bool inVeh = false, string vehType = "", string role = "Rifleman")
	{
		GRAD_BC_PlayerSnapshot snapshot = new GRAD_BC_PlayerSnapshot();
		snapshot.playerId = id;
		snapshot.playerName = name;
		snapshot.factionKey = faction;
		snapshot.position = pos;
		snapshot.angles = ang;
		snapshot.isAlive = alive;
		snapshot.isInVehicle = inVeh;
		snapshot.vehicleType = vehType;
		snapshot.unitRole = role;
		return snapshot;
	}
}

class GRAD_BC_ProjectileSnapshot : Managed
{
	string projectileType;
	vector position; // firing position
	vector impactPosition; // where projectile will impact/last known position
	vector velocity;
	float timeToLive; // remaining lifetime for optimization
	
	static GRAD_BC_ProjectileSnapshot Create(string type, vector pos, vector impactPos, vector vel, float ttl)
	{
		GRAD_BC_ProjectileSnapshot snapshot = new GRAD_BC_ProjectileSnapshot();
		snapshot.projectileType = type;
		snapshot.position = pos;
		snapshot.impactPosition = impactPos;
		snapshot.velocity = vel;
		snapshot.timeToLive = ttl;
		return snapshot;
	}
}

//------------------------------------------------------------------------------------------------
// Temporary data structure for projectiles fired between recording frames
class GRAD_BC_ProjectileData : Managed
{
	vector position; // firing position
	vector velocity;
	string ammoType;
	float fireTime;
}

//------------------------------------------------------------------------------------------------
class GRAD_BC_ReplayData : Managed
{
	ref array<ref GRAD_BC_ReplayFrame> frames = {};
	float totalDuration;
	string missionName;
	string mapName;
	float startTime;
	
	static GRAD_BC_ReplayData Create()
	{
		GRAD_BC_ReplayData data = new GRAD_BC_ReplayData();
		
		// Use safe world time access
		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			data.startTime = world.GetWorldTime() / 1000.0; // Convert milliseconds to seconds
		}
		else
		{
			data.startTime = 0; // Fallback if world not ready
		}
		
		MissionHeader header = GetGame().GetMissionHeader();
		if (header)
		{
			data.missionName = "Breaking Contact"; // MissionHeader.GetMissionName() doesn't exist
			data.mapName = header.GetWorldPath();
		}
		
		return data;
	}
}

// Main replay manager component
class GRAD_BC_ReplayManager : ScriptComponent
{
	[Attribute("1.0", desc: "Recording interval in seconds")]
	protected float m_fRecordingInterval;
	
	[Attribute("true", desc: "Record projectiles")]
	protected bool m_bRecordProjectiles;
	
	[Attribute("500.0", desc: "Max projectile recording distance")]
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
	
	// Projectile data pending recording
	protected ref array<ref GRAD_BC_ProjectileData> m_pendingProjectiles = {};
	
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
		
		// Set up projectile event handlers if enabled
		if (m_bRecordProjectiles)
		{
			Print("GRAD_BC_ReplayManager: Projectile recording enabled", LogLevel.NORMAL);
			// TODO: Set up projectile spawn/destroy event handlers
			// This would need engine support or world query polling
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
			GRAD_BC_ReplayFrame lastFrame = m_replayData.frames[m_replayData.frames.Count() - 1];
			m_replayData.totalDuration = lastFrame.timestamp - m_replayData.startTime;
			
			Print(string.Format("GRAD_BC_ReplayManager: Recorded %1 frames over %2 seconds", 
				m_replayData.frames.Count(), m_replayData.totalDuration), LogLevel.NORMAL);
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
		
		// Record projectiles if enabled
		if (m_bRecordProjectiles)
		{
			RecordProjectiles(frame);
		}
		
		// Add frame to replay data
		m_replayData.frames.Insert(frame);
		m_fLastRecordTime = currentTime;
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
			bool isInVehicle = character.IsInVehicle();
			string vehicleType = "";
			
			// Get vehicle info if in vehicle
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
						}
					}
				}
			}
			
			// Determine unit role based on equipment
			string unitRole = DetermineUnitRole(character);
			
			GRAD_BC_PlayerSnapshot snapshot = GRAD_BC_PlayerSnapshot.Create(
				playerId, playerName, factionKey, position, angles, isAlive, isInVehicle, vehicleType, unitRole
			);
			
			// Debug: Log recording positions for first few frames
			static int recordLogCount = 0;
			recordLogCount++;
			if (recordLogCount <= 10)
			{
				Print(string.Format("GRAD_BC_ReplayManager: Recording player %1 (%2) at position [%3, %4, %5]", 
					playerId, playerName, position[0], position[1], position[2]));
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
		Print("GRAD_BC_ReplayManager: VERSION CHECK - Single-player detection code is active", LogLevel.NORMAL);
		
	// Check if running on dedicated server
	bool isServer = Replication.IsServer();
	bool isClient = Replication.IsClient();
	bool isDedicatedServer = isServer && !isClient;
	
	PrintFormat("GRAD_BC_ReplayManager: IsServer=%1, IsClient=%2, Dedicated=%3", isServer, isClient, isDedicatedServer);
	Print("GRAD_BC_ReplayManager: Single-player/listen server detected, starting direct local playback", LogLevel.NORMAL);
	StartLocalReplayPlayback();
	
	// Dedicated server - use RPC to send replay to clients
	Print("GRAD_BC_ReplayManager: Dedicated server detected, using RPC to send replay to clients", LogLevel.NORMAL);
	
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
}

//------------------------------------------------------------------------------------------------
void StartLocalReplayPlayback()
{
	Print("GRAD_BC_ReplayManager: Starting local single-player replay playback", LogLevel.NORMAL);
		
		// Open map first
		GetGame().GetCallqueue().CallLater(OpenMapForLocalPlayback, 100, false); // Reduced from 500ms to 100ms
		m_fPlaybackStartTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
		m_fCurrentPlaybackTime = 0;
		m_iCurrentFrameIndex = 0;
		m_bPlaybackPaused = false;
		
		// Start playback after map opens
		GetGame().GetCallqueue().CallLater(StartActualPlayback, 800, false); // Reduced from 2000ms to 800ms
		
		Print("GRAD_BC_ReplayManager: Local playback initialized", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void StartActualPlayback()
	{
		Print("GRAD_BC_ReplayManager: Starting actual playback sequence", LogLevel.NORMAL);
		
		// Reset timing
		m_fPlaybackStartTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
		
		// Start playback loop
		GetGame().GetCallqueue().CallLater(UpdatePlayback, 100, true);
		
		// Show replay controls
		ShowReplayControls();
	}
	
	//------------------------------------------------------------------------------------------------
	void OpenMapForLocalPlayback()
	{
		Print("GRAD_BC_ReplayManager: Opening map for local playback", LogLevel.NORMAL);
		
		// Try to open map using player controller
		PlayerController playerController = GetGame().GetPlayerController();
		if (playerController)
		{
			IEntity playerEntity = playerController.GetControlledEntity();
			if (playerEntity)
			{
				// Try to find gadget manager component
				SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(playerEntity.FindComponent(SCR_GadgetManagerComponent));
				if (gadgetManager)
				{
					// Try to get map gadget directly
					IEntity mapGadget = gadgetManager.GetGadgetByType(EGadgetType.MAP);
					if (mapGadget)
					{
						gadgetManager.SetGadgetMode(mapGadget, EGadgetMode.IN_HAND);
						Print("GRAD_BC_ReplayManager: Map opened via gadget manager", LogLevel.NORMAL);
						return;
					}
				}
			}
		}
		
		// Fallback: try Breaking Contact player component
		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
		if (playerComponent)
		{
			playerComponent.ToggleMap(true);
			Print("GRAD_BC_ReplayManager: Map opened via Breaking Contact player component", LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_ReplayManager: Failed to open map - no suitable method found", LogLevel.ERROR);
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
		}
		
		Rpc(RpcAsk_ReceiveFrameChunk, timestamps, playerIds, positions, rotations, factions, inVehicles, playerNames);
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
	void RpcAsk_ReceiveFrameChunk(array<float> timestamps, array<string> playerIds, array<vector> positions, 
		array<vector> rotations, array<string> factions, array<bool> inVehicles, array<string> playerNames)
	{
		string isServer = "Client";
		if (Replication.IsServer()) { isServer = "Server"; }
		Print(string.Format("GRAD_BC_ReplayManager: === RPC RECEIVED === RpcAsk_ReceiveFrameChunk on %1", isServer), LogLevel.NORMAL);
		Print(string.Format("GRAD_BC_ReplayManager: RpcAsk_ReceiveFrameChunk received - Server: %1, Data points: %2", Replication.IsServer(), timestamps.Count()), LogLevel.NORMAL);
		
		if (Replication.IsServer())
			return;
			
		if (!m_replayData)
		{
			Print("GRAD_BC_ReplayManager: Replay data not initialized!", LogLevel.ERROR);
			return;
		}
		
		// Reconstruct frames from the chunk data
		// Use simple arrays instead of complex maps
		ref array<float> uniqueTimestamps = {};
		ref array<ref GRAD_BC_ReplayFrame> newFrames = {};
		
		for (int i = 0; i < timestamps.Count(); i++)
		{
			float timestamp = timestamps[i];
			
			// Find or create frame for this timestamp
			GRAD_BC_ReplayFrame frame = null;
			int frameIndex = -1;
			
			for (int j = 0; j < uniqueTimestamps.Count(); j++)
			{
				if (uniqueTimestamps[j] == timestamp)
				{
					frameIndex = j;
					frame = newFrames[j];
					break;
				}
			}
			
			if (!frame)
			{
				frame = new GRAD_BC_ReplayFrame();
				frame.timestamp = timestamp;
				uniqueTimestamps.Insert(timestamp);
				newFrames.Insert(frame);
			}
			
			// Add player data to frame
			GRAD_BC_PlayerSnapshot playerData = GRAD_BC_PlayerSnapshot.Create(
				playerIds[i].ToInt(), // Convert string back to int
				playerNames[i],
				factions[i],
				positions[i],
				rotations[i],
				true, // Assume alive for now
				inVehicles[i]
			);
			
			frame.players.Insert(playerData);
		}
		
		// Add all frames to replay data
		for (int i = 0; i < newFrames.Count(); i++)
		{
			m_replayData.frames.Insert(newFrames[i]);
		}
		
		Print(string.Format("GRAD_BC_ReplayManager: Received chunk with %1 data points", timestamps.Count()), LogLevel.NORMAL);
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
		
		// Wait longer for map to fully initialize
		GetGame().GetCallqueue().CallLater(VerifyMapIsOpen, 2000, false);
		
		// Start playback
		m_bIsPlayingBack = true;
		m_fPlaybackStartTime = GetGame().GetWorld().GetWorldTime();
		m_fCurrentPlaybackTime = 0;
		m_iCurrentFrameIndex = 0;
		
		Print("GRAD_BC_ReplayManager: Starting playback loop", LogLevel.NORMAL);
		// Start playback loop with slower update for better visibility
		GetGame().GetCallqueue().CallLater(UpdatePlayback, 200, true); // 5 FPS playback updates
		
		// Show replay controls - add delay to ensure everything is ready
		Print("GRAD_BC_ReplayManager: Scheduling UI creation", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(ShowReplayControls, 1000, false); // Longer delay for UI
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdatePlayback()
	{
		if (!m_bIsPlayingBack || !m_replayData || m_bPlaybackPaused)
		{
			static int updatePlaybackSkipCounter = 0;
			updatePlaybackSkipCounter++;
			if (updatePlaybackSkipCounter % 50 == 0)
			{
				Print(string.Format("GRAD_BC_ReplayManager: UpdatePlayback skipped - IsPlayingBack: %1, HasData: %2, IsPaused: %3", 
					m_bIsPlayingBack, m_replayData != null, m_bPlaybackPaused), LogLevel.WARNING);
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
		float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // Convert to seconds
		m_fCurrentPlaybackTime = (currentWorldTime - m_fPlaybackStartTime) * m_fPlaybackSpeed;
		
		// Find current frame to display
		while (m_iCurrentFrameIndex < m_replayData.frames.Count())
		{
			GRAD_BC_ReplayFrame frame = m_replayData.frames[m_iCurrentFrameIndex];
			float frameTimeFromStart = frame.timestamp - m_replayData.startTime;
			
			if (frameTimeFromStart <= m_fCurrentPlaybackTime)
			{
				// Display this frame
				DisplayReplayFrame(frame);
				m_iCurrentFrameIndex++;
			}
			else
			{
				break; // Wait for next update
			}
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
				// Only log warning every 20th time to reduce spam
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
		
		Print("GRAD_BC_ReplayManager: Playback finished", LogLevel.NORMAL);
		
		// Show replay finished notification to all players
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
		{
			bcm.ShowHintToAllPlayers("Replay finished. Return to main menu or wait for server restart.", "REPLAY COMPLETE", 15, false);
		}
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
		if (!m_bIsRecording || !m_bRecordProjectiles)
			return;
			
		// Store the projectile data for the next recording frame
		GRAD_BC_ProjectileData projData = new GRAD_BC_ProjectileData();
		projData.position = position;
		projData.velocity = velocity;
		projData.ammoType = ammoType;
		projData.fireTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // Convert to seconds
		
		m_pendingProjectiles.Insert(projData);
		
		Print(string.Format("GRAD_BC_ReplayManager: Queued projectile for recording - %1 at %2", 
			ammoType, position.ToString()), LogLevel.VERBOSE);
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
		
		Print("GRAD_BC_ReplayManager: Test replay UI should now be visible", LogLevel.NORMAL);
	}
}
