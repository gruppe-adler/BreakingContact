[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "adds transmission point management")]
class GRAD_BC_TransmissionComponentClass : ScriptComponentClass
{
}

enum ETransmissionState
{
	OFF,
	TRANSMITTING,
	INTERRUPTED,
	DISABLED,
	DONE
}

// entity to be able to work without spawning an actual antenna
class GRAD_BC_TransmissionComponent : ScriptComponent
{
	[RplProp()]
	protected ETransmissionState m_eTransmissionState;

	[RplProp()]
	protected float m_iTransmissionProgress;

	static float m_iTransmissionDuration = 900.0; // gets overriden in init
	static float m_iTransmissionUpdateTickSize = 1.0 /m_iTransmissionDuration;

	private RplComponent m_RplComponent;

	protected bool m_bTransmissionActive;
	
	[RplProp()]
	vector m_position;

	private int m_retryCount = 0; // Instance variable for retry logic
	const int MAX_RETRIES = 50;

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("TPC EOnInit: owner=%1, position=%2, RplId=%3, IsServer=%4", owner, owner.GetOrigin(), Replication.FindItemId(owner), Replication.IsServer());
		m_position = owner.GetOrigin();
		Replication.BumpMe();
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("TPC parent: %1", owner.GetParent());

		if (!Replication.IsServer())
			return;

		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (m_RplComponent)
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("TPC RplComponent: IsMaster=%1, IsProxy=%2, IsOwner=%3", m_RplComponent.IsMaster(), m_RplComponent.IsProxy(), m_RplComponent.IsOwner());
			if (m_RplComponent.IsMaster())
			{
				// Defer registration until next frame to allow RplId to be assigned
				GetGame().GetCallqueue().CallLater(DeferredRegistration, 100, false, owner);
			}
			else
			{
				// Retry activation after a short delay
				GetGame().GetCallqueue().CallLater(DeferredActivation, 500, false, owner);
			}
		}
		else
		{
			Print("TPC RplComponent is null!", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void DeferredRegistration(IEntity owner)
	{
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm) {
			bcm.RegisterTransmissionComponent(this);
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("TPC Registered with BCM (deferred): %1, RplId=%2", bcm, Replication.FindItemId(owner));
			m_iTransmissionDuration = bcm.GetTransmissionDuration();
			m_iTransmissionUpdateTickSize = 1.0 /m_iTransmissionDuration;
			
			// Start transmission immediately
			SetTransmissionActive(true);
			// State machine tick (server only)
			GetGame().GetCallqueue().CallLater(MainLoop, 1000, true, owner);
		} else {
			Print("TPC Registration failed: BCM is null!", LogLevel.ERROR);
		}
	}
	
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	override void OnDelete(IEntity owner)
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("TPC OnDelete: owner=%1, position=%2", owner, m_position);

		// Always clean up CallLater callbacks to prevent memory leaks
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(MainLoop);
			GetGame().GetCallqueue().Remove(DeferredActivation);
			GetGame().GetCallqueue().Remove(DeferredRegistration);
		}

		if (!GetGame() || !GetGame().GetGameMode())
			return;

		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
			bcm.UnregisterTransmissionComponent(this);
		else
			Print("BCM - Warning: Could not unregister transmission component, manager not found!", LogLevel.WARNING);
	}
		
		
	//------------------------------------------------------------------------------------------------
	float GetTransmissionDuration()
	{
		//RpcAsk_Authority_SyncVariables();

		return m_iTransmissionProgress;
	}

	//------------------------------------------------------------------------------------------------
	ETransmissionState GetTransmissionState()
	{
		return m_eTransmissionState;
	}
	
	//------------------------------------------------------------------------------------------------
	bool GetTransmissionActive()
	{
		return m_bTransmissionActive;
	}
	
	vector GetPosition() 
	{
		return m_position;
	}
	
	void SetPosition(vector center) 
	{
		m_position = center;
		Replication.BumpMe();
	}

	
	//------------------------------------------------------------------------------------------------
	void SetTransmissionState(ETransmissionState transmissionState)
	{
		if (m_eTransmissionState != transmissionState) {
			ETransmissionState oldState = m_eTransmissionState;
			m_eTransmissionState = transmissionState;
			Replication.BumpMe();

			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("TPC: State changed from %1 to %2", oldState, transmissionState);

			// Notify BreakingContactManager of state change for instant map updates
			GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
			if (bcm) {
				// Update replicated marker data so clients can see markers even when out of streaming distance
				bcm.UpdateTransmissionMarkerData();
				bcm.NotifyTransmissionPointListeners();
			}

			// Show transmission hint to all players for all state changes
			PlayerManager playerManager = GetGame().GetPlayerManager();
			if (playerManager) {
				array<int> playerIds = {};
				playerManager.GetAllPlayers(playerIds);
				foreach (int playerId : playerIds) {
					PlayerController pc = playerManager.GetPlayerController(playerId);
					if (!pc) continue;
					GRAD_PlayerComponent gradPC = GRAD_PlayerComponent.Cast(pc.FindComponent(GRAD_PlayerComponent));
					if (gradPC) {
						gradPC.ShowTransmissionHintRPC(transmissionState);
					}
				}
			}
		}
	}
	
	void SetTransmissionActive(bool setState) {
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("TPC SetTransmissionActive called: setState=%1, current m_bTransmissionActive=%2, current state=%3", setState, m_bTransmissionActive, GetTransmissionState());
		
		if (m_bTransmissionActive != setState) {
			m_bTransmissionActive = setState;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("TPC SetTransmissionActive: Changed m_bTransmissionActive to %1", setState);
			
			if (m_bTransmissionActive &&
				(
					GetTransmissionState() == ETransmissionState.OFF ||
					GetTransmissionState() == ETransmissionState.INTERRUPTED
				)
			) {
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					PrintFormat("TPC SetTransmissionActive: Starting transmission (setState=true, current state=%1)", GetTransmissionState());
				SetTransmissionState(ETransmissionState.TRANSMITTING);
			};

			if (!setState &&
				(
					GetTransmissionState() == ETransmissionState.TRANSMITTING
				)
			) {
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					PrintFormat("TPC SetTransmissionActive: Interrupting transmission (setState=false, current state=%1)", GetTransmissionState());
				SetTransmissionState(ETransmissionState.INTERRUPTED);
			};
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("TPC - SetTransmissionActive final state: %1", GetTransmissionState());

		} else {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("TPC SetTransmissionActive: No change needed, setState=%1 equals current m_bTransmissionActive=%2", setState, m_bTransmissionActive);
		}
	}


	//------------------------------------------------------------------------------------------------
	protected void MainLoop(IEntity owner)
	{
		// this function runs on server-side only
	
		ETransmissionState currentState = GetTransmissionState();

		if (currentState == ETransmissionState.DONE) {
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("TPC - skipping mainloop due to transmission state done", LogLevel.NORMAL);
			return;
		}
		
		// todo make string stuff in mapmanager
        float currentProgress = Math.Floor(m_iTransmissionProgress * 100);
		string progressString = string.Format("Antenna: %1\% ...", currentProgress); // % needs to be escaped
		
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(("TPC mainloop, progress is " + progressString + " and state is " + currentState), LogLevel.NORMAL);
		
		// 
		if (currentProgress >= 100) {
			SetTransmissionState(ETransmissionState.DONE);
			currentState = GetTransmissionState(); // update to reflect this change

			GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		};
		 
		switch (currentState)
		{
	   		 case ETransmissionState.TRANSMITTING: {
		        m_iTransmissionProgress += m_iTransmissionUpdateTickSize;
				Replication.BumpMe();
				if (GRAD_BC_BreakingContactManager.IsDebugMode())
					PrintFormat("m_iTransmissionProgress %1", m_iTransmissionProgress);
	        	break;
			}
					
			case ETransmissionState.DISABLED: {
				progressString = string.Format("Antenna: %1\%", currentProgress); // % needs to be escaped
				break;
			}
	
		    case ETransmissionState.INTERRUPTED: {
				progressString = string.Format("Antenna: %1\%", currentProgress); // % needs to be escaped
				break;
			}
	        
	
	    	case ETransmissionState.DONE: {
				progressString = "Transmission Done";
		        break;
			}
	
	   	 	// Add more cases as needed
	
	   		 default: {
	        	// Handle any other state if necessary
	        	break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Called on server: broadcasts an RPC so every client spawns local debris visuals
	void BroadcastSpawnDebris(vector centerPosition, vector angles)
	{
		Rpc(RpcDo_Broadcast_SpawnAntennaDebris, centerPosition, angles);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Broadcast_SpawnAntennaDebris(vector centerPosition, vector angles)
	{
		SpawnAntennaDebrisLocal(centerPosition, angles);
	}

	private void SpawnAntennaDebrisLocal(vector centerPosition, vector angles)
	{
		array<string> debrisModels = {
			"{1B6E5E82B4E9805A}Assets/Props/Military/Antennas/Antenna_USSR_02/Dst/Antenna_USSR_02_dst_01.xob",
			"{4EA38F4B91808AA0}Assets/Props/Military/Antennas/Antenna_USSR_02/Dst/Antenna_USSR_02_dst_01_dbr_01.xob",
			"{D7BD36D28C67BB30}Assets/Props/Military/Antennas/Antenna_USSR_02/Dst/Antenna_USSR_02_dst_01_dbr_02.xob",
			"{A0B75E5A78C55440}Assets/Props/Military/Antennas/Antenna_USSR_02/Dst/Antenna_USSR_02_dst_01_dbr_03.xob"
		};

		int debrisCount = debrisModels.Count();
		for (int i = 0; i < debrisCount; i++)
		{
			string modelPath = debrisModels[i];

			float angle = Math.RandomFloatInclusive(0.0, 360.0);
			float distance = Math.RandomFloatInclusive(2.0, 8.0);

			float radians = angle * Math.DEG2RAD;
			float offsetX = distance * Math.Cos(radians);
			float offsetZ = distance * Math.Sin(radians);

			vector debrisPos = centerPosition + Vector(offsetX, 0, offsetZ);
			debrisPos[1] = GetGame().GetWorld().GetSurfaceY(debrisPos[0], debrisPos[2]) + 0.5;

			vector debrisMat[4];
			Math3D.MatrixIdentity4(debrisMat);
			debrisMat[3] = debrisPos;

			vector debrisAngles = Vector(
				Math.RandomFloatInclusive(0.0, 360.0),
				Math.RandomFloatInclusive(-30.0, 30.0),
				Math.RandomFloatInclusive(-30.0, 30.0)
			);
			Math3D.AnglesToMatrix(debrisAngles, debrisMat);
			debrisMat[3] = debrisPos;

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

			float debrisMass = Math.RandomFloatInclusive(2.0, 8.0);
			const float debrisLifetime = 30.0;
			const float debrisMaxDist = 500.0;
			const int debrisPriority = 3;

			SCR_DebrisSmallEntity.SpawnDebris(
				GetGame().GetWorld(),
				debrisMat,
				modelPath,
				debrisMass,
				debrisLifetime,
				debrisMaxDist,
				debrisPriority,
				linearVel,
				angularVel,
				"",
				false,
				SCR_EMaterialSoundTypeDebris.METAL_LIGHT,
				true
			);
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("BC Debug - Antenna debris spawned (%1 pieces)", debrisCount), LogLevel.NORMAL);
	}

	void DeferredActivation(IEntity owner)
	{
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (m_RplComponent && m_RplComponent.IsMaster())
		{
			GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
			if (bcm) {
				bcm.RegisterTransmissionComponent(this);
			}
			SetTransmissionActive(true);
			GetGame().GetCallqueue().CallLater(MainLoop, 1000, true, owner);
		}
		else
		{
			m_retryCount++;
			if (m_retryCount < MAX_RETRIES)
			{
				GetGame().GetCallqueue().CallLater(DeferredActivation, 500, false, owner);
			}
			else
			{
				Print("TPC DeferredActivation: Max retries reached, giving up", LogLevel.WARNING);
			}
		}
	}
}
