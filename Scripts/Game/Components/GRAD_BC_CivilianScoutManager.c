// Manager component that spawns a civilian scout vehicle after the radio truck has
// been non-transmitting for a configurable period. A civilian AI driver is spawned
// inside the vehicle, which then drives toward the radio truck via the AI waypoint
// system. Once the scout has line-of-sight to the truck every player receives a
// hint and a map marker.
//
// Attach this component to the same game-mode entity as GRAD_BC_BreakingContactManager.

[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Spawns a civilian scout when the radio truck has not transmitted for a set period.")]
class GRAD_BC_CivilianScoutManagerClass : ScriptComponentClass {}

class GRAD_BC_CivilianScoutManager : ScriptComponent
{
	// -------------------------------------------------------------------------
	// Configurable attributes
	// -------------------------------------------------------------------------

	[Attribute(defvalue: "600", uiwidget: UIWidgets.Slider, desc: "Seconds of no transmission during GAME phase before the civilian scout is triggered.", params: "60 1800 60", category: "Breaking Contact - Civilian Scout")]
	protected int m_iNoTransmissionTriggerTime;

	[Attribute("{57A441224AC02CF3}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_Randomized.et", UIWidgets.ResourcePickerThumbnail, "Civilian vehicle prefab to spawn as the scout.", params: "et", category: "Breaking Contact - Civilian Scout")]
	protected ResourceName m_sScoutVehiclePrefab;

	[Attribute("{22E43956740A6794}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Randomized.et", UIWidgets.ResourcePickerThumbnail, "Civilian character prefab to spawn as the driver.", params: "et", category: "Breaking Contact - Civilian Scout")]
	protected ResourceName m_sScoutDriverPrefab;

	[Attribute("{000CD338713F2B5A}Prefabs/Groups/Group_Base.et", UIWidgets.ResourcePickerThumbnail, "AI group prefab used to assign waypoints to the driver.", params: "et", category: "Breaking Contact - Civilian Scout")]
	protected ResourceName m_sScoutGroupPrefab;

	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourcePickerThumbnail, "Waypoint prefab used to direct the driver toward the radio truck.", params: "et", category: "Breaking Contact - Civilian Scout")]
	protected ResourceName m_sScoutWaypointPrefab;

	[Attribute("{15A293B1904F942B}UI/Textures/Icons/icon_objective.edds", UIWidgets.ResourcePickerThumbnail, "Map icon texture shown when the civilian reports the radio truck (placeholder until dedicated texture is provided).", params: "edds", category: "Breaking Contact - Civilian Scout")]
	protected ResourceName m_sScoutIconTexture;

	// -------------------------------------------------------------------------
	// Constants
	// -------------------------------------------------------------------------

	// How far from the radio truck the scout spawns (metres).
	static const float SCOUT_SPAWN_DISTANCE = 2000.0;

	// Maximum spawn distance when expanding the ring search (metres).
	static const float SCOUT_SPAWN_DISTANCE_MAX = 4000.0;

	// Step added to the ring when no valid spawn was found at the current distance (metres).
	static const float SCOUT_SPAWN_DISTANCE_STEP = 500.0;

	// Height offset (metres) used when evaluating line-of-sight from the scout or truck.
	static const float LOS_HEIGHT_OFFSET = 2.0;

	// Small height added above ground when placing the scout at a spawn position
	// to prevent the vehicle from clipping into terrain.
	static const float SPAWN_GROUND_OFFSET = 0.5;

	// Approximate eye height of a standing player character (metres) used for LoS checks.
	static const float PLAYER_EYE_HEIGHT = 1.7;

	// Interval (ms) at which the LoS check is performed.
	static const int LOS_CHECK_MS = 5000;

	// Distance (metres) the scout must be from the truck before reporting.
	static const float LOS_TRIGGER_DISTANCE = 100.0;

	// How long the map marker stays visible (seconds).
	static const float MARKER_LIFETIME_S = 180.0;

	// Interval (ms) at which the waypoint is updated to follow the moving truck.
	static const int WAYPOINT_UPDATE_MS = 60000;

	// Minimum truck movement (metres) required to bother updating the waypoint.
	static const float WAYPOINT_UPDATE_MIN_MOVE = 100.0;

	// Interval (ms) at which the return-to-spawn despawn check is polled.
	static const int DESPAWN_CHECK_MS = 5000;

	// Distance (metres) from spawn position within which the scout is considered "home".
	static const float DESPAWN_HOME_RADIUS = 50.0;

	// -------------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------------

	protected RplComponent m_RplComponent;

	// Seconds elapsed without an active transmission during GAME phase.
	protected float m_fNoTransmissionSeconds = 0.0;

	// Whether a scout vehicle is currently alive and heading toward the truck.
	protected bool m_bScoutActive = false;

	// Whether the report has already been broadcast for this activation.
	protected bool m_bScoutReported = false;

	// Prevents re-triggering during the same non-transmission window.
	protected bool m_bTriggered = false;

	protected IEntity m_ScoutVehicle = null;
	protected IEntity m_ScoutDriver = null;
	protected AIGroup m_ScoutGroup = null;
	protected AIWaypoint m_CurrentWaypoint = null;
	protected vector m_vLastWaypointTruckPos = vector.Zero;
	protected vector m_vSpawnPos = vector.Zero;

	// -------------------------------------------------------------------------
	// Lifecycle
	// -------------------------------------------------------------------------

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!m_RplComponent)
			Print("GRAD_BC_CivilianScoutManager: Warning - No RplComponent found on owner; RPCs will not work.", LogLevel.WARNING);

		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(TrackingLoop, 1000, true);
	}

	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(TrackingLoop);
			GetGame().GetCallqueue().Remove(AssignScoutWaypoint);
			GetGame().GetCallqueue().Remove(ScoutLoSCheck);
			GetGame().GetCallqueue().Remove(DespawnWhenSafe);
			GetGame().GetCallqueue().Remove(DespawnScout);
		}

		DespawnScout();
		super.OnDelete(owner);
	}

	// -------------------------------------------------------------------------
	// Server-side tracking loop (runs every second)
	// -------------------------------------------------------------------------

	protected void TrackingLoop()
	{
		if (!Replication.IsServer())
			return;

		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
			return;

		// Only active during GAME phase.
		EBreakingContactPhase phase = bcm.GetBreakingContactPhase();
		if (phase != EBreakingContactPhase.GAME)
			return;

		IEntity radioTruck = bcm.GetRadioTruck();
		if (!radioTruck)
			return;

		GRAD_BC_RadioTruckComponent rtc = GRAD_BC_RadioTruckComponent.Cast(radioTruck.FindComponent(GRAD_BC_RadioTruckComponent));
		if (!rtc)
			return;

		bool isTransmitting = rtc.GetTransmissionActive();

		if (isTransmitting)
		{
			// Reset when transmission becomes active again.
			m_fNoTransmissionSeconds = 0.0;
			m_bTriggered = false;
		}
		else if (!m_bTriggered && !m_bScoutActive)
		{
			m_fNoTransmissionSeconds += 1.0;

			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print(string.Format("GRAD_BC_CivilianScoutManager: No-transmission timer: %1s / %2s", m_fNoTransmissionSeconds, m_iNoTransmissionTriggerTime), LogLevel.NORMAL);

			if (m_fNoTransmissionSeconds >= m_iNoTransmissionTriggerTime)
			{
				m_bTriggered = true;
				TriggerCivilianScout(radioTruck);
			}
		}
	}

	// -------------------------------------------------------------------------
	// Scout activation
	// -------------------------------------------------------------------------

	protected void TriggerCivilianScout(IEntity radioTruck)
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_CivilianScoutManager: Triggering civilian scout.", LogLevel.NORMAL);

		vector truckPos = radioTruck.GetOrigin();

		vector spawnPos = FindScoutSpawnPosition(truckPos);
		if (spawnPos == vector.Zero)
		{
			Print("GRAD_BC_CivilianScoutManager: Could not find a valid spawn position; scout will not spawn.", LogLevel.WARNING);
			return;
		}

		m_vSpawnPos = spawnPos;

		// Build a transform oriented toward the truck.
		vector forward = truckPos - spawnPos;
		forward[1] = 0;
		forward.Normalize();
		if (forward.LengthSq() < 0.0001)
			forward = "0 0 1";

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.DirectionAndUpMatrix(forward, "0 1 0", spawnParams.Transform);
		spawnParams.Transform[3] = spawnPos;

		// 1. Spawn group.
		IEntity groupEnt = GetGame().SpawnEntityPrefab(Resource.Load(m_sScoutGroupPrefab), GetGame().GetWorld(), spawnParams);
		m_ScoutGroup = AIGroup.Cast(groupEnt);
		if (!m_ScoutGroup)
		{
			Print("GRAD_BC_CivilianScoutManager: Failed to spawn AI group.", LogLevel.ERROR);
			return;
		}

		// Assign CIV faction and prevent auto-deletion when waypoints complete.
		SCR_AIGroup scrGroup = SCR_AIGroup.Cast(groupEnt);
		if (scrGroup)
		{
			scrGroup.SetDeleteWhenEmpty(false);

			FactionManager factionMgr = GetGame().GetFactionManager();
			if (factionMgr)
			{
				Faction civFaction = factionMgr.GetFactionByKey("CIV");
				if (civFaction)
					scrGroup.SetFaction(civFaction);
				else
					Print("GRAD_BC_CivilianScoutManager: CIV faction not found.", LogLevel.WARNING);
			}
		}
		Print(string.Format("GRAD_BC_CivilianScoutManager: SCR_AIGroup cast succeeded=%1.", (scrGroup != null).ToString()), LogLevel.NORMAL);

		// Prevent combat logic so the driver only follows waypoints.
		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(groupEnt.FindComponent(SCR_AIGroupUtilityComponent));
		if (utility)
			utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);

		// 2. Spawn vehicle.
		m_ScoutVehicle = GetGame().SpawnEntityPrefab(Resource.Load(m_sScoutVehiclePrefab), GetGame().GetWorld(), spawnParams);
		if (!m_ScoutVehicle)
		{
			Print("GRAD_BC_CivilianScoutManager: Failed to spawn scout vehicle.", LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(groupEnt);
			m_ScoutGroup = null;
			return;
		}

		Print(string.Format("GRAD_BC_CivilianScoutManager: Scout vehicle spawned at %1.", spawnPos.ToString()), LogLevel.NORMAL);

		// 3. Spawn driver at the same position (will be force-teleported into seat).
		m_ScoutDriver = GetGame().SpawnEntityPrefab(Resource.Load(m_sScoutDriverPrefab), GetGame().GetWorld(), spawnParams);
		if (!m_ScoutDriver)
		{
			Print("GRAD_BC_CivilianScoutManager: Failed to spawn scout driver.", LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(m_ScoutVehicle);
			m_ScoutVehicle = null;
			SCR_EntityHelper.DeleteEntityAndChildren(groupEnt);
			m_ScoutGroup = null;
			return;
		}
		Print(string.Format("GRAD_BC_CivilianScoutManager: Scout driver spawned at %1.", m_ScoutDriver.GetOrigin().ToString()), LogLevel.NORMAL);

		// 4. Link driver agent to group.
		AIControlComponent aiControl = AIControlComponent.Cast(m_ScoutDriver.FindComponent(AIControlComponent));
		if (aiControl)
		{
			AIAgent agent = aiControl.GetControlAIAgent();
			if (agent)
			{
				agent.PreventMaxLOD();
				m_ScoutGroup.AddAgent(agent);
				Print("GRAD_BC_CivilianScoutManager: Driver AIAgent linked to group.", LogLevel.NORMAL);
			}
			else
			{
				Print("GRAD_BC_CivilianScoutManager: Driver has no AIAgent — driver will not navigate.", LogLevel.WARNING);
			}
		}
		else
		{
			Print("GRAD_BC_CivilianScoutManager: Driver has no AIControlComponent — driver will not navigate.", LogLevel.WARNING);
		}

		// 5. Force-teleport driver into the pilot compartment.
		BaseCompartmentManagerComponent compartmentMgr = BaseCompartmentManagerComponent.Cast(m_ScoutVehicle.FindComponent(BaseCompartmentManagerComponent));
		CompartmentAccessComponent compartmentAccess = CompartmentAccessComponent.Cast(m_ScoutDriver.FindComponent(CompartmentAccessComponent));
		Print(string.Format("GRAD_BC_CivilianScoutManager: CompartmentMgr=%1 CompartmentAccess=%2.", (compartmentMgr != null).ToString(), (compartmentAccess != null).ToString()), LogLevel.NORMAL);
		if (compartmentMgr && compartmentAccess)
		{
			array<BaseCompartmentSlot> compartments = {};
			compartmentMgr.GetCompartments(compartments);
			Print(string.Format("GRAD_BC_CivilianScoutManager: Vehicle has %1 compartment(s).", compartments.Count().ToString()), LogLevel.NORMAL);
			bool pilotFound = false;
			foreach (BaseCompartmentSlot slot : compartments)
			{
				Print(string.Format("GRAD_BC_CivilianScoutManager:   Slot type=%1 occupied=%2.", slot.GetType().ToString(), slot.IsOccupied().ToString()), LogLevel.NORMAL);
				if (slot.GetType() == ECompartmentType.PILOT)
				{
					pilotFound = true;
					bool entered = compartmentAccess.GetInVehicle(m_ScoutVehicle, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
					Print(string.Format("GRAD_BC_CivilianScoutManager: GetInVehicle(PILOT) returned %1.", entered.ToString()), LogLevel.NORMAL);
					break;
				}
			}
			if (!pilotFound)
				Print("GRAD_BC_CivilianScoutManager: No PILOT compartment found on vehicle.", LogLevel.WARNING);
		}
		else
		{
			Print("GRAD_BC_CivilianScoutManager: Missing CompartmentAccessComponent or BaseCompartmentManagerComponent.", LogLevel.WARNING);
		}

		// 6. Restart AI so it recognises the vehicle context, then start the engine.
		if (aiControl)
		{
			AIAgent agent = aiControl.GetControlAIAgent();
			if (agent)
			{
				agent.DeactivateAI();
				agent.ActivateAI();
			}
			Print(string.Format("GRAD_BC_CivilianScoutManager: AI restarted, IsAIActivated=%1.", aiControl.IsAIActivated().ToString()), LogLevel.NORMAL);
		}

		CarControllerComponent carController = CarControllerComponent.Cast(m_ScoutVehicle.FindComponent(CarControllerComponent));
		Print(string.Format("GRAD_BC_CivilianScoutManager: CarControllerComponent found=%1.", (carController != null).ToString()), LogLevel.NORMAL);
		if (carController)
		{
			carController.StartEngine();
			carController.SetPersistentHandBrake(false);
			Print("GRAD_BC_CivilianScoutManager: Engine start requested, handbrake released.", LogLevel.NORMAL);
		}

		m_bScoutActive = true;
		m_bScoutReported = false;

		// 7. Assign waypoint after a short delay so the driver is settled in the seat,
		//    then refresh it every WAYPOINT_UPDATE_MS in case the truck moves.
		GetGame().GetCallqueue().CallLater(AssignScoutWaypoint, 2000, false);
		GetGame().GetCallqueue().CallLater(LogDriverState, 3000, false);
		GetGame().GetCallqueue().CallLater(AssignScoutWaypoint, WAYPOINT_UPDATE_MS, true);

		// Begin LoS checks.
		GetGame().GetCallqueue().CallLater(ScoutLoSCheck, LOS_CHECK_MS, true);
	}

	// Logs driver seating and AI state 3 seconds after spawn — useful for spotting seat failures.
	protected void LogDriverState()
	{
		if (!m_ScoutDriver || !m_ScoutVehicle)
		{
			Print("GRAD_BC_CivilianScoutManager: LogDriverState — driver or vehicle gone.", LogLevel.WARNING);
			return;
		}

		Print(string.Format("GRAD_BC_CivilianScoutManager: [3s post-spawn] driver pos=%1 vehicle pos=%2.", m_ScoutDriver.GetOrigin().ToString(), m_ScoutVehicle.GetOrigin().ToString()), LogLevel.NORMAL);

		CompartmentAccessComponent compartmentAccess = CompartmentAccessComponent.Cast(m_ScoutDriver.FindComponent(CompartmentAccessComponent));
		if (compartmentAccess)
		{
			BaseCompartmentSlot occupiedSlot = compartmentAccess.GetCompartment();
			if (occupiedSlot)
				Print(string.Format("GRAD_BC_CivilianScoutManager: [3s post-spawn] driver is in compartment type=%1.", occupiedSlot.GetType().ToString()), LogLevel.NORMAL);
			else
				Print("GRAD_BC_CivilianScoutManager: [3s post-spawn] driver is NOT seated in any compartment.", LogLevel.WARNING);
		}

		AIControlComponent aiControl = AIControlComponent.Cast(m_ScoutDriver.FindComponent(AIControlComponent));
		if (aiControl)
		{
			Print(string.Format("GRAD_BC_CivilianScoutManager: [3s post-spawn] AIActivated=%1 group=%2.", aiControl.IsAIActivated().ToString(), (m_ScoutGroup != null).ToString()), LogLevel.NORMAL);
		}

		if (m_ScoutGroup)
		{
			Print(string.Format("GRAD_BC_CivilianScoutManager: [3s post-spawn] group origin=%1 agent count=%2.", m_ScoutGroup.GetOrigin().ToString(), m_ScoutGroup.GetAgentsCount().ToString()), LogLevel.NORMAL);
		}
	}

	protected void AssignScoutWaypoint()
	{
		if (!m_ScoutGroup)
		{
			Print("GRAD_BC_CivilianScoutManager: AssignScoutWaypoint — group is null, skipping.", LogLevel.WARNING);
			return;
		}

		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
			return;

		IEntity radioTruck = bcm.GetRadioTruck();
		if (!radioTruck)
		{
			Print("GRAD_BC_CivilianScoutManager: AssignScoutWaypoint — radio truck is null, skipping.", LogLevel.WARNING);
			return;
		}

		vector truckPos = radioTruck.GetOrigin();
		vector groupOrigin = m_ScoutGroup.GetOrigin();

		Print(string.Format("GRAD_BC_CivilianScoutManager: AssignScoutWaypoint — group at %1, truck at %2, dist=%3m.", groupOrigin.ToString(), truckPos.ToString(), vector.Distance(groupOrigin, truckPos).ToString()), LogLevel.NORMAL);

		// Skip update if truck hasn't moved enough.
		if (m_CurrentWaypoint && vector.DistanceSq(truckPos, m_vLastWaypointTruckPos) < WAYPOINT_UPDATE_MIN_MOVE * WAYPOINT_UPDATE_MIN_MOVE)
		{
			Print("GRAD_BC_CivilianScoutManager: Truck hasn't moved enough, skipping waypoint update.", LogLevel.NORMAL);
			return;
		}

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		vector waypointPos = truckPos;
		if (aiWorld)
		{
			RoadNetworkManager roadMgr = aiWorld.GetRoadNetworkManager();
			if (roadMgr)
			{
				vector reachable;
				bool found = roadMgr.GetReachableWaypointInRoad(groupOrigin, truckPos, 500.0, reachable);
				Print(string.Format("GRAD_BC_CivilianScoutManager: GetReachableWaypointInRoad returned %1, reachable=%2.", found.ToString(), reachable.ToString()), LogLevel.NORMAL);
				if (found)
					waypointPos = reachable;
			}
			else
			{
				Print("GRAD_BC_CivilianScoutManager: AssignScoutWaypoint — RoadNetworkManager unavailable.", LogLevel.WARNING);
			}
		}
		else
		{
			Print("GRAD_BC_CivilianScoutManager: AssignScoutWaypoint — AIWorld unavailable.", LogLevel.WARNING);
		}

		// Remove old waypoint first so the nudge + destination become the queue.
		if (m_CurrentWaypoint)
		{
			m_ScoutGroup.RemoveWaypoint(m_CurrentWaypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentWaypoint);
			m_CurrentWaypoint = null;
		}

		// Prepend a short forward nudge to prevent the AI from reversing when the
		// destination is behind the vehicle's current heading.
		SpawnNudgeWaypoint();

		EntitySpawnParams wpParams = new EntitySpawnParams();
		wpParams.Transform[3] = waypointPos;

		AIWaypoint newWaypoint = AIWaypoint.Cast(
			GetGame().SpawnEntityPrefab(Resource.Load(m_sScoutWaypointPrefab), GetGame().GetWorld(), wpParams)
		);

		if (newWaypoint)
		{
			newWaypoint.SetCompletionRadius(50.0);
			m_ScoutGroup.AddWaypoint(newWaypoint);

			m_CurrentWaypoint = newWaypoint;
			m_vLastWaypointTruckPos = truckPos;
			Print(string.Format("GRAD_BC_CivilianScoutManager: Waypoint updated to %1.", waypointPos.ToString()), LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_CivilianScoutManager: Failed to spawn AIWaypoint prefab; driver will not navigate.", LogLevel.WARNING);
		}
	}

	// -------------------------------------------------------------------------
	// Spawn position search
	// -------------------------------------------------------------------------

	// Tries eight compass directions at SCOUT_SPAWN_DISTANCE, snapping each to the
	// nearest road via GetClosestRoad (same approach as SCR_AmbientTrafficManager).
	// Expands the ring if no valid candidate is found.
	protected vector FindScoutSpawnPosition(vector truckPos)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
		{
			Print("GRAD_BC_CivilianScoutManager: No AIWorld available.", LogLevel.WARNING);
			return vector.Zero;
		}

		RoadNetworkManager roadMgr = aiWorld.GetRoadNetworkManager();
		if (!roadMgr)
		{
			Print("GRAD_BC_CivilianScoutManager: No RoadNetworkManager available.", LogLevel.WARNING);
			return vector.Zero;
		}

		float oceanLevel = GetGame().GetWorld().GetOceanBaseHeight();

		float distance = SCOUT_SPAWN_DISTANCE;
		while (distance <= SCOUT_SPAWN_DISTANCE_MAX)
		{
			Print(string.Format("GRAD_BC_CivilianScoutManager: Searching at ring distance=%1m.", distance.ToString()), LogLevel.NORMAL);

			for (int i = 0; i < 8; i++)
			{
				float angle = i * 45.0 * Math.DEG2RAD;
				float x = truckPos[0] + distance * Math.Sin(angle);
				float z = truckPos[2] + distance * Math.Cos(angle);
				vector candidate = Vector(x, 0, z);

				BaseRoad road;
				float roadDist;
				if (roadMgr.GetClosestRoad(candidate, road, roadDist) == -1 || !road)
				{
					Print(string.Format("GRAD_BC_CivilianScoutManager:   dir%1 no road found.", i.ToString()), LogLevel.NORMAL);
					continue;
				}

				array<vector> points = {};
				road.GetPoints(points);
				if (points.IsEmpty())
					continue;

				vector roadPoint = points[0];
				float groundY = GetGame().GetWorld().GetSurfaceY(roadPoint[0], roadPoint[2]);
				roadPoint[1] = groundY + SPAWN_GROUND_OFFSET;

				if (groundY <= oceanLevel)
				{
					Print(string.Format("GRAD_BC_CivilianScoutManager:   dir%1 REJECTED underwater.", i.ToString()), LogLevel.NORMAL);
					continue;
				}

				if (IsNearAnyPlayer(roadPoint))
				{
					Print(string.Format("GRAD_BC_CivilianScoutManager:   dir%1 REJECTED player nearby road=%2.", i.ToString(), roadPoint.ToString()), LogLevel.NORMAL);
					continue;
				}

				if (IsVisibleToAnyPlayer(roadPoint))
				{
					Print(string.Format("GRAD_BC_CivilianScoutManager:   dir%1 REJECTED visible to player road=%2.", i.ToString(), roadPoint.ToString()), LogLevel.NORMAL);
					continue;
				}

				Print(string.Format("GRAD_BC_CivilianScoutManager:   dir%1 OK road=%2 roadDist=%3m.", i.ToString(), roadPoint.ToString(), roadDist.ToString()), LogLevel.NORMAL);
				return roadPoint;
			}

			Print(string.Format("GRAD_BC_CivilianScoutManager: No valid spawn at %1m, expanding.", distance.ToString()), LogLevel.NORMAL);
			distance += SCOUT_SPAWN_DISTANCE_STEP;
		}

		Print("GRAD_BC_CivilianScoutManager: Could not find a valid road spawn position.", LogLevel.WARNING);
		return vector.Zero;
	}

	// Returns true if any living player is within SCOUT_SPAWN_DISTANCE / 4 of pos.
	protected bool IsNearAnyPlayer(vector pos)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;

		float minDist = SCOUT_SPAWN_DISTANCE * 0.25;
		float minDistSq = minDist * minDist;

		array<int> playerIds = {};
		pm.GetAllPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = pm.GetPlayerControlledEntity(playerId);
			if (!playerEntity)
				continue;

			if (vector.DistanceSq(pos, playerEntity.GetOrigin()) < minDistSq)
				return true;
		}

		return false;
	}

	// Returns true if any living player has an unobstructed sightline to pos.
	protected bool IsVisibleToAnyPlayer(vector pos)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;

		array<int> playerIds = {};
		pm.GetAllPlayers(playerIds);

		vector checkPos = pos + Vector(0, LOS_HEIGHT_OFFSET, 0);

		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = pm.GetPlayerControlledEntity(playerId);
			if (!playerEntity)
				continue;

			vector eyePos = playerEntity.GetOrigin() + Vector(0, PLAYER_EYE_HEIGHT, 0);
			if (HasLineOfSight(eyePos, checkPos))
				return true;
		}

		return false;
	}


	// -------------------------------------------------------------------------
	// LoS check (server-side, called every LOS_CHECK_MS)
	// -------------------------------------------------------------------------

	protected void ScoutLoSCheck()
	{
		if (!m_bScoutActive || m_bScoutReported || !m_ScoutDriver)
		{
			GetGame().GetCallqueue().Remove(ScoutLoSCheck);
			return;
		}

		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
			return;

		IEntity radioTruck = bcm.GetRadioTruck();
		if (!radioTruck)
		{
			GetGame().GetCallqueue().Remove(ScoutLoSCheck);
			return;
		}

		vector truckPos = radioTruck.GetOrigin();

		if (!m_ScoutDriver)
		{
			GetGame().GetCallqueue().Remove(ScoutLoSCheck);
			return;
		}

		float distSq = vector.DistanceSq(m_ScoutDriver.GetOrigin(), truckPos);

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CivilianScoutManager: Scout dist to truck=%1m (trigger at %2m).", Math.Sqrt(distSq).ToString(), LOS_TRIGGER_DISTANCE.ToString()), LogLevel.NORMAL);

		if (distSq <= LOS_TRIGGER_DISTANCE * LOS_TRIGGER_DISTANCE)
		{
			Print("GRAD_BC_CivilianScoutManager: Scout close enough to truck – broadcasting report.", LogLevel.NORMAL);
			BroadcastScoutReport(truckPos);
		}
	}

	// -------------------------------------------------------------------------
	// Reporting
	// -------------------------------------------------------------------------

	protected void BroadcastScoutReport(vector truckPos)
	{
		m_bScoutReported = true;
		m_bScoutActive = false;

		// Stop LoS check loop and waypoint updates.
		GetGame().GetCallqueue().Remove(ScoutLoSCheck);
		GetGame().GetCallqueue().Remove(AssignScoutWaypoint);

		// Reset timer so another scout can trigger after the full delay has elapsed again.
		m_fNoTransmissionSeconds = 0.0;
		m_bTriggered = false;

		// Send to all clients.
		if (m_RplComponent)
			Rpc(RpcDo_Broadcast_ShowScoutReport, truckPos);

		// Also handle on listen-server if there is a local player controller.
		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
			ShowScoutReportUI(truckPos);

		// Route scout back to spawn position, then despawn when safe.
		AssignReturnWaypoint();
		GetGame().GetCallqueue().CallLater(DespawnWhenSafe, DESPAWN_CHECK_MS, true);
	}

	// Assigns a waypoint at the original spawn position so the scout drives home.
	protected void AssignReturnWaypoint()
	{
		if (!m_ScoutGroup || m_vSpawnPos == vector.Zero)
			return;

		if (m_CurrentWaypoint)
		{
			m_ScoutGroup.RemoveWaypoint(m_CurrentWaypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentWaypoint);
			m_CurrentWaypoint = null;
		}

		SpawnNudgeWaypoint();

		EntitySpawnParams wpParams = new EntitySpawnParams();
		wpParams.Transform[3] = m_vSpawnPos;

		AIWaypoint returnWaypoint = AIWaypoint.Cast(
			GetGame().SpawnEntityPrefab(Resource.Load(m_sScoutWaypointPrefab), GetGame().GetWorld(), wpParams)
		);

		if (!returnWaypoint)
		{
			Print("GRAD_BC_CivilianScoutManager: Failed to spawn return waypoint; despawning immediately.", LogLevel.WARNING);
			DespawnScout();
			return;
		}

		returnWaypoint.SetCompletionRadius(DESPAWN_HOME_RADIUS);
		m_ScoutGroup.AddWaypoint(returnWaypoint);

		m_CurrentWaypoint = returnWaypoint;
		Print(string.Format("GRAD_BC_CivilianScoutManager: Return waypoint set to spawn pos %1.", m_vSpawnPos.ToString()), LogLevel.NORMAL);
	}

	// Polls every DESPAWN_CHECK_MS: despawn once the scout is near spawn AND no players are close.
	// If players are near the spawn position, route to the nearest safe alternative road point.
	protected void DespawnWhenSafe()
	{
		if (!m_ScoutVehicle)
		{
			GetGame().GetCallqueue().Remove(DespawnWhenSafe);
			DespawnScout();
			return;
		}

		vector scoutPos = m_ScoutVehicle.GetOrigin();
		float distToSpawn = vector.Distance(scoutPos, m_vSpawnPos);

		if (distToSpawn > DESPAWN_HOME_RADIUS)
			return;

		// Scout is home — check if any player is nearby.
		if (!IsNearAnyPlayer(scoutPos))
		{
			GetGame().GetCallqueue().Remove(DespawnWhenSafe);
			Print("GRAD_BC_CivilianScoutManager: Scout reached spawn, no players nearby — despawning.", LogLevel.NORMAL);
			DespawnScout();
			return;
		}

		// Players too close — find an alternative safe road point and route there.
		Print("GRAD_BC_CivilianScoutManager: Players near spawn, rerouting scout to safe despawn point.", LogLevel.NORMAL);

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		RoadNetworkManager roadMgr = aiWorld.GetRoadNetworkManager();
		if (!roadMgr)
			return;

		float oceanLevel = GetGame().GetWorld().GetOceanBaseHeight();

		for (int i = 0; i < 8; i++)
		{
			float angle = i * 45.0 * Math.DEG2RAD;
			float x = scoutPos[0] + SCOUT_SPAWN_DISTANCE * 0.5 * Math.Sin(angle);
			float z = scoutPos[2] + SCOUT_SPAWN_DISTANCE * 0.5 * Math.Cos(angle);
			vector candidate = Vector(x, 0, z);

			BaseRoad road;
			float roadDist;
			if (roadMgr.GetClosestRoad(candidate, road, roadDist) == -1 || !road)
				continue;

			array<vector> points = {};
			road.GetPoints(points);
			if (points.IsEmpty())
				continue;

			vector roadPoint = points[0];
			float groundY = GetGame().GetWorld().GetSurfaceY(roadPoint[0], roadPoint[2]);
			roadPoint[1] = groundY + SPAWN_GROUND_OFFSET;

			if (groundY <= oceanLevel)
				continue;

			if (IsNearAnyPlayer(roadPoint))
				continue;

			// Found a safe spot — reroute.
			m_vSpawnPos = roadPoint;

			EntitySpawnParams wpParams = new EntitySpawnParams();
			wpParams.Transform[3] = roadPoint;

			AIWaypoint newWaypoint = AIWaypoint.Cast(
				GetGame().SpawnEntityPrefab(Resource.Load(m_sScoutWaypointPrefab), GetGame().GetWorld(), wpParams)
			);

			if (newWaypoint && m_ScoutGroup)
			{
				if (m_CurrentWaypoint)
				{
					m_ScoutGroup.RemoveWaypoint(m_CurrentWaypoint);
					SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentWaypoint);
					m_CurrentWaypoint = null;
				}

				SpawnNudgeWaypoint();

				newWaypoint.SetCompletionRadius(DESPAWN_HOME_RADIUS);
				m_ScoutGroup.AddWaypoint(newWaypoint);

				m_CurrentWaypoint = newWaypoint;
				Print(string.Format("GRAD_BC_CivilianScoutManager: Rerouted to safe despawn point %1.", roadPoint.ToString()), LogLevel.NORMAL);
			}
			return;
		}

		// No safe alternative found — wait for players to move.
		Print("GRAD_BC_CivilianScoutManager: No safe alternative despawn point, waiting.", LogLevel.NORMAL);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Broadcast_ShowScoutReport(vector truckPos)
	{
		// Skip on the server – it already called ShowScoutReportUI directly in BroadcastScoutReport.
		if (Replication.IsServer())
			return;

		ShowScoutReportUI(truckPos);
	}

	// Runs on every client (and listen-server) to update the HUD and map.
	protected void ShowScoutReportUI(vector reportPos)
	{
		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CivilianScoutManager: ShowScoutReportUI at %1.", reportPos.ToString()), LogLevel.NORMAL);

		PlayerController localPC = GetGame().GetPlayerController();
		if (!localPC)
			return;

		// Show image hint via the traffic display.
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.Cast(localPC.FindComponent(SCR_HUDManagerComponent));
		if (hudManager)
		{
			GRAD_BC_Traffic trafficDisplay = GRAD_BC_Traffic.Cast(hudManager.FindInfoDisplay(GRAD_BC_Traffic));
			if (trafficDisplay)
			{
				GetGame().GetCallqueue().CallLater(
					trafficDisplay.showTrafficHint,
					0,
					false,
					e_currentTrafficDisplay.CIVILIAN_SIGHTING,
					reportPos
				);
			}
		}

		// Show text hint.
		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (hintManager)
			hintManager.ShowCustomHint("Strange Ural sighted near. Military activity nearby.", "Civilian Report", 15, false);

		// Place map icon.
		CreateScoutMapIcon(reportPos);
	}

	// -------------------------------------------------------------------------
	// Map icon helpers
	// -------------------------------------------------------------------------

	protected string FormatTimestamp()
	{
		int totalSeconds = System.GetTickCount() / 1000;
		int hours   = (totalSeconds / 3600) % 24;
		int minutes = (totalSeconds / 60) % 60;
		int secs    =  totalSeconds % 60;

		string hh = hours.ToString();
		string mm = minutes.ToString();
		string ss = secs.ToString();

		if (hours   < 10) hh = "0" + hh;
		if (minutes < 10) mm = "0" + mm;
		if (secs    < 10) ss = "0" + ss;

		return string.Format("%1:%2:%3", hh, mm, ss);
	}

	protected void CreateScoutMapIcon(vector position)
	{
		GRAD_PlayerComponent playerComp = GRAD_PlayerComponent.GetInstance();
		if (!playerComp)
		{
			Print("GRAD_BC_CivilianScoutManager: No GRAD_PlayerComponent found; cannot create map icon.", LogLevel.WARNING);
			return;
		}

		GRAD_IconMarkerUI iconMarkerUI = playerComp.GetIconMarkerUI();
		if (!iconMarkerUI)
		{
			Print("GRAD_BC_CivilianScoutManager: No GRAD_IconMarkerUI found; cannot create map icon.", LogLevel.WARNING);
			return;
		}

		if (m_sScoutIconTexture.IsEmpty())
		{
			Print("GRAD_BC_CivilianScoutManager: Scout icon texture not configured.", LogLevel.WARNING);
			return;
		}

		string timestamp = FormatTimestamp();
		string label = string.Format("CIV SIGHTING [%1]", timestamp);

		float iconX = position[0];
		float iconZ = position[2];
		int iconId = iconMarkerUI.AddIcon(iconX, iconZ, iconX, iconZ, m_sScoutIconTexture, -1, false, label);

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CivilianScoutManager: Created map icon id=%1 at %2.", iconId, position.ToString()), LogLevel.NORMAL);

		// Schedule removal.
		GetGame().GetCallqueue().CallLater(
			RemoveScoutMapIcon,
			MARKER_LIFETIME_S * 1000,
			false,
			iconId
		);
	}

	protected void RemoveScoutMapIcon(int iconId)
	{
		GRAD_PlayerComponent playerComp = GRAD_PlayerComponent.GetInstance();
		if (!playerComp)
			return;

		GRAD_IconMarkerUI iconMarkerUI = playerComp.GetIconMarkerUI();
		if (!iconMarkerUI)
			return;

		iconMarkerUI.RemoveIcon(iconId);

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CivilianScoutManager: Removed expired map icon id=%1.", iconId), LogLevel.NORMAL);
	}

	// -------------------------------------------------------------------------
	// Scout vehicle cleanup
	// -------------------------------------------------------------------------

	protected void DespawnScout()
	{
		GetGame().GetCallqueue().Remove(DespawnWhenSafe);

		if (m_ScoutDriver)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_ScoutDriver);
			m_ScoutDriver = null;
		}

		if (m_ScoutVehicle)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_ScoutVehicle);
			m_ScoutVehicle = null;
		}

		if (m_ScoutGroup)
		{
			// AIGroup inherits from AIAgent which is an IEntity — delete the group entity directly.
			SCR_EntityHelper.DeleteEntityAndChildren(AIAgent.Cast(m_ScoutGroup));
			m_ScoutGroup = null;
		}

		m_CurrentWaypoint = null;
		m_vLastWaypointTruckPos = vector.Zero;
		m_vSpawnPos = vector.Zero;
		m_bScoutActive = false;

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_CivilianScoutManager: Scout vehicle and driver despawned.", LogLevel.NORMAL);
	}

	// -------------------------------------------------------------------------
	// Utility
	// -------------------------------------------------------------------------

	// Distance (metres) in front of the vehicle for the anti-reverse nudge waypoint.
	static const float NUDGE_WAYPOINT_DISTANCE = 30.0;

	// Spawns a short waypoint directly in front of the vehicle to prevent the AI
	// from reversing when the real destination is behind its current heading.
	// The nudge waypoint is NOT tracked in m_CurrentWaypoint — it is consumed
	// automatically once the vehicle drives past it.
	protected void SpawnNudgeWaypoint()
	{
		if (!m_ScoutVehicle || !m_ScoutGroup)
		{
			Print("GRAD_BC_CivilianScoutManager: SpawnNudgeWaypoint — skipped (vehicle or group null).", LogLevel.WARNING);
			return;
		}

		vector mat[4];
		m_ScoutVehicle.GetTransform(mat);
		vector forward = mat[2]; // local Z = forward
		forward[1] = 0;
		forward.Normalize();

		vector nudgePos = m_ScoutVehicle.GetOrigin() + forward * NUDGE_WAYPOINT_DISTANCE;
		nudgePos[1] = GetGame().GetWorld().GetSurfaceY(nudgePos[0], nudgePos[2]) + SPAWN_GROUND_OFFSET;

		EntitySpawnParams nudgeParams = new EntitySpawnParams();
		nudgeParams.Transform[3] = nudgePos;

		AIWaypoint nudgeWaypoint = AIWaypoint.Cast(
			GetGame().SpawnEntityPrefab(Resource.Load(m_sScoutWaypointPrefab), GetGame().GetWorld(), nudgeParams)
		);

		if (nudgeWaypoint)
		{
			nudgeWaypoint.SetCompletionRadius(5.0);
			m_ScoutGroup.AddWaypoint(nudgeWaypoint);
			Print(string.Format("GRAD_BC_CivilianScoutManager: SpawnNudgeWaypoint — added nudge at %1.", nudgePos.ToString()), LogLevel.NORMAL);
		}
		else
		{
			Print("GRAD_BC_CivilianScoutManager: SpawnNudgeWaypoint — failed to spawn waypoint prefab.", LogLevel.WARNING);
		}
	}

	// Returns true when the line from `from` to `to` is not obstructed by terrain.
	protected bool HasLineOfSight(vector from, vector to)
	{
		TraceParam trace = new TraceParam();
		trace.Start = from;
		trace.End = to;
		trace.Flags = TraceFlags.WORLD;
		trace.LayerMask = ~0;

		float hitFraction = GetGame().GetWorld().TraceMove(trace, null);
		return (hitFraction >= 1.0);
	}
}
