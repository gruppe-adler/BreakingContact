// Replay data structures
class GRAD_BC_ReplayFrame : Managed
{
	float timestamp;
	ref array<ref GRAD_BC_PlayerSnapshot> players = {};
	ref array<ref GRAD_BC_ProjectileSnapshot> projectiles = {};
	ref array<ref GRAD_BC_TransmissionSnapshot> transmissions = {};
	ref array<ref GRAD_BC_VehicleSnapshot> vehicles = {};
	
	static GRAD_BC_ReplayFrame Create(float time)
	{
		GRAD_BC_ReplayFrame frame = new GRAD_BC_ReplayFrame();
		frame.timestamp = time; // Time should already be in seconds
		return frame;
	}
};

class GRAD_BC_PlayerSnapshot : Managed
{
	int playerId;
	string playerName;
	string factionKey;
	vector position;
	vector angles;
	bool isAlive;
	bool isInVehicle;
	RplId vehicleId;
	string vehicleType; // for vehicles
	string unitRole; // detected role based on equipment
	
	static GRAD_BC_PlayerSnapshot Create(int id, string name, string faction, vector pos, vector ang, bool alive, bool inVeh, string vehType, string role, RplId vehId)
	{
		GRAD_BC_PlayerSnapshot snapshot = new GRAD_BC_PlayerSnapshot();
		snapshot.playerId = id;
		snapshot.playerName = name;
		snapshot.factionKey = faction;
		snapshot.position = pos;
		snapshot.angles = ang;
		snapshot.isAlive = alive;
		snapshot.isInVehicle = inVeh;
		snapshot.vehicleId = vehId;
		snapshot.vehicleType = vehType;
		snapshot.unitRole = role;
		return snapshot;
	}
};

class GRAD_BC_VehicleSnapshot : Managed
{
    RplId entityId;
    string vehicleType;
    string factionKey;
    vector position;
    vector angles;
	bool m_bIsEmpty;
	bool m_bWasUsed;

    static GRAD_BC_VehicleSnapshot Create(RplId id, string type, string faction, vector pos, vector ang, bool isEmptyNow, bool wasUsed)
    {
        GRAD_BC_VehicleSnapshot snapshot = new GRAD_BC_VehicleSnapshot();
        snapshot.entityId = id;
        snapshot.vehicleType = type;
        snapshot.factionKey = faction;
        snapshot.position = pos;
        snapshot.angles = ang;
		snapshot.m_bIsEmpty = isEmptyNow;
		snapshot.m_bWasUsed = wasUsed;
        return snapshot;
    }
};

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
};

class GRAD_BC_TransmissionSnapshot : Managed
{
	vector position;
	ETransmissionState state;
	float progress; // 0.0 to 1.0
	
	static GRAD_BC_TransmissionSnapshot Create(vector pos, ETransmissionState transmissionState, float transmissionProgress)
	{
		GRAD_BC_TransmissionSnapshot snapshot = new GRAD_BC_TransmissionSnapshot();
		snapshot.position = pos;
		snapshot.state = transmissionState;
		snapshot.progress = transmissionProgress;
		return snapshot;
	}
};

//------------------------------------------------------------------------------------------------
// Temporary data structure for projectiles fired between recording frames
class GRAD_BC_ProjectileData : Managed
{
	vector position; // firing position
	vector velocity;
	string ammoType;
	float fireTime;
};

//------------------------------------------------------------------------------------------------
// Explosive event categories. Each renders differently during replay, so the category is
// recorded rather than re-derived from the prefab name on the client.
enum EGradBCExplosiveKind
{
	SMOKE,	// lingering coloured cloud
	HE,		// point detonation, brief flash
	AT		// launch -> impact, animated projectile
};

//------------------------------------------------------------------------------------------------
// A single explosive event (smoke deployment, HE detonation or AT shot).
//
// Unlike players/vehicles, these are NOT per-frame snapshots. A rocket's whole flight lasts
// ~1-3s while the recording interval is 3s, so sampling would miss it entirely. Instead each
// event is recorded ONCE with absolute timestamps, and the map layer interpolates against the
// playback clock. This is both cheaper on the wire and smooth regardless of recording interval.
class GRAD_BC_ExplosiveEvent : Managed
{
	EGradBCExplosiveKind kind;
	vector position;		// SMOKE: where the cloud sits. HE: detonation point. AT: launch point.
	vector endPosition;		// AT only: impact point. Unused for SMOKE/HE.
	float startTime;		// seconds, same clock as frame timestamps
	float endTime;			// SMOKE: when the cloud disperses. AT: impact time. HE: == startTime.
	int color;				// ARGB, used for smoke tinting; ignored for HE/AT
	string factionKey;		// faction of whoever caused it, may be empty

	static GRAD_BC_ExplosiveEvent Create(EGradBCExplosiveKind eventKind, vector pos, vector endPos, float start, float end, int argb, string faction)
	{
		GRAD_BC_ExplosiveEvent evt = new GRAD_BC_ExplosiveEvent();
		evt.kind = eventKind;
		evt.position = pos;
		evt.endPosition = endPos;
		evt.startTime = start;
		evt.endTime = end;
		evt.color = argb;
		evt.factionKey = faction;
		return evt;
	}
};

//------------------------------------------------------------------------------------------------
class GRAD_BC_ReplayData : Managed
{
	ref array<ref GRAD_BC_ReplayFrame> frames = {};

	// Explosive events live at the top level, not inside frames: they carry their own absolute
	// start/end times and are interpolated against the playback clock rather than being tied to
	// whichever frame happened to be sampled nearest them.
	ref array<ref GRAD_BC_ExplosiveEvent> explosiveEvents = {};

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
};
