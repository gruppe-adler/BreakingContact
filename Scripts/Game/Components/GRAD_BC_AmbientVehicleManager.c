[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Spawns ambient vehicles near buildings along roads.")]
class GRAD_BC_AmbientVehicleManagerClass : ScriptComponentClass
{
}

class GRAD_BC_AmbientVehicleManager : ScriptComponent
{
	// ------------------------------------------------------------------------------------------------
	// CONSTANTS (Adjustable)
	// ------------------------------------------------------------------------------------------------
	const int TARGET_VEHICLE_COUNT = 50; 		// Total vehicles to spawn
	const int MAX_SPAWN_ATTEMPTS = 500;  		// Safety break for the loop
	const float BUILDING_SEARCH_RADIUS = 40.0; 	// How close a building must be to the road
	const float ROAD_SEARCH_RADIUS = 100.0; 	// Radius to find road from random point
	const float MIN_FUEL = 0.1; 				// 10%
	const float MAX_FUEL = 0.6; 				// 60%
	const float CLEARANCE_CHECK_RADIUS = 2.5; 	// Collision check size
	
	// ------------------------------------------------------------------------------------------------
	// ATTRIBUTES
	// ------------------------------------------------------------------------------------------------
	[Attribute(desc: "List of vehicle prefabs to spawn", params: "et")]
	protected ref array<ResourceName> m_aVehiclePrefabs = {
		"{49C909AFD66E90A1}Prefabs/Vehicles/Wheeled/S1203/S1203_transport_randomized.et",
		"{128253A267BE9424}Prefabs/Vehicles/Wheeled/S105/S105_randomized.et",
		"{57A441224AC02CF3}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_Randomized.et"
	};
	
	// ------------------------------------------------------------------------------------------------
	// MEMBERS
	// ------------------------------------------------------------------------------------------------
	protected ref array<ResourceName> m_aResolvedVehiclePrefabs;
	protected int m_iSpawnedCount = 0;
	protected int m_iTotalAttempts = 0;
	protected bool m_bBuildingFound = false; // Helper for query callback
	protected bool m_bCollisionFound = false; // Helper for collision callback
	
	// ------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		// Only run on server
		if (Replication.IsServer())
		{
			// Delay to ensure world and road network are fully loaded
			GetGame().GetCallqueue().CallLater(SpawnAmbientVehicles, 5000, false);
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	// VEHICLE RESOLUTION (priority: mission header override > faction catalog > hardcoded defaults)
	// ------------------------------------------------------------------------------------------------
	protected void ResolveVehiclePrefabs()
	{
		m_aResolvedVehiclePrefabs = new array<ResourceName>();

		// Priority 1: Mission header override
		if (TryGetMissionHeaderOverrides())
		{
			Print(string.Format("GRAD_BC_AmbientVehicleManager: Using %1 vehicle prefabs from mission header override.", m_aResolvedVehiclePrefabs.Count()), LogLevel.NORMAL);
			return;
		}

		// Priority 2: Auto-detect from civilian faction catalog
		if (TryGetFactionCatalogVehicles())
		{
			Print(string.Format("GRAD_BC_AmbientVehicleManager: Using %1 vehicle prefabs from civilian faction catalog.", m_aResolvedVehiclePrefabs.Count()), LogLevel.NORMAL);
			return;
		}

		// Priority 3: Hardcoded defaults from attribute
		if (m_aVehiclePrefabs && !m_aVehiclePrefabs.IsEmpty())
		{
			foreach (ResourceName rn : m_aVehiclePrefabs)
			{
				m_aResolvedVehiclePrefabs.Insert(rn);
			}
			Print(string.Format("GRAD_BC_AmbientVehicleManager: Using %1 hardcoded default vehicle prefabs.", m_aResolvedVehiclePrefabs.Count()), LogLevel.NORMAL);
			return;
		}

		Print("GRAD_BC_AmbientVehicleManager: No vehicle prefabs available from any source!", LogLevel.ERROR);
	}

	// ------------------------------------------------------------------------------------------------
	protected bool TryGetMissionHeaderOverrides()
	{
		MissionHeader header = GetGame().GetMissionHeader();
		if (!header)
			return false;

		GRAD_BC_MissionHeader bcHeader = GRAD_BC_MissionHeader.Cast(header);
		if (!bcHeader)
			return false;

		if (!bcHeader.HasTrafficVehicleOverrides())
			return false;

		bcHeader.GetTrafficVehicleOverrides(m_aResolvedVehiclePrefabs);
		return !m_aResolvedVehiclePrefabs.IsEmpty();
	}

	// ------------------------------------------------------------------------------------------------
	protected bool TryGetFactionCatalogVehicles()
	{
		// Determine civilian faction key
		string civFactionKey = "CIV";

		MissionHeader header = GetGame().GetMissionHeader();
		if (header)
		{
			GRAD_BC_MissionHeader bcHeader = GRAD_BC_MissionHeader.Cast(header);
			if (bcHeader)
				civFactionKey = bcHeader.GetCivilianFactionKey();
		}

		// Get faction manager
		FactionManager factionMgr = GetGame().GetFactionManager();
		if (!factionMgr)
		{
			Print("GRAD_BC_AmbientVehicleManager: No FactionManager found.", LogLevel.WARNING);
			return false;
		}

		// Get civilian faction
		SCR_Faction civFaction = SCR_Faction.Cast(factionMgr.GetFactionByKey(civFactionKey));
		if (!civFaction)
		{
			Print(string.Format("GRAD_BC_AmbientVehicleManager: Faction '%1' not found.", civFactionKey), LogLevel.WARNING);
			return false;
		}

		// Get vehicle entity catalog
		SCR_EntityCatalog catalog = civFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE);
		if (!catalog)
		{
			Print(string.Format("GRAD_BC_AmbientVehicleManager: No vehicle catalog found for faction '%1'.", civFactionKey), LogLevel.WARNING);
			return false;
		}

		// Extract enabled entries
		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);

		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (!entry.IsEnabled())
				continue;

			ResourceName prefab = entry.GetPrefab();
			if (prefab.IsEmpty())
				continue;

			// Skip air assets - only spawn land vehicles
			string prefabStr = prefab;
			prefabStr.ToLower();
			if (prefabStr.Contains("helicopter") || prefabStr.Contains("plane") || prefabStr.Contains("aircraft") || prefabStr.Contains("/air/"))
			{
				Print(string.Format("GRAD_BC_AmbientVehicleManager: Skipping air asset from catalog: %1", prefab), LogLevel.NORMAL);
				continue;
			}

			Resource res = Resource.Load(prefab);
			if (!res || !res.IsValid())
			{
				Print(string.Format("GRAD_BC_AmbientVehicleManager: Skipping invalid prefab from catalog: %1", prefab), LogLevel.WARNING);
				continue;
			}

			m_aResolvedVehiclePrefabs.Insert(prefab);
		}

		return !m_aResolvedVehiclePrefabs.IsEmpty();
	}

	// ------------------------------------------------------------------------------------------------
	protected void SpawnAmbientVehicles()
	{
		// Resolve vehicle prefabs on first call
		if (!m_aResolvedVehiclePrefabs)
			ResolveVehiclePrefabs();

		if (!m_aResolvedVehiclePrefabs || m_aResolvedVehiclePrefabs.IsEmpty())
		{
			Print("GRAD_BC_AmbientVehicleManager: No vehicle prefabs available!", LogLevel.WARNING);
			return;
		}
		
		// Get map bounds to pick random positions
		vector mapMin, mapMax;
		GetGame().GetWorldEntity().GetWorldBounds(mapMin, mapMax);
		
		// Throttle settings
		const int BATCH_SIZE = 5;
		int currentBatchAttempts = 0;
		
		if (m_iTotalAttempts == 0)
			Print(string.Format("GRAD_BC_AmbientVehicleManager: Starting spawn loop. Target: %1", TARGET_VEHICLE_COUNT), LogLevel.NORMAL);
		
		while (m_iSpawnedCount < TARGET_VEHICLE_COUNT && m_iTotalAttempts < MAX_SPAWN_ATTEMPTS && currentBatchAttempts < BATCH_SIZE)
		{
			m_iTotalAttempts++;
			currentBatchAttempts++;
			
			// 1. Pick a random position on the map
			vector randomPos = GenerateRandomPosition(mapMin, mapMax);
			
			// 2. Find nearest road to that position
			BaseRoad road = GetNearestRoad(randomPos, ROAD_SEARCH_RADIUS);
			if (!road) 
				continue;
			
			// 3. Get specific point and direction on the road
			vector roadPos, roadDir;
			if (!GetRoadPointAndDir(road, randomPos, roadPos, roadDir))
				continue;
			
			// 4. Check if there is a building nearby
			// We want vehicles parked near houses, not in the middle of nowhere
			if (!IsBuildingNearby(roadPos, BUILDING_SEARCH_RADIUS))
				continue;
				
			// 5. Calculate spawn position at the side of the road
			float roadWidth = road.GetWidth();
			// Offset: Half road width + vehicle half width approx (1.5m) + margin
			float offsetDist = (roadWidth * 0.5) + 2.0; 
			
			// Calculate right vector from road direction (assuming Y is up)
			vector up = "0 1 0";
			vector sideVector = roadDir * up; // Cross product
			sideVector.Normalize();
			
			// Randomly pick left or right side
			if (Math.RandomFloat01() > 0.5)
				sideVector = -sideVector;
				
			vector spawnPos = roadPos + (sideVector * offsetDist);
			
			// Snap to terrain height
			float groundY = GetGame().GetWorld().GetSurfaceY(spawnPos[0], spawnPos[2]);
			spawnPos[1] = groundY;
			
			// 6. Check for collisions (trees, rocks, other vehicles)
			if (CheckCollision(spawnPos))
				continue;
				
			// 7. Spawn the vehicle
			SpawnVehicle(spawnPos, roadDir);
		}
		
		if (m_iSpawnedCount < TARGET_VEHICLE_COUNT && m_iTotalAttempts < MAX_SPAWN_ATTEMPTS)
		{
			// Continue later to prevent freeze
			GetGame().GetCallqueue().CallLater(SpawnAmbientVehicles, 100, false);
		}
		else
		{
			Print(string.Format("GRAD_BC_AmbientVehicleManager: Finished. Spawned %1 vehicles in %2 attempts.", m_iSpawnedCount, m_iTotalAttempts), LogLevel.NORMAL);
		}
	}
	
	// ------------------------------------------------------------------------------------------------
	protected void SpawnVehicle(vector pos, vector roadDir)
	{
		// Pick random prefab from resolved list
		ResourceName prefab = m_aResolvedVehiclePrefabs.GetRandomElement();
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		
		Resource res = Resource.Load(prefab);
		if (!res.IsValid()) return;
		
		IEntity vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!vehicle) return;

		// --- BC MOD: Register ambient vehicle with replay manager ---
		Vehicle v = Vehicle.Cast(vehicle);
		if (v)
		{
			GRAD_BC_ReplayManager replayMgr = GRAD_BC_ReplayManager.GetInstance();
			if (replayMgr)
			{
				replayMgr.RegisterTrackedVehicle(v);
				Print("BC Debug - Registered ambient vehicle with replay manager", LogLevel.NORMAL);
			}
		}
		
		// Align with road (0 or 180 degrees)
		vector angles = roadDir.VectorToAngles();
		if (Math.RandomFloat01() > 0.5)
		{
			// Flip 180 degrees
			angles[0] = angles[0] + 180;
		}
		vehicle.SetYawPitchRoll(angles);
		
		// Randomize Fuel
		SCR_FuelManagerComponent fuelComp = SCR_FuelManagerComponent.Cast(vehicle.FindComponent(SCR_FuelManagerComponent));
		if (fuelComp)
		{
			array<BaseFuelNode> fuelNodes = {};
			fuelComp.GetFuelNodesList(fuelNodes);
			float randomFuelPct = Math.RandomFloatInclusive(MIN_FUEL, MAX_FUEL);
			
			foreach (BaseFuelNode node : fuelNodes)
			{
				node.SetFuel(node.GetMaxFuel() * randomFuelPct);
			}
		}
		
		// Physics settlement
		Physics phys = vehicle.GetPhysics();
		if (phys)
		{
			phys.SetVelocity("0 -1 0"); // Push down slightly
			phys.SetAngularVelocity("0 0 0");
		}
		
		m_iSpawnedCount++;
	}
	
	// ------------------------------------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------------------------------------
	
	protected vector GenerateRandomPosition(vector min, vector max)
	{
		return Vector(
			Math.RandomFloatInclusive(min[0], max[0]),
			0,
			Math.RandomFloatInclusive(min[2], max[2])
		);
	}
	
	protected BaseRoad GetNearestRoad(vector center, float radius)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld) return null;
		
		RoadNetworkManager roadMgr = aiWorld.GetRoadNetworkManager();
		if (!roadMgr) return null;
		
		vector min = center - Vector(radius, radius, radius);
		vector max = center + Vector(radius, radius, radius);
		
		array<BaseRoad> roads = {};
		roadMgr.GetRoadsInAABB(min, max, roads);
		
		if (roads.IsEmpty()) return null;
		
		// Return the first one found (simplification, but sufficient for random sampling)
		return roads[0];
	}
	
	protected bool GetRoadPointAndDir(BaseRoad road, vector targetPos, out vector outPos, out vector outDir)
	{
		array<vector> points = {};
		road.GetPoints(points);
		
		if (points.Count() < 2) return false;
		
		// Find closest segment point
		// Note: A more complex projection could be done, but finding the closest node is usually enough for ambient placement
		int closestIdx = 0;
		float closestDist = float.MAX;
		
		for (int i = 0; i < points.Count(); i++)
		{
			float d = vector.DistanceSq(targetPos, points[i]);
			if (d < closestDist)
			{
				closestDist = d;
				closestIdx = i;
			}
		}
		
		outPos = points[closestIdx];
		
		// Calculate direction
		// If at end, look back; otherwise look forward
		if (closestIdx < points.Count() - 1)
			outDir = vector.Direction(points[closestIdx], points[closestIdx + 1]);
		else
			outDir = vector.Direction(points[closestIdx - 1], points[closestIdx]);
			
		outDir.Normalize();
		return true;
	}
	
	protected bool IsBuildingNearby(vector pos, float radius)
	{
		m_bBuildingFound = false;
		GetGame().GetWorld().QueryEntitiesBySphere(pos, radius, FindBuildingCallback, null, EQueryEntitiesFlags.STATIC);
		return m_bBuildingFound;
	}
	
	protected bool FindBuildingCallback(IEntity e)
	{
		if (m_bBuildingFound) return false; // Stop if already found
		
		// Check for MapDescriptor with BUILDING type
		SCR_MapDescriptorComponent mapDesc = SCR_MapDescriptorComponent.Cast(e.FindComponent(SCR_MapDescriptorComponent));
		if (mapDesc)
		{
			static const array<EMapDescriptorType> allowedTypes = {
				EMapDescriptorType.MDT_BUILDING,
				EMapDescriptorType.MDT_HOUSE,
				EMapDescriptorType.MDT_FUELSTATION,
				EMapDescriptorType.MDT_BUSSTOP,
				EMapDescriptorType.MDT_BUSSTATION,
				EMapDescriptorType.MDT_HOTEL,
				EMapDescriptorType.MDT_PARKING,
				EMapDescriptorType.MDT_PORT,
				EMapDescriptorType.MDT_AIRPORT
			};
			EMapDescriptorType type = mapDesc.GetBaseType();
			foreach (EMapDescriptorType allowed : allowedTypes)
			{
				if (type == allowed)
				{
					m_bBuildingFound = true;
					return false; // Stop query
				}
			}
		}
		{
			m_bBuildingFound = true;
			return false; // Stop query
		}
		
		return true; // Continue query
	}
	
	protected bool CheckCollision(vector pos)
	{
		// Simple sphere trace to check if space is empty
		// We check slightly above ground
		vector checkPos = pos + "0 1.0 0";
		
		// Using TraceMove or just QueryEntities
		// Let's use QueryEntities to check for obstacles
		m_bCollisionFound = false;
		GetGame().GetWorld().QueryEntitiesBySphere(checkPos, CLEARANCE_CHECK_RADIUS, 
			CollisionCheckCallback, null, EQueryEntitiesFlags.ALL);
			
		return m_bCollisionFound;
	}
	
	protected bool CollisionCheckCallback(IEntity e)
	{
		if (e.GetPhysics()) {
			m_bCollisionFound = true;
			return false; // Stop query
		}
		return true; // Continue query
	}
}