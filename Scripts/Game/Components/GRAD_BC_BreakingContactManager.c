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
	protected bool m_debug = true;
	
	protected string m_sWinnerSide;

	[RplProp()]
    protected vector m_vOpforSpawnPos;
	
	[RplProp()]
    protected vector m_vBluforSpawnPos;
	
	[RplProp()]
    protected vector m_vOpforSpawnDir;
	
	[RplProp()]
    protected vector m_vBluforSpawnDir;

	[RplProp(onRplName: "OnBreakingContactPhaseChanged")]
    protected EBreakingContactPhase m_iBreakingContactPhase = EBreakingContactPhase.LOADING;	

	static float m_iMaxTransmissionDistance = 500.0;

    protected ref array<GRAD_TransmissionPoint> m_transmissionPoints = {};
	
	protected IEntity m_radioTruck;
	protected IEntity m_westCommandVehicle;
	
	protected bool choosingBluforSpawn;
	
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
		
		// no rpc needed here, logs already on client
		SCR_HintManagerComponent.GetInstance().ShowCustomHint(message, title, duration, isSilent);
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
			
		// show logo for all
		if (m_iBreakingContactPhase == EBreakingContactPhase.GAMEOVER) {
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
		
		if (currentPhase == EBreakingContactPhase.BLUFOR && !choosingBluforSpawn) {
			Print(string.Format("Breaking Contact - choosingBluforSpawn upcoming"), LogLevel.NORMAL);
			choosingBluforSpawn = true;
			vector bluforSpawnPos = findBluforPosition();
			Print(string.Format("Breaking Contact - bluforSpawnPos %1", bluforSpawnPos), LogLevel.NORMAL);
			SetBluforSpawnPos(bluforSpawnPos);
			InitiateBluforSpawn();
		};
		
		if (m_skipWinConditions || !(GameModeStarted()))
        {
			Print(string.Format("Breaking Contact - Game not started yet"), LogLevel.NORMAL);
			return;
		};
		
		ManageMarkers();
		
        // skip win conditions if active
		if (GameModeStarted() && !(GameModeOver())) {
			CheckWinConditions();
			Print(string.Format("Breaking Contact - Checking Win Conditions..."), LogLevel.NORMAL);
		};
	}

	//------------------------------------------------------------------------------------------------
	int GetTransmissionsDoneCount()
	{
		int count = 0;
		
		foreach (ref GRAD_TransmissionPoint transmissionPoint : m_transmissionPoints)
		{
			if (transmissionPoint.GetTransmissionState() == ETransmissionState.DONE) {
				count = count + 1;
			}
		}
		return count;
	}
	
	
	//------------------------------------------------------------------------------------------------
	IEntity GetRadioTruck() 
	{
		return m_radioTruck;
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
		
		GRAD_TransmissionPoint nearestTPCAntenna = GetNearestTransmissionPoint(m_radioTruck.GetOrigin(), isTransmitting);
		array<GRAD_TransmissionPoint> transmissionPoints = GetTransmissionPoints();
		
		if (!nearestTPCAntenna) {
			Print(string.Format("Breaking Contact RTC -  No Transmission Point found"), LogLevel.NORMAL);
			return;
		}
		
		GRAD_TransmissionPoint activeTPC = nearestTPCAntenna;
		if (activeTPC) {
			if (stateChanged && isTransmitting) {
				activeTPC.SetTransmissionActive(true);
				
				Print(string.Format("Breaking Contact RTC - activating active TPC: %1 - Component: %2", nearestTPCAntenna, activeTPC), LogLevel.NORMAL);
				
				foreach (ref GRAD_TransmissionPoint singleTPCAntenna : transmissionPoints)
				{
					// disable all others
					if (nearestTPCAntenna != singleTPCAntenna) {
						GRAD_TransmissionPoint singleTPC = singleTPCAntenna;
						singleTPC.SetTransmissionActive(false);
						Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
					};
				};
				
			} else if (stateChanged && !isTransmitting) {
				foreach (ref GRAD_TransmissionPoint singleTPCAntenna : transmissionPoints)
				{
					// disable all
					GRAD_TransmissionPoint singleTPC = singleTPCAntenna;
					singleTPC.SetTransmissionActive(false);
					Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", singleTPCAntenna), LogLevel.NORMAL);
				};
			} else {
			//	Print(string.Format("Breaking Contact RTC - No state change"), LogLevel.NORMAL);
			};
		} else {
			Print(string.Format("Breaking Contact RTC - No GRAD_TransmissionPoint found"), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	GRAD_TransmissionPoint GetNearestTransmissionPoint(vector center, bool isTransmitting)
	{
		
			array<GRAD_TransmissionPoint> transmissionPoints = GetTransmissionPoints();
			GRAD_TransmissionPoint selectedPoint;
		
			Print(string.Format("Breaking Contact RTC - GetNearestTransmissionPoint"), LogLevel.NORMAL);

			// if transmission points exist, find out which one is the nearest
			if (transmissionPoints.Count() > 0) {
				float distanceMaxTemp;

				foreach (ref GRAD_TransmissionPoint TPCAntenna : transmissionPoints)
				{
					float distance = vector.Distance(TPCAntenna.GetPosition(), center);

					// check if distance is in reach of radiotruck
					if (distance < m_iMaxTransmissionDistance) {
						distanceMaxTemp = distance;
						selectedPoint = TPCAntenna;
					}
				}
				// create transmission point if player is outside existing but multiple exist
				if (!selectedPoint && isTransmitting) {
					selectedPoint = CreateTransmissionPoint(center);
				}
				return selectedPoint;
			}
			if (isTransmitting) {
				selectedPoint = CreateTransmissionPoint(center);
				Print(string.Format("Breaking Contact RTC - CreateTransmissionPoint called - transmitting"), LogLevel.NORMAL);
			}
			return selectedPoint;
	}

	
	//------------------------------------------------------------------------------------------------
	void SpawnSpawnVehicleWest()
	{
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = m_vBluforSpawnPos;
		params.TransformMode = ETransformMode.WORLD;
		
        // create antenna that serves as component holder for transmission point
        Resource ressource = Resource.Load("{36BDCC88B17B3BFA}Prefabs/Vehicles/Wheeled/M923A1/M923A1_command.et");
        m_westCommandVehicle = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
	    m_westCommandVehicle.SetYawPitchRoll(m_vBluforSpawnDir.VectorToAngles());
		
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
	void SpawnSpawnVehicleEast()
	{		
		EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = m_vOpforSpawnPos;
		params.TransformMode = ETransformMode.WORLD;
		
        // create radiotruck
        Resource ressource = Resource.Load("{1BABF6B33DA0AEB6}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_command.et");
        m_radioTruck = GetGame().SpawnEntityPrefab(ressource, GetGame().GetWorld(), params);
		m_radioTruck.SetYawPitchRoll(m_vOpforSpawnDir.VectorToAngles());
		
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
		// SetVehiclePhysics(m_radioTruck);
	
		Print(string.Format("BCM - East Radio Truck spawned: %1 at %2", m_radioTruck, params), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	GRAD_TransmissionPoint CreateTransmissionPoint(vector center) {
		// if no transmission point exists, create one
		GRAD_TransmissionPoint TPCAntenna = SpawnTransmissionPoint(center);
		Print(string.Format("Breaking Contact RTC -  Create TransmissionPoint: %1", TPCAntenna), LogLevel.NORMAL);
		
		return TPCAntenna;
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
			// local stuff is managed by breaking contact phase handler
	        GetGame().GetCallqueue().CallLater(SetBreakingContactPhase, 20000, false, EBreakingContactPhase.GAMEOVER);
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
	void Rpc_RequestInitiateOpforSpawn()
	{
		Print(string.Format("Breaking Contact - Rpc_RequestInitiateOpforSpawn"), LogLevel.NORMAL);		
	    TeleportFactionToMapPos("USSR");
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
        TeleportFactionToMapPos("US");
	}

	//------------------------------------------------------------------------------------------------
	GRAD_TransmissionPoint SpawnTransmissionPoint(vector center)
	{	
		// create antenna that serves as component holder for transmission point
        // Resource ressource = Resource.Load("{5B8922E61D8DF345}Prefabs/Props/Military/Antennas/Antenna_R161_01.et");
        // GRAD_TransmissionPoint transmissionPoint = GRAD_TransmissionPoint.Cast(GetGame().SpawnEntity(GRAD_TransmissionPoint, GetWorld(), params));
		// RemoveChild(transmissionPoint, false); // disable attachment hierarchy to radiotruck (?!)
		
		GRAD_TransmissionPoint transmissionPoint = new GRAD_TransmissionPoint();
		transmissionPoint.SetPosition(center);
		
        addTransmissionPoint(transmissionPoint); // add to array
		
		Print(string.Format("BCM - Transmission Point spawned: %1 at %2", transmissionPoint), LogLevel.NORMAL);
		
		return transmissionPoint;
	}

	// 
	protected array<vector> GetNearestRoadPos(vector center) {
		
		vector mins, maxs;
		GetGame().GetWorldEntity().GetWorldBounds(mins, maxs);
		vector worldCenter = vector.Lerp(mins, maxs, 0.5);
		
		auto worldCenterArray = new array<vector>();
		worldCenterArray.Insert(worldCenter);
		worldCenterArray.Insert(worldCenter);
		
		const float halfSize = 5000; // Adjust as needed for your AABB size
		// Adjust AABB for XZY coordinate system
		// i dont care for Z anymore now, just give me all you have 
		vector aabbMin = center - Vector(halfSize, halfSize, halfSize); 
		vector aabbMax = center + Vector(halfSize, halfSize, halfSize); 

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		RoadNetworkManager roadNetworkManager = aiWorld.GetRoadNetworkManager();
		
		if (roadNetworkManager) {
				BaseRoad emptyRoad;
				auto outPoints = new array<vector>();

				// needs 2 points i presume, no documentation for this
				// outPoints.Insert("0 0 1");
				// outPoints.Insert("1 1 1");
				auto emptyRoads = new array<BaseRoad>;
				// roadNetworkManager.GetClosestRoad(center, emptyRoad, distanceRoad, true);
				int result = roadNetworkManager.GetRoadsInAABB(aabbMin, aabbMax, emptyRoads);
			
			 	// Debug outputs
			    Print("BCM - Center: " + center.ToString());
			    Print("BCM - aabbMin: " + aabbMin.ToString());
			    Print("BCM - aabbMax: " + aabbMax.ToString());
			    Print("BCM - Result Code: " + result);
			
			    // Output the retrieved roads
			    for (int i = 0; i < emptyRoads.Count(); i++) {
			        Print("Road " + i + ": " + emptyRoads[i].ToString());
			    }
				
				if (result > 0) {
					emptyRoad = emptyRoads[0];
					emptyRoad.GetPoints(outPoints);
					PrintFormat("BCM - found road %1 - outPoints after %2", emptyRoad, outPoints);
				
					return outPoints;
				}
			}
			return worldCenterArray;
	}

    //------------------------------------------------------------------------------------------------
    protected array<vector> FindSpawnPointOnRoad(vector center)
    {
        bool foundSpawnPoint;
		int loopCount = 1;
		int minRadius = 1;
		auto roadPoints = new array<vector>;
		
        while (!foundSpawnPoint) {
			
			minRadius = loopCount;
			
			roadPoints = GetNearestRoadPos(center);
			vector roadPos = roadPoints[0];
			
			bool spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(roadPos, roadPos, minRadius, minRadius);
			
			loopCount = loopCount + 1;	
			Print(string.Format("BCM - spawn point loop '%1 at %2, success is %3, radius is %4.", loopCount, roadPos, spawnEmpty, minRadius), LogLevel.NORMAL);
			
            if (spawnEmpty) {
                foundSpawnPoint = true;
				Print(string.Format("BCM - spawn point found after '%1 loops'.", loopCount), LogLevel.NORMAL);
            }
			
			if (loopCount > 100) {
				return roadPoints;
				Print(string.Format("BCM - no spawn point after '%1 loops'.", loopCount), LogLevel.ERROR);
			}
        }

        return roadPoints;
    }


    //------------------------------------------------------------------------------------------------
	protected void addTransmissionPoint(GRAD_TransmissionPoint transmissionPoint)
	{
        m_transmissionPoints.Insert(transmissionPoint);		
    }
	
	
	//------------------------------------------------------------------------------------------------
	array<GRAD_TransmissionPoint> GetTransmissionPoints()
	{
        return m_transmissionPoints;		
    }


    //------------------------------------------------------------------------------------------------
    vector findBluforPosition() {
		bool foundPositionOnLand = false;
		int loopCount = 0;
		vector bluforSpawnPos;
		
		while (!foundPositionOnLand) {
			
			loopCount = loopCount + 1;
			int degrees = Math.RandomIntInclusive(0, 360);
			// todo m_iBluforSpawnDistance
			bluforSpawnPos = GetPointOnCircle(m_vOpforSpawnPos, 1000, degrees);
						
			float distanceToOpfor = vector.Distance(bluforSpawnPos, m_vOpforSpawnPos);
			Print(string.Format("BCM - loopCount %1 - distanceToOpfor %2 - bluforSpawnPos %4 - m_vOpforSpawnPos %5 - degrees %3.", loopCount, distanceToOpfor, degrees, bluforSpawnPos, m_vOpforSpawnPos), LogLevel.NORMAL);
			
			// minimum of blufor spawn distance is set value - 200
			if (!SurfaceIsWater(bluforSpawnPos)) {
				Print(string.Format("BCM - findBluforPosition after '%1 loops'.", loopCount), LogLevel.NORMAL);				
				foundPositionOnLand = true;
			}
			
			if (loopCount > 100) {
				foundPositionOnLand = true;
				Print(string.Format("BCM - findBluforPosition panic exit after '%1 loops'.", loopCount), LogLevel.ERROR);		
			}
		}

		return bluforSpawnPos;
    }
	
	// helper to find point on circle
	protected vector GetPointOnCircle(vector center, int radius, int degrees)
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
	void TeleportFactionToMapPos(string factionName)
	{
		if (factionName == "USSR")
		{	
			Print(string.Format("Breaking Contact - Opfor spawn is done"), LogLevel.NORMAL);	
			SpawnSpawnVehicleEast();
			
		}
		if (factionName == "US") {	
			Print(string.Format("Breaking Contact - Blufor spawn is done"), LogLevel.NORMAL);
			SpawnSpawnVehicleWest();
		}
		if (factionName != "US" && factionName != "USSR") {
			Print(string.Format("Breaking Contact - PANIC, faction is %1", factionName), LogLevel.NORMAL);
			return;
		}
		
		PS_PlayableManager m_PlayableManager = PS_PlayableManager.GetInstance();
		array<PS_PlayableContainer> playables = m_PlayableManager.GetPlayablesSorted();
		int index = 10000;
		
		foreach (PS_PlayableContainer playableCont : playables)
		{	
			index = index + 1;
			
			PS_PlayableComponent playableComp = playableCont.GetPlayableComponent();
			
			FactionAffiliationComponent factionComp = playableComp.GetFactionAffiliationComponent();
			if (!factionComp) {
				Print(string.Format("BCM - no factionComp"), LogLevel.ERROR);
				return;
			}
			
			string playerFactionName = factionComp.GetAffiliatedFaction().GetFactionKey();			
			Print(string.Format("BCM - playerFactionName %1 - factionName %2", playerFactionName, factionName), LogLevel.NORMAL);
			
			if (factionName == playerFactionName)
			{
				IEntity playerInstance = playableComp.GetOwnerCharacter();
				// delay is because spawn vehicle opfor needs to find road position and will change opfor spawn pos once again
				if (factionName == "US") {
					GetGame().GetCallqueue().CallLater(TeleportPlayerInstanceToMapPos, index, false, playerInstance, m_vBluforSpawnPos);
					Print(string.Format("BCM - ID %1 is %2, moves to %3 after %4 s", playerInstance, playerFactionName, m_vBluforSpawnPos, index), LogLevel.NORMAL);
				} else {
					GetGame().GetCallqueue().CallLater(TeleportPlayerInstanceToMapPos, index, false, playerInstance, m_vOpforSpawnPos);
					Print(string.Format("BCM - ID %1 is %2, moves to %3 after %4 s", playerInstance, playerFactionName, m_vOpforSpawnPos, index), LogLevel.NORMAL);
				}	
			}
		}
	}
	
	// Server side!
	protected void TeleportPlayerInstanceToMapPos(IEntity playerInstance, vector spawnPos)
	{
		// executed locally on players machine
	
		bool spawnEmpty;
		int spawnSearchLoop = 0;
		vector foundSpawnPos;
		
		while (!spawnEmpty)
		{
			int radius = 3 + spawnSearchLoop; // increasing each loop

			spawnSearchLoop = spawnSearchLoop + 1;
			spawnEmpty = SCR_WorldTools.FindEmptyTerrainPosition(foundSpawnPos, spawnPos, radius*2);
			Print(string.Format("GRAD playerInstance - foundSpawnPos %1 - spawnEmpty %2 - spawnSearchLoop %3", foundSpawnPos, spawnEmpty, spawnSearchLoop), LogLevel.NORMAL);
			// Print(string.Format("GRAD playerInstance - spawnSearchLoop %1", spawnSearchLoop), LogLevel.NORMAL);
			
			if (spawnSearchLoop > 150) {
				foundSpawnPos = spawnPos;
				Print(string.Format("GRAD playerInstance - not found a position after %1 tries and radius %2", spawnSearchLoop, radius), LogLevel.ERROR);
			}
		}
		
		SCR_Global.TeleportLocalPlayer(foundSpawnPos, SCR_EPlayerTeleportedReason.DEFAULT);
		
		/*
		// cause of crash?
		BaseGameEntity baseGameEntity = BaseGameEntity.Cast(playerInstance);
		if (baseGameEntity && !BaseVehicle.Cast(baseGameEntity))
		{
			baseGameEntity.Teleport(foundSpawnPos);
			Print(string.Format("GRAD player teleport successful to %1 while spawnPos was %2", foundSpawnPos, spawnPos), LogLevel.NORMAL);
		}
		else
		{
			playerInstance.SetWorldTransform(foundSpawnPos);
			Print(string.Format("GRAD AI teleport successful to %1 while spawnPos was %2", foundSpawnPos, spawnPos), LogLevel.NORMAL);
		}
		*/
		
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
			physicsComponent.SetActive(ActiveState.ACTIVE);
			physicsComponent.SetVelocity(vector.Zero);
			physicsComponent.SetVelocity("0 -1 0");
			physicsComponent.SetAngularVelocity(vector.Zero);
		}
	}
	
	void SetOpforSpawnPos(vector spawnPos) {		
		array<vector> roadPositions = FindSpawnPointOnRoad(spawnPos);
		m_vOpforSpawnDir = vector.Direction(roadPositions[0], roadPositions[1]);
		vector midpoint = vector.Lerp(roadPositions[0], roadPositions[1], 0.5);
		
		m_vOpforSpawnPos = midpoint; // midpoint;
		
		Replication.BumpMe();
	}
	
	void SetBluforSpawnPos(vector spawnPos) {
		array<vector> roadPositions = FindSpawnPointOnRoad(spawnPos);
		m_vBluforSpawnDir = vector.Direction(roadPositions[0], roadPositions[1]);
		vector midpoint = vector.Lerp(roadPositions[0], roadPositions[1], 0.5);
		
		m_vBluforSpawnPos = midpoint; // midpoint;
		
		Replication.BumpMe();
	}

    //------------------------------------------------------------------------------------------------
	vector MapPosToWorldPos(int mapPos[2])
	{	
        // get surfaceY of position mapPos X(Z)Y
		vector worldPos = {mapPos[0], GetGame().GetWorld().GetSurfaceY(mapPos[0], mapPos[1]), mapPos[1]};
		return worldPos;
	}
	
	//------------------------------------------------------------------------------------------------
	static bool SurfaceIsWater(vector pos)
	{
	    pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
	    vector outWaterSurfacePoint;
	    EWaterSurfaceType outType;
	    vector transformWS[4];
	    vector obbExtents;
	    ChimeraWorldUtils.TryGetWaterSurface(GetGame().GetWorld(), pos, outWaterSurfacePoint, outType, transformWS, obbExtents);
	    return outType != EWaterSurfaceType.WST_NONE;
	}
}
