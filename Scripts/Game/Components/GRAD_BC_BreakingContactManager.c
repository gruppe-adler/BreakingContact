// no GAMEMASTER phase as everything should be run out of the box self explaining
enum EBreakingContactPhase
{
	LOADING,
	PREPTIME,
	OPFOR,
	BLUFOR,
	GAME,
    GAMEOVER
}

[EntityEditorProps(category: "Gruppe Adler", description: "Breaking Contact Gamemode Manager")]
class GRAD_BC_BreakingContactManagerClass : GenericEntityClass
{
}

// This class is server-only code

class GRAD_BC_BreakingContactManager : GenericEntity
{
    [Attribute(defvalue: "3", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How many transmissions are needed to win.", category: "Breaking Contact - Parameters", params: "1 3 1")]
	protected int m_iTransmissionCount;
	
	[Attribute(defvalue: "600", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How long one transmission needs to last.", category: "Breaking Contact - Parameters", params: "1 600 1")]
	protected int m_TransmissionDuration;
	
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How far away BLUFOR spawns from OPFOR.", category: "Breaking Contact - Parameters", params: "700 3000 1000")]
	protected int m_iBluforSpawnDistance;
	
	[Attribute(defvalue: "10", uiwidget: UIWidgets.Slider, enums: NULL, desc: "How long in seconds the notifications should be displayed", category: "Breaking Contact - Parameters", params: "1 30 1")]
	protected int m_iNotificationDuration;
	

    protected bool m_bluforCaptured;
    protected bool m_skipWinConditions;
	protected bool m_debug;
	
	protected string m_sWinnerSide;

	[RplProp()]
    protected vector m_vOpforSpawnPos;
	
	[RplProp()]
    protected vector m_vBluforSpawnPos;

	[RplProp(onRplName: "OnBreakingContactPhaseChanged")]
    protected EBreakingContactPhase m_iBreakingContactPhase = EBreakingContactPhase.LOADING;	

	static float m_iMaxTransmissionDistance = 500.0;

    protected ref array<IEntity> m_transmissionPoints = {};
	protected ref array<IEntity> m_iTransmissionsDone = {};
	
	protected IEntity m_radioTruck;
	protected IEntity m_westCommandVehicle;
	
	// not possible to replicate the IEntity itself!
	[RplProp()]
    protected RplId radioTruckRplId;
	[RplProp()]
    protected RplId westCommandVehRplId;
	
	protected bool m_bIsTransmittingCache;
	RplComponent m_RplComponent;
	
	
    //------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		Print(string.Format("Breaking Contact BCM -  main init -"), LogLevel.NORMAL);
		
		m_RplComponent = RplComponent.Cast(FindComponent(RplComponent));  
						
		
		// execute only on the server
		if (m_RplComponent.IsMaster()) {
			m_iNotificationDuration = 10;
			
			// check win conditions every second
			GetGame().GetCallqueue().CallLater(mainLoop, 10000, true);
			GetGame().GetCallqueue().CallLater(setPhaseInitial, 11000, false);
		}
    }
	
	
	void OnBreakingContactPhaseChanged()
	{
		Print(string.Format("Client: Notifying player of phase change: %1", SCR_Enum.GetEnumName(EBreakingContactPhase, m_iBreakingContactPhase)), LogLevel.NORMAL);
		
		string factionKey = GetPlayerFactionKey();
		
		string title = string.Format("New phase '%1' entered.", SCR_Enum.GetEnumName(EBreakingContactPhase, m_iBreakingContactPhase));
		string message = "Breaking Contact";
		
		switch (m_iBreakingContactPhase) {
			case EBreakingContactPhase.PREPTIME :
			{
				message = "Pretime still running.";
				break;
			}
			case EBreakingContactPhase.OPFOR :
			{
				message = "Opfor has to spawn now.";
				break;
			}
			case EBreakingContactPhase.BLUFOR :
			{
				message = "Blufor will spawn now.";
				break;
			}
			case EBreakingContactPhase.GAME :
			{
				message = "Blufor spawned, Game begins now.";
				break;
			}
			case EBreakingContactPhase.GAMEOVER :
			{
				message = "Game is over.";
				break;
			}
		}
		
		const int duration = 10;
		bool isSilent = false;
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController) {
			Print(string.Format("No player controller in grad playercontroller"), LogLevel.NORMAL);
			return;
		}
		
		playerController.ShowHint(message, title, duration, isSilent);
		Print(string.Format("Notifying player about phase %1", m_iBreakingContactPhase), LogLevel.NORMAL);
		
		// close map for opfor
		if (m_iBreakingContactPhase == EBreakingContactPhase.BLUFOR && factionKey == "USSR") {
			playerController.ToggleMap(false);
			playerController.setChoosingSpawn(false);
			Print(string.Format("GRAD Playercontroller PhaseChange - closing map - opfor done"), LogLevel.NORMAL);
		}
		
		// close map for blufor
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAME && factionKey == "US") {
			playerController.ToggleMap(false);
			playerController.setChoosingSpawn(false);
			Print(string.Format("GRAD Playercontroller PhaseChange - closing map - blufor done"), LogLevel.NORMAL);
		}
		
		// show logo for all
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAME) {
			Print(string.Format("GRAD Playercontroller PhaseChange - game started, show logo"), LogLevel.NORMAL);
			playerController.ShowBCLogo();
		}

	}
	
	//-----
	string GetPlayerFactionKey() {
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
				
		if (!playerController) {
			Print(string.Format("No playerController found in RpcAsk_Authority_NotifyLocalPlayerOnPhaseChange"), LogLevel.NORMAL);
			return "";
		}
		
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!ch)  {
			Print(string.Format("SCR_ChimeraCharacter missing in playerController"), LogLevel.NORMAL);
			return "";
		}
		
		string factionKey = ch.GetFactionKey();
		
		return factionKey;
	}
	
	//------------------------------------------------------------------------------------------------
	void setPhaseInitial() 
	{
		Print(string.Format("setPhaseInitial executed"), LogLevel.NORMAL);
		SetBreakingContactPhase(EBreakingContactPhase.PREPTIME);
	}
	
	//------------------------------------------------------------------------------------------------
	void InitRadioTruckMarker(IEntity entity)
	{
		Print(string.Format("Breaking Contact BCM - InitRadioTruckMarker %1", entity), LogLevel.NORMAL);
		
		
		RplId rplId = Replication.FindId(entity.FindComponent(RplComponent));
		
		Print(string.Format("Breaking Contact BCM - InitRadioTruckMarker for rplId %1", rplId), LogLevel.NORMAL);
		
		array<int> allPlayers = {};
		
		GetGame().GetPlayerManager().GetPlayers(allPlayers);
		foreach(int playerId : allPlayers)
		{
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			// todo do this on all clients
			playerController.SetIconMarker(
				"{243D963F2E18E435}UI/Textures/Map/radiotruck_active.edds",
				rplId
			);
		};
	}

    //------------------------------------------------------------------------------------------------
	bool factionEliminated(string factionName)
	{
		return (GetAlivePlayersOfSide(factionName) == 0);
	}
	
	
	//------------------------------------------------------------------------------------------------
	int GetAlivePlayersOfSide(string factionName)
	{
		array<int> alivePlayersOfSide = {};
		
		array<int> allPlayers = {};
		GetGame().GetPlayerManager().GetPlayers(allPlayers);
		foreach(int playerId : allPlayers)
		{
			// null check bc of null pointer crash
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
			if (!pc) continue;
			
			// Game Master is also a player but perhaps with no controlled entity
			IEntity controlled = pc.GetControlledEntity();
			if (!controlled) continue;
			
			// null check bc of null pointer crash
			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(controlled);
			if (!ch) continue;
			
			CharacterControllerComponent ccc = ch.GetCharacterController();
			if (factionName != ch.GetFactionKey() || ccc.IsDead()) continue;
			
			alivePlayersOfSide.Insert(playerId);
		}
		
		return alivePlayersOfSide.Count();
	}


    //------------------------------------------------------------------------------------------------
	protected void mainLoop()
	
	{

		/* 
		Print(string.Format("Breaking Contact BCM -  -------------------------------------------------"), LogLevel.NORMAL);
        Print(string.Format("Breaking Contact BCM -  Main Loop Tick ----------------------------------"), LogLevel.NORMAL);
		Print(string.Format("Breaking Contact BCM -  -------------------------------------------------"), LogLevel.NORMAL);
		*/
		
		
		// set opfor phase as soon as players leave lobby
		PS_GameModeCoop psGameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		EBreakingContactPhase currentPhase = GetBreakingContactPhase();
		if (psGameMode && currentPhase == EBreakingContactPhase.PREPTIME)
		{
			if (psGameMode.GetState() == SCR_EGameModeState.GAME)
				SetBreakingContactPhase(EBreakingContactPhase.OPFOR);
		};
		
		if (currentPhase == EBreakingContactPhase.BLUFOR) {
			
			bool isvalid = false;
			int loopcount = 0;
			while (!isvalid) {
				loopcount = loopcount + 1;
				m_vBluforSpawnPos = findBluforPosition();
			 	Print(string.Format("Breaking Contact - Blufor spawn position %1 - %2.", isvalid, loopcount), LogLevel.NORMAL);
				isvalid = SCR_WorldTools.FindEmptyTerrainPosition(m_vBluforSpawnPos, m_vBluforSpawnPos, 5, 7);
				
				
				if (loopcount > 150) { isvalid = true }; // emergency exit
			}
				
			InitiateBluforSpawn();
		}				
		
		if (m_skipWinConditions || !(GameModeStarted()))
        {
			Print(string.Format("Breaking Contact - Game not started yet"), LogLevel.NORMAL);
			return;
		};
		
        // skip win conditions if active
		if (GameModeStarted() && !(GameModeOver())) {
			ManageMarkers();
			CheckWinConditions();
			Print(string.Format("Breaking Contact - Checking Win Conditions..."), LogLevel.NORMAL);
		};
	}

	//------------------------------------------------------------------------------------------------
	int GetTransmissionsDoneCount()
	{
		return (m_iTransmissionsDone.Count());
	} 

	//------------------------------------------------------------------------------------------------
	void AddTransmissionPointDone(IEntity transmissionPoint) 
	{
		m_iTransmissionsDone.Insert(transmissionPoint);
	}
	
	
	//------------------------------------------------------------------------------------------------
	void ManageMarkers() 
	{
		Print(string.Format("Breaking Contact BCM -  Manage markers..."), LogLevel.NORMAL);
		
		if (!m_radioTruck) {
			Print(string.Format("Breaking Contact BCM -  No radio truck found"), LogLevel.NORMAL);
			return;
		};
		
		GRAD_BC_RadioTruckComponent RTC = GRAD_BC_RadioTruckComponent.Cast(m_radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
		if (!RTC) {
			Print(string.Format("Breaking Contact BCM -  No radio truck component found"), LogLevel.NORMAL);
			return;
		}
		
		bool isTransmitting = RTC.GetTransmissionActive();
		bool stateChanged = (m_bIsTransmittingCache != isTransmitting);
		m_bIsTransmittingCache = isTransmitting;
		
		IEntity nearestTPCAntenna = GetNearestTransmissionPoint(m_radioTruck.GetOrigin(), isTransmitting);
		array<IEntity> transmissionPoints = GetTransmissionPoints();
		
		if (!nearestTPCAntenna) {
			Print(string.Format("Breaking Contact RTC -  No Transmission Point found"), LogLevel.NORMAL);
			return;
		}
		
		GRAD_BC_TransmissionPointComponent activeTPC = GRAD_BC_TransmissionPointComponent.Cast(nearestTPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
		if (activeTPC) {
			if (stateChanged && isTransmitting) {
				activeTPC.SetTransmissionActive(true);
				
				Print(string.Format("Breaking Contact RTC - activating active TPC: %1 - Component: %2", nearestTPCAntenna, activeTPC), LogLevel.NORMAL);
				
				foreach (IEntity singleTPCAntenna : transmissionPoints)
				{
					// disable all others
					if (nearestTPCAntenna != singleTPCAntenna) {
						GRAD_BC_TransmissionPointComponent singleTPC = GRAD_BC_TransmissionPointComponent.Cast(singleTPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
						singleTPC.SetTransmissionActive(false);
						Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
					};
				};
				
			} else if (stateChanged && !isTransmitting) {
				foreach (IEntity singleTPCAntenna : transmissionPoints)
				{
					// disable all
					GRAD_BC_TransmissionPointComponent singleTPC = GRAD_BC_TransmissionPointComponent.Cast(singleTPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
					singleTPC.SetTransmissionActive(false);
					Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
				};
			} else {
			//	Print(string.Format("Breaking Contact RTC - No state change"), LogLevel.NORMAL);
			};
		} else {
			Print(string.Format("Breaking Contact RTC - No GRAD_BC_TransmissionPointComponent found"), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected IEntity GetNearestTransmissionPoint(vector center, bool isTransmitting)
	{
		
			array<IEntity> transmissionPoints = GetTransmissionPoints();
			IEntity selectedPoint = null;

			// if transmission points exist, find out which one is the nearest
			if (transmissionPoints.Count() > 0) {
				float distanceMaxTemp;

				foreach (IEntity TPCAntenna : transmissionPoints)
				{
					float distance = vector.Distance(TPCAntenna.GetOrigin(), center);

					// check if distance is in reach of radiotruck
					if (distance < m_iMaxTransmissionDistance) {
						distanceMaxTemp = distance;
						selectedPoint = TPCAntenna;
					}
				}
				if (!selectedPoint && isTransmitting) {
					selectedPoint = CreateTransmissionPoint(center);
				}
				return selectedPoint;
			}
			if (isTransmitting) {
				selectedPoint = CreateTransmissionPoint(center);
			}
			return selectedPoint;
	}
	
	
	//------------------------------------------------------------------------------------------------
	void SpawnSpawnVehicleWest(vector center)
	{
		vector newCenter = FindSpawnPoint(center);
		
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = newCenter;
		params.TransformMode = ETransformMode.WORLD;
		
        // create antenna that serves as component holder for transmission point
        Resource ressource = Resource.Load("{36BDCC88B17B3BFA}Prefabs/Vehicles/Wheeled/M923A1/M923A1_command.et");
        m_westCommandVehicle = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
		
		if (!m_westCommandVehicle) {
			Print(string.Format("BCM - West Command Truck failed to spawn: %1", params), LogLevel.ERROR);
			return;
		}

		RplComponent rplComponent = RplComponent.Cast(m_westCommandVehicle.FindComponent(RplComponent));
        if (rplComponent)
        {
             westCommandVehRplId = Replication.FindId(rplComponent);
             Replication.BumpMe(); // Replicate the RplId
			Print(string.Format("BCM - West Command Truck has rplComponent"), LogLevel.NORMAL);
        }
		
		SetVehiclePhysics(m_westCommandVehicle);
		
		Print(string.Format("BCM - West Command Truck spawned: %1 at %2", m_westCommandVehicle, params), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void SpawnSpawnVehicleEast(vector center)
	{
		vector newCenter = FindSpawnPoint(center);
		
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = newCenter;
		params.TransformMode = ETransformMode.WORLD;
		
        // create antenna that serves as component holder for transmission point
        Resource ressource = Resource.Load("{1BABF6B33DA0AEB6}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_command.et");
        m_radioTruck = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
		
		if (!m_radioTruck) {
			Print(string.Format("BCM - East Radio Truck failed to spawn: %1", params), LogLevel.ERROR);
			return;
		}
		
		RplComponent rplComponent = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));
        if (rplComponent)
        {
             radioTruckRplId = Replication.FindId(rplComponent);
             Replication.BumpMe(); // Replicate the RplId
			 Print(string.Format("BCM - East Radio Truck has rplComponent"), LogLevel.NORMAL);
        }
		SetVehiclePhysics(m_radioTruck);
	
		Print(string.Format("BCM - East Radio Truck spawned: %1 at %2", m_radioTruck, params), LogLevel.NORMAL);
	}
	
	
	//------------------------------------------------------------------------------------------------
	IEntity CreateTransmissionPoint(vector center) {
		// if no transmission point exists, create one
		IEntity TPCAntenna = SpawnTransmissionPoint(center, 10);
		Print(string.Format("Breaking Contact RTC -  Create TransmissionPoint: %1", TPCAntenna), LogLevel.NORMAL);
	
		
		
		GetGame().GetCallqueue().CallLater(WaitForAntennaRplId, 1000, false, TPCAntenna, 1);
		
		return TPCAntenna;
	}
	
	void WaitForAntennaRplId(IEntity entity, int timeToWaitForCheck)
	{
		RplId rplId = Replication.FindId(entity.FindComponent(RplComponent));
		
		Print(string.Format("Breaking Contact BCM - waited for antenna %1 s, its %2", timeToWaitForCheck, rplId), LogLevel.NORMAL);
		
		AddTransmissionMarker(entity, m_iMaxTransmissionDistance);
	}
	
	//------------------------------------------------------------------------------------------------
	void AddTransmissionMarker(IEntity TPCAntenna, float radius)
	{

		vector center = TPCAntenna.GetOrigin();
		RplId rplId = Replication.FindId(TPCAntenna.FindComponent(RplComponent));

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);

		foreach (int playerId : playerIds)
		{

			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));

			if (!playerController)
				return;
			
			Print(string.Format("Breaking Contact AddTransmissionMarker - Replication Id of Antenna is %1", rplId), LogLevel.WARNING);
		
			
			playerController.AddCircleMarker(
				center[0] - radius,
				center[2] + radius,
				center[0] + radius,
				center[2] + radius,
				rplId
			);
			
			playerController.SetCircleMarkerActive(rplId); // if a new transmission point is created, its active by default
			
			
			
			playerController.AddIconMarker(
				center[0] - (radius / 12),
				center[2] + (radius / 12),
				center[0] + (radius / 12),
				center[2] + (radius / 12),
				"{534DF45C06CFB00C}UI/Textures/Map/transmission_active.edds",
				rplId
			);
		}
	}

    //------------------------------------------------------------------------------------------------
	void CheckWinConditions()
	{
		// in debug mode we want to test alone without ending the game
		bool bluforEliminated = factionEliminated("US") && !m_debug;
		bool opforEliminated = factionEliminated("USSR") && !m_debug;
        bool isOver;

		bool finishedAllTransmissions = (GetTransmissionsDoneCount() >= m_iTransmissionCount);
		
		if (bluforEliminated) {
			isOver = true;
			m_sWinnerSide = "opfor";
			Print(string.Format("Breaking Contact - Blufor eliminated"), LogLevel.NORMAL);
		}

		if (finishedAllTransmissions) {
			isOver = true;
			m_sWinnerSide = "opfor";
			Print(string.Format("Breaking Contact - All transmissions done"), LogLevel.NORMAL);
		}
		
		if (opforEliminated) {
			isOver = true;
			m_sWinnerSide = "blufor";
			Print(string.Format("Breaking Contact - Opfor eliminated"), LogLevel.NORMAL);
		}

		if (m_bluforCaptured) {
			isOver = true;
			m_sWinnerSide = "blufor";
			Print(string.Format("Breaking Contact - Blufor captured radio truck"), LogLevel.NORMAL);
		}
		
		// needs to be on last position as would risk to be overwritten
		if (bluforEliminated && opforEliminated) {
			isOver = true;
			m_sWinnerSide = "draw";
			Print(string.Format("Breaking Contact - Both sides eliminated"), LogLevel.NORMAL);
		}
		
		if (isOver) {
			// show game over screen with a 20s delay
	        // GetGame().GetCallqueue().CallLater(showGameOver, 20000, false, m_winnerSide);
            SetBreakingContactPhase(EBreakingContactPhase.GAMEOVER);
			Print(string.Format("Breaking Contact - Game Over Screen TODO"), LogLevel.NORMAL);
		}
	}

    //------------------------------------------------------------------------------------------------
    protected bool GameModeStarted()
    {
        return (m_iBreakingContactPhase == EBreakingContactPhase.GAME);
    }


    //------------------------------------------------------------------------------------------------
    protected bool GameModeOver()
    {
        return (m_iBreakingContactPhase == EBreakingContactPhase.GAMEOVER);
    }


    //------------------------------------------------------------------------------------------------
	EBreakingContactPhase GetBreakingContactPhase()
	{
		Print(string.Format("GetBreakingContactPhase - Phase '%1'", m_iBreakingContactPhase, LogLevel.NORMAL));
		return m_iBreakingContactPhase;
	}
	
	//------------------------------------------------------------------------------------------------
	void Rpc_RequestInitiateOpforSpawn(vector spawnPos)
	{
		Print(string.Format("Breaking Contact - Rpc_RequestInitiateOpforSpawn"), LogLevel.NORMAL);
		m_vOpforSpawnPos = spawnPos;
		Replication.BumpMe();
		
	    TeleportFactionToMapPos("USSR", false);
		SetBreakingContactPhase(EBreakingContactPhase.BLUFOR);
	}
	
	 //------------------------------------------------------------------------------------------------
	// called by InitiateOpforSpawn - server side
	void SetBreakingContactPhase(EBreakingContactPhase phase)
    {
        m_iBreakingContactPhase = phase;
        Replication.BumpMe();

        Print(string.Format("Breaking Contact - Phase %1 entered - %2 -", SCR_Enum.GetEnumName(EBreakingContactPhase, phase), phase), LogLevel.NORMAL);
    }	
	
	//------------------------------------------------------------------------------------------------
	void InitiateBluforSpawn() 
	{
		SetBreakingContactPhase(EBreakingContactPhase.GAME);
        TeleportFactionToMapPos("US", false);
	}

	//------------------------------------------------------------------------------------------------
	IEntity SpawnTransmissionPoint(vector center, int radius)
	{		
		vector newCenter = FindSpawnPoint(center, radius);
		
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = newCenter;
		
        // create antenna that serves as component holder for transmission point
        Resource ressource = Resource.Load("{5B8922E61D8DF345}Prefabs/Props/Military/Antennas/Antenna_R161_01.et");
        IEntity transmissionPoint = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
		
		RemoveChild(transmissionPoint, false); // disable attachment hierarchy to radiotruck (?!)
		
        addTransmissionPoint(transmissionPoint); // add to array
		
		Print(string.Format("BCM - Transmission Point spawned: %1 at %2", transmissionPoint, params), LogLevel.NORMAL);
		
		return transmissionPoint;
	}

	// 
	protected vector GetNearestRoadPos(vector center) {
	
		RoadNetworkManager roadNetworkManager = GetGame().GetAIWorld().GetRoadNetworkManager();
		
		if (roadNetworkManager) {
				BaseRoad emptyRoad;
				float distanceRoad;
				auto outPoints = new array<vector>();
				// needs 2 points i presume, no documentation for this
				outPoints.Insert(Vector(0, 0, 0));
				outPoints.Insert(Vector(0, 0, 0));
				roadNetworkManager.GetClosestRoad(center, emptyRoad, distanceRoad, true);
				
				if (emptyRoad) {
					emptyRoad.GetPoints(outPoints);
					PrintFormat("BCM - found road %1 - outPoints after %2", emptyRoad, outPoints);
				
					return outPoints[0];
				}
			}
			return center;
	}

    //------------------------------------------------------------------------------------------------
    protected vector FindSpawnPoint(vector center, int minradius = -1)
    {
        bool foundSpawnPoint;
		int loopCount = 0;
		
		// (vector pos, out BaseRoad foundRoad, out float distance, bool skipNavlinks = false);
		

        while (!foundSpawnPoint) {
			
			if (minradius < 0) {
				minradius = loopCount;
			}
			
			vector roadPos = GetNearestRoadPos(center);
			// vector worldPos = {roadPos[0], GetGame().GetWorld().GetSurfaceY(roadPos[0], roadPos[2]), roadPos[2]};
            bool spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(roadPos, roadPos, minradius, 7);
			
			loopCount = loopCount + 1;	
			Print(string.Format("BCM - spawn point loop '%1 at %2, success is %3 .", loopCount, roadPos, spawnEmpty), LogLevel.NORMAL);
			
            if (spawnEmpty) {
                foundSpawnPoint = true;
				center = roadPos;
				
				Print(string.Format("BCM - spawn point found after '%1 loops'.", loopCount), LogLevel.NORMAL);
            }
			
			if (loopCount > 100) {
				return center;
				Print(string.Format("BCM - no spawn point after '%1 loops'.", loopCount), LogLevel.ERROR);
			}
        }

        return center;
    }


    //------------------------------------------------------------------------------------------------
	protected void addTransmissionPoint(IEntity transmissionPoint)
	{
        m_transmissionPoints.Insert(transmissionPoint);		
    }
	
	
	//------------------------------------------------------------------------------------------------
	array<IEntity> GetTransmissionPoints()
	{
        return m_transmissionPoints;		
    }


    //------------------------------------------------------------------------------------------------
    vector findBluforPosition() {
		bool foundPositionOnLand = false;
		int loopCount = 0;
		vector bluforSpawnPos = m_vOpforSpawnPos;
		
		while (!foundPositionOnLand) {
			
			loopCount = loopCount + 1;
			int degrees = Math.RandomIntInclusive(0, 360);
			bluforSpawnPos = GetPointOnCircle(m_vOpforSpawnPos, m_iBluforSpawnDistance, degrees);
			
			vector spawnPosOnRoad = GetNearestRoadPos(bluforSpawnPos);
			float distanceToOpfor = vector.DistanceXZ(spawnPosOnRoad, m_vOpforSpawnPos);
			Print(string.Format("BCM - blufor position distanceToOpfor %1 - degrees %2 - bluforSpawnPos %3 - spawnPosOnRoad %4 - distanceToOpfor %5 - m_vOpforSpawnPos %6 .", distanceToOpfor, degrees, bluforSpawnPos, spawnPosOnRoad, distanceToOpfor, m_vOpforSpawnPos), LogLevel.NORMAL);
			
			// we accept up to 200m closer to opfor than param
			if (distanceToOpfor >= m_iBluforSpawnDistance) {
				bluforSpawnPos = spawnPosOnRoad;
				Print(string.Format("BCM - findBluforPosition on road after '%1 loops'.", loopCount), LogLevel.NORMAL);				
				foundPositionOnLand = true;
			}
			
			if (loopCount > 150) {
				foundPositionOnLand = true;
				Print(string.Format("BCM - findBluforPosition on road NOT FOUND after '%1 loops'.", loopCount), LogLevel.ERROR);		
			}
		}

		return bluforSpawnPos;
    }
	
	// helper to find point on circle
	protected vector GetPointOnCircle(vector center, float radius, int degrees)
	{
	    // - half PI because 90 deg offset left
	    return Vector(
	        center[0] + radius * Math.Cos(degrees - 0.5 * Math.PI),
	        center[2] + radius * Math.Sin(degrees - 0.5 * Math.PI),
	        center[1] // If this is a 2D context, you can omit this line.
	    );
	}
	
	//------------------------------------------------------------------------------------------------
	void NotifyPlayerWrongRole(int playerId, string neededRole)
	{

		const string title = "Breaking Contact";
		string message = string.Format("You have the wrong role to create a teleport marker. You need to have the '%1' role.", neededRole);
		int duration = m_iNotificationDuration;
		bool isSilent = false;
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		
		if (!playerController)
			return;
	
		playerController.ShowHint(message, title, duration, isSilent);
	}


    //------------------------------------------------------------------------------------------------
	void NotifyFactionWrongPhaseForMarker(Faction faction)
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);

		const string title = "Breaking Contact";
		const string message = "You can't create the marker in this phase.";
		int duration = m_iNotificationDuration;
		bool isSilent = false;
		
		foreach (int playerId : playerIds)
		{
			if (SCR_FactionManager.SGetPlayerFaction(playerId) == faction)
			{
				SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
				
				if (!playerController)
					return;
			
				playerController.ShowHint(message, title, duration, isSilent);
			}
		}
	}


    //------------------------------------------------------------------------------------------------
	void TeleportFactionToMapPos(string factionName, bool isdebug)
	{
		Print(string.Format("Breaking Contact - TeleportFactionToMapPos"), LogLevel.NORMAL);
		
		int ExecutingPlayerId = GetGame().GetPlayerController().GetPlayerId();
		SCR_PlayerController ExecutingPlayerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(ExecutingPlayerId));
				
		if (!ExecutingPlayerController)
			return;
			
		ExecutingPlayerController.ShowHint("Teleporting to destination.", "Teleport initiated", 3, false);
		
		if (factionName == "USSR" || isdebug)
		{	
			Print(string.Format("Breaking Contact - Opfor spawn is done"), LogLevel.NORMAL);
						
			// enter debug mode
			if (isdebug) {
				m_debug = true;
			}
			
			SpawnSpawnVehicleEast(m_vOpforSpawnPos);
			
		} else {
			
			SpawnSpawnVehicleWest(m_vBluforSpawnPos);
			
			Print(string.Format("Breaking Contact - Blufor spawn is done"), LogLevel.NORMAL);
		}
		
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);
		
		foreach (int playerId : playerIds)
		{	
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			
			if (!playerController)
				return;
			
			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(playerController.GetControlledEntity());
			if (!ch)  {
				Print(string.Format("SCR_ChimeraCharacter missing in playerController"), LogLevel.NORMAL);
				return;
			}
			
			string playerFactionName = ch.GetFactionKey();
			
			
			Print(string.Format("BCM - playerFactionName %1 - factionName %2", playerFactionName, factionName), LogLevel.NORMAL);
			
			if (factionName == playerFactionName)
			{
				if (factionName == "US") {
					
					GetGame().GetCallqueue().CallLater(playerController.TeleportPlayerToMapPos, 3000, false, playerId, m_vBluforSpawnPos);
					Print(string.Format("Breaking Contact - Player with ID %1 is Member of Faction %2 and will be teleported to %3", playerId, playerFactionName, m_vBluforSpawnPos), LogLevel.NORMAL);
				} else {
					GetGame().GetCallqueue().CallLater(playerController.TeleportPlayerToMapPos, 3000, false, playerId, m_vOpforSpawnPos);
					Print(string.Format("Breaking Contact - Player with ID %1 is Member of Faction %2 and will be teleported to %3", playerId, playerFactionName, m_vOpforSpawnPos), LogLevel.NORMAL);
				}	
			}
		}
	}

	//----
	void SetVehiclePhysics(IEntity vehicle) {
		// handbrake first, saw this in BI method		
		CarControllerComponent carController = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
		// Activate handbrake so the vehicles don't go downhill on their own when spawned
		// not entirely sure why there are two functions for this, probably one for unsimulated/frozen and one for simulated
		if (carController)
			carController.SetPersistentHandBrake(true);
		
		VehicleWheeledSimulation simulation = carController.GetSimulation();
		if (simulation) {
			simulation.SetBreak(true, true);	
		}
		
		Physics physicsComponent = vehicle.GetPhysics();
		if (physicsComponent)
		{
			physicsComponent.SetVelocity(vector.Zero);
			physicsComponent.SetVelocity("0 -1 0");
			physicsComponent.SetAngularVelocity(vector.Zero);
		}
	}

    //------------------------------------------------------------------------------------------------
	vector MapPosToWorldPos(int mapPos[2])
	{	
        // get surfaceY of position mapPos X(Z)Y
		vector worldPos = {mapPos[0], GetGame().GetWorld().GetSurfaceY(mapPos[0], mapPos[1]), mapPos[1]};
		return worldPos;
	}
}
