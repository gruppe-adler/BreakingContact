// Hook into Vehicle class to detect when vehicles are spawned
modded class Vehicle
{
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		Print(string.Format("[GRAD BC Supply] Vehicle EOnInit: %1", owner), LogLevel.NORMAL);
		
		// Notify the listener after a small delay to ensure comboxes are registered
		GetGame().GetCallqueue().CallLater(NotifySpawnListener, 100, false, owner);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void NotifySpawnListener(IEntity owner)
	{
		GRAD_BC_VehicleSpawnListener.GetInstance().OnVehicleSpawned(owner, owner.GetOrigin());
	}
}

class GRAD_BC_VehicleSpawnListener
{
	static ref GRAD_BC_VehicleSpawnListener s_Instance;
	
	protected ref map<IEntity, GRAD_BC_VehicleSupplyComponent> m_mComboxes = new map<IEntity, GRAD_BC_VehicleSupplyComponent>();
	
	//------------------------------------------------------------------------------------------------
	static GRAD_BC_VehicleSpawnListener GetInstance()
	{
		if (!s_Instance)
			s_Instance = new GRAD_BC_VehicleSpawnListener();
		
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	void RegisterCombox(IEntity combox, GRAD_BC_VehicleSupplyComponent supplyComp)
	{
		Print(string.Format("[GRAD BC Supply Listener] Registered combox: %1", combox), LogLevel.NORMAL);
		m_mComboxes.Set(combox, supplyComp);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnVehicleSpawned(IEntity vehicle, vector position)
	{
		Print("========================================", LogLevel.NORMAL);
		Print("[GRAD BC Supply Listener] Vehicle spawned!", LogLevel.NORMAL);
		Print(string.Format("[GRAD BC Supply Listener] Vehicle: %1", vehicle), LogLevel.NORMAL);
		Print(string.Format("[GRAD BC Supply Listener] Position: %1", position), LogLevel.NORMAL);
		
		// Find the nearest combox
		GRAD_BC_VehicleSupplyComponent nearestSupply;
		float nearestDist = 999999;
		
		foreach (IEntity combox, GRAD_BC_VehicleSupplyComponent supply : m_mComboxes)
		{
			if (!combox || !supply)
				continue;
				
			vector comboxPos = combox.GetOrigin();
			float dist = vector.Distance(comboxPos, position);
			
			Print(string.Format("[GRAD BC Supply Listener] Combox distance: %1m", dist), LogLevel.NORMAL);
			
			if (dist < nearestDist && dist < 200) // Within 200m
			{
				nearestDist = dist;
				nearestSupply = supply;
			}
		}
		
		if (nearestSupply)
		{
			Print(string.Format("[GRAD BC Supply Listener] Found nearby combox (%1m away)", nearestDist), LogLevel.NORMAL);
			Print("[GRAD BC Supply Listener] Supplies already deducted by PerformAction, skipping duplicate deduction", LogLevel.NORMAL);
			Print(string.Format("[GRAD BC Supply Listener] Current balance: %1/%2", 
				nearestSupply.GetCurrentSupplies(), nearestSupply.GetMaxSupplies()), LogLevel.NORMAL);
		}
		else
		{
			Print("[GRAD BC Supply Listener] No nearby combox found", LogLevel.NORMAL);
		}
		
		Print("========================================", LogLevel.NORMAL);
	}
}
