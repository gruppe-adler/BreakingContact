[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "manages the radio truck itself")]
class GRAD_BC_RadioTruckComponentClass : ScriptComponentClass
{
}

class GRAD_BC_RadioTruckComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "Update Interval", params: "", category: "Breaking Contact - Radio Truck")];
	protected int m_iRadioTransmissionUpdateInterval;

	static float m_iMaxTransmissionDistance = 500.0;

	[RplProp()]
	protected bool m_bIsTransmitting;

	private Vehicle m_radioTruck;

	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private VehicleWheeledSimulation_SA_B m_VehicleWheeledSimulationComponent;

	private RplComponent m_RplComponent;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		//Print("BC Debug - OnPostInit()", LogLevel.NORMAL);

		m_radioTruck = Vehicle.Cast(GetOwner());

		m_mapDescriptorComponent = SCR_MapDescriptorComponent.Cast(m_radioTruck.FindComponent(SCR_MapDescriptorComponent));
		m_VehicleWheeledSimulationComponent = VehicleWheeledSimulation_SA_B.Cast(m_radioTruck.FindComponent(VehicleWheeledSimulation_SA_B));

		m_RplComponent = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));

		//PrintFormat("BC Debug - IsMaster(): %1", m_RplComponent.IsMaster()); // IsMaster() does not mean Authority
		//PrintFormat("BC Debug - IsProxy(): %1", m_RplComponent.IsProxy());
		//PrintFormat("BC Debug - IsOwner(): %1", m_RplComponent.IsOwner());

		if (m_RplComponent.IsMaster())
			GetGame().GetCallqueue().CallLater(mainLoop, 1000, true);
	}


	//------------------------------------------------------------------------------------------------
	void mainLoop()
	{
		if (GetTransmissionActive()) { applyBrakes(); }
	}
	
	void applyBrakes() {
		RplComponent rplComp = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));
		// currently log is on server always, even when players steer the truck :/
		if (!rplComp.IsProxy() && rplComp.IsOwner()) {
			Print(string.Format("Breaking Contact RTC - i am server, exiting brake lock"), LogLevel.NORMAL);
			return;
		}
			
		CarControllerComponent carController = CarControllerComponent.Cast(m_radioTruck.FindComponent(CarControllerComponent));
		// apparently this does not work?		
		if (carController && !carController.GetPersistentHandBrake()) {
			carController.SetPersistentHandBrake(true);
			Print(string.Format("Breaking Contact RTC - setting handbrake"), LogLevel.NORMAL);
		}
		
		VehicleWheeledSimulation simulation = carController.GetSimulation();
		if (simulation && !simulation.GetBrake()) {
			simulation.SetBreak(true, true);	
			Print(string.Format("Breaking Contact RTC - setting brake"), LogLevel.NORMAL);
		}
	}
	
	bool GetTransmissionActive() 
	{
		return m_bIsTransmitting;
	}

	void SetTransmissionActive(bool setTo) {
		m_bIsTransmitting = setTo;
		Replication.BumpMe();
		
		Print(string.Format("Breaking Contact RTC -  Setting m_bIsTransmitting to %1", m_bIsTransmitting), LogLevel.NORMAL);
		
		SCR_VehicleDamageManagerComponent VDMC = SCR_VehicleDamageManagerComponent.Cast(m_radioTruck.FindComponent(SCR_VehicleDamageManagerComponent));

		// disable transmissions for every transmission point
		if (!m_bIsTransmitting) {
			if (VDMC) {
				// VDMC.SetEngineFunctional(true);
				Print(string.Format("Breaking Contact RTC -  Enabling Engine due to transmission ended"), LogLevel.NORMAL);
			}
		} else {
			if (VDMC) {
				// VDMC.SetEngineFunctional(false); // this seems protected now and was not before :/
				Print(string.Format("Breaking Contact RTC -  Disabling Engine due to transmission started"), LogLevel.NORMAL);
			}
		}
	}
	
	/*
	GRAD_TransmissionPoint GetNearestTPC(vector center) {
		GRAD_TransmissionPoint nearestPoint;	
		array<GRAD_TransmissionPoint> transmissionPoints = GetTransmissionPoints();	
		int transmissionPointsCount = transmissionPoints.Count();
		
		// if transmission points exist, find out which one is the nearest
		if (transmissionPointsCount > 0) {
			float minDistance = 999999;
			
			PrintFormat("Found %1 transmission points", transmissionPointsCount);

			foreach (GRAD_TransmissionPoint TPCAntenna : transmissionPoints)
			{
				float distance = vector.Distance(TPCAntenna.GetPosition(), center);

				// check if distance is in reach of radiotruck
				if (distance < minDistance) {
					minDistance = distance;
					nearestPoint = TPCAntenna;
					
					PrintFormat("Nearest TPC is %1 s", nearestPoint);
				}
			}
		}
		return nearestPoint;
	}
	*/
	
	/*
	array<GRAD_TransmissionPoint> GetTransmissionPoints() {
		array<GRAD_TransmissionPoint> allPoints;
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(SCR_PlayerController.GetLocalPlayerId()));
		if (!playerController) {
			return allPoints;
		}
		
		GRAD_BC_BreakingContactManager BCM = playerController.FindBreakingContactManager();
		if (!BCM) {
			return allPoints;
		}
		
		return(BCM.GetTransmissionPoints());
	}
	*/

	//------------------------------------------------------------------------------------------------
	void SyncVariables()
	{
		Rpc(RpcAsk_Authority_SyncVariables);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_SyncVariables()
	{
		//Print("BC Debug - RpcAsk_Authority_SyncTransmissionDuration()", LogLevel.NORMAL);

		Replication.BumpMe();
	}


}
