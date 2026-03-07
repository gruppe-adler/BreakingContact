// Manager component that spawns a civilian scout vehicle after the radio truck has
// been non-transmitting for a configurable period. The civilian vehicle moves toward
// the radio truck in steps; once it has line-of-sight to the truck every player
// receives a hint and a map marker.
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

	[Attribute("{57A441224AC02CF3}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_Randomized.et", UIWidgets.ResourcePickerThumbnail, "Civilian vehicle prefab to spawn as the scout.", "et", "Breaking Contact - Civilian Scout")]
	protected ResourceName m_sScoutVehiclePrefab;

	[Attribute("{B8C70A7749D318C2}UI/Transmission/us_established.edds", UIWidgets.ResourcePickerThumbnail, "Map icon texture shown when the civilian reports the radio truck (placeholder until dedicated texture is provided).", "edds", "Breaking Contact - Civilian Scout")]
	protected ResourceName m_sScoutIconTexture;

	// -------------------------------------------------------------------------
	// Constants
	// -------------------------------------------------------------------------

	// How far from the radio truck the scout spawns (metres).
	static const float SCOUT_SPAWN_DISTANCE = 2000.0;

	// Distance the scout vehicle steps toward the truck each waypoint update (metres).
	static const float SCOUT_STEP_DISTANCE = 150.0;

	// Height offset (metres) used when evaluating line-of-sight from the scout or truck.
	static const float LOS_HEIGHT_OFFSET = 2.0;

	// Small height added above ground when placing the scout at a spawn or step position
	// to prevent the vehicle from clipping into terrain.
	static const float SPAWN_GROUND_OFFSET = 0.5;

	// Approximate eye height of a standing player character (metres) used for LoS checks.
	static const float PLAYER_EYE_HEIGHT = 1.7;

	// Interval (ms) at which the scout position is updated toward the truck.
	static const int WAYPOINT_UPDATE_MS = 10000;

	// Interval (ms) at which the LoS check is performed.
	static const int LOS_CHECK_MS = 5000;

	// Delay (ms) after reporting before the scout vehicle is despawned.
	static const int DESPAWN_DELAY_MS = 60000;

	// How long the map marker stays visible (seconds).
	static const float MARKER_LIFETIME_S = 180.0;

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
			GetGame().GetCallqueue().Remove(ScoutWaypointUpdate);
			GetGame().GetCallqueue().Remove(ScoutLoSCheck);
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

		Resource res = Resource.Load(m_sScoutVehiclePrefab);
		if (!res || !res.IsValid())
		{
			Print(string.Format("GRAD_BC_CivilianScoutManager: Invalid scout vehicle prefab: %1", m_sScoutVehiclePrefab), LogLevel.ERROR);
			return;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = spawnPos;

		m_ScoutVehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), spawnParams);
		if (!m_ScoutVehicle)
		{
			Print("GRAD_BC_CivilianScoutManager: Failed to spawn scout vehicle.", LogLevel.ERROR);
			return;
		}

		// Orient vehicle toward the truck.
		vector dir = truckPos - spawnPos;
		dir[1] = 0;
		dir.Normalize();
		m_ScoutVehicle.SetYawPitchRoll(dir.VectorToAngles());

		m_bScoutActive = true;
		m_bScoutReported = false;

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CivilianScoutManager: Scout vehicle spawned at %1.", spawnPos.ToString()), LogLevel.NORMAL);

		// Begin periodic movement and LoS checks.
		GetGame().GetCallqueue().CallLater(ScoutWaypointUpdate, WAYPOINT_UPDATE_MS, true);
		GetGame().GetCallqueue().CallLater(ScoutLoSCheck, LOS_CHECK_MS, true);
	}

	// -------------------------------------------------------------------------
	// Spawn position search
	// -------------------------------------------------------------------------

	// Tries eight compass directions at SCOUT_SPAWN_DISTANCE, preferring positions
	// not visible to any player. Falls back to the first direction if none qualify.
	protected vector FindScoutSpawnPosition(vector truckPos)
	{
		vector firstCandidate = vector.Zero;

		for (int i = 0; i < 8; i++)
		{
			float angle = i * 45.0 * Math.DEG2RAD;
			float x = truckPos[0] + SCOUT_SPAWN_DISTANCE * Math.Sin(angle);
			float z = truckPos[2] + SCOUT_SPAWN_DISTANCE * Math.Cos(angle);
			float y = GetGame().GetWorld().GetSurfaceY(x, z);

			vector candidate = Vector(x, y + SPAWN_GROUND_OFFSET, z);

			if (firstCandidate == vector.Zero)
				firstCandidate = candidate;

			if (!IsVisibleToAnyPlayer(candidate))
				return candidate;
		}

		// Return best-effort fallback (first position, even if potentially visible).
		return firstCandidate;
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
	// Scout movement (server-side, called every WAYPOINT_UPDATE_MS)
	// -------------------------------------------------------------------------

	protected void ScoutWaypointUpdate()
	{
		if (!m_bScoutActive || !m_ScoutVehicle)
		{
			GetGame().GetCallqueue().Remove(ScoutWaypointUpdate);
			return;
		}

		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
			return;

		IEntity radioTruck = bcm.GetRadioTruck();
		if (!radioTruck)
		{
			GetGame().GetCallqueue().Remove(ScoutWaypointUpdate);
			return;
		}

		vector truckPos = radioTruck.GetOrigin();
		vector scoutPos = m_ScoutVehicle.GetOrigin();

		float distance = vector.Distance(scoutPos, truckPos);

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CivilianScoutManager: Scout is %1 m from the truck.", distance), LogLevel.NORMAL);

		// Stop stepping when already close enough for LoS checks to work reliably.
		if (distance <= SCOUT_STEP_DISTANCE)
			return;

		// Move one step closer.
		vector dir = truckPos - scoutPos;
		dir[1] = 0;
		dir.Normalize();

		float stepDist = Math.Max(0.0, Math.Min(SCOUT_STEP_DISTANCE, distance - SCOUT_STEP_DISTANCE));
		if (stepDist <= 0)
			return;

		vector newPos = scoutPos + dir * stepDist;
		newPos[1] = GetGame().GetWorld().GetSurfaceY(newPos[0], newPos[2]) + SPAWN_GROUND_OFFSET;

		m_ScoutVehicle.SetOrigin(newPos);
		m_ScoutVehicle.SetYawPitchRoll(dir.VectorToAngles());

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CivilianScoutManager: Scout stepped to %1.", newPos.ToString()), LogLevel.NORMAL);
	}

	// -------------------------------------------------------------------------
	// LoS check (server-side, called every LOS_CHECK_MS)
	// -------------------------------------------------------------------------

	protected void ScoutLoSCheck()
	{
		if (!m_bScoutActive || m_bScoutReported || !m_ScoutVehicle)
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

		vector scoutEye = m_ScoutVehicle.GetOrigin() + Vector(0, LOS_HEIGHT_OFFSET, 0);
		vector truckCenter = radioTruck.GetOrigin() + Vector(0, LOS_HEIGHT_OFFSET, 0);

		if (HasLineOfSight(scoutEye, truckCenter))
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("GRAD_BC_CivilianScoutManager: LoS to truck established – broadcasting report.", LogLevel.NORMAL);

			BroadcastScoutReport(radioTruck.GetOrigin());
		}
	}

	// -------------------------------------------------------------------------
	// Reporting
	// -------------------------------------------------------------------------

	protected void BroadcastScoutReport(vector truckPos)
	{
		m_bScoutReported = true;
		m_bScoutActive = false;

		// Stop update loops.
		GetGame().GetCallqueue().Remove(ScoutWaypointUpdate);
		GetGame().GetCallqueue().Remove(ScoutLoSCheck);

		// Send to all clients.
		if (m_RplComponent)
			Rpc(RpcDo_Broadcast_ShowScoutReport, truckPos);

		// Also handle on listen-server if there is a local player controller.
		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
			ShowScoutReportUI(truckPos);

		// Schedule vehicle despawn.
		GetGame().GetCallqueue().CallLater(DespawnScout, DESPAWN_DELAY_MS, false);
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
		if (m_ScoutVehicle)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_ScoutVehicle);
			m_ScoutVehicle = null;
		}
		m_bScoutActive = false;

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("GRAD_BC_CivilianScoutManager: Scout vehicle despawned.", LogLevel.NORMAL);
	}

	// -------------------------------------------------------------------------
	// Utility
	// -------------------------------------------------------------------------

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
