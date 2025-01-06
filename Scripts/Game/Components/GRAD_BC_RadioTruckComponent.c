[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_RadioTruckComponentClass : ScriptComponentClass
{
}

class GRAD_BC_RadioTruckComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "Update Interval", params: "", category: "Breaking Contact - Radio Truck")];
	protected int m_iRadioTransmissionUpdateInterval;

	static float m_iMaxTransmissionDistance = 500.0;

	protected bool m_bIsTransmitting;

	private Vehicle m_radioTruck;

	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private VehicleWheeledSimulation_SA_B m_VehicleWheeledSimulationComponent;

	private RplComponent m_RplComponent;

	private GRAD_BC_TransmissionPointComponent m_nearestTransmissionPoint;

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

		if (m_mapDescriptorComponent) {
			adjustMapItem(GetTransmissionActive());	
		}
		
		if (GetTransmissionActive()) { applyBrakes(); }
	}
	
	void adjustMapItem(bool active) {
		MapItem item;
		item = m_mapDescriptorComponent.Item();
		MapDescriptorProps props = item.GetProps();
		
		if (active)
		{
			Print(string.Format("Breaking Contact RTC - Transmitting..."), LogLevel.NORMAL);

			string progressString = string.Format("Radio Truck active");

			props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
			item.SetImageDef("{9C5B2BA4695A421C}UI/Textures/Icons/GRAD_BC_mapIcons.imageset.edds");
	        // props.SetImageDef("{3E2F061E35D2DA76}UI/Textures/Icons/GRAD_BC_mapIcons.imageset");
			props.SetIconVisible(true);
			props.SetFrontColor(Color.FromRGBA(0, 0, 0, 0));
			props.SetOutlineColor(Color.Black);
			props.SetTextColor(Color.Red);
			props.SetTextSize(30.0, 30.0, 30.0);
			props.SetTextOffsetX(-10);
			props.SetTextOffsetY(-16.5);
			props.SetTextBold();
			props.SetIconSize(30.0, 30.0, 30.0);
			props.Activate(true);
			item.SetProps(props);
			item.SetDisplayName(progressString);
			item.SetVisible(true);
			
		} else {
			props.SetIconVisible(true);
			props.SetFrontColor(Color.FromRGBA(0, 0, 0, 0));
			props.SetOutlineColor(Color.Black);
			props.SetTextColor(Color.FromRGBA(0, 0, 0, 0));
			props.SetTextSize(30.0, 30.0, 30.0);
			props.SetIconSize(3.0, 3.0, 3.0);
			props.Activate(true);
			item.SetProps(props);
		}	
	}
	
	void applyBrakes() {
		RplComponent rplComp = RplComponent.Cast(m_radioTruck.FindComponent(RplComponent));
		// currently log is on server always, even when players steer the truck :/
		if (!rplComp.IsProxy()) {
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

	float GetTransmissionDuration() {
		float duration = -1.0;
		vector positionRadioTruck = m_radioTruck.GetOrigin();

		// todo get nearest transmission, get duration of that
		GRAD_BC_TransmissionPointComponent nearestTPC = GetNearestTPC(positionRadioTruck);
		array<GRAD_BC_TransmissionPointComponent> allPoints = GetTransmissionPoints();
		
		if (nearestTPC) {
			duration = nearestTPC.GetTransmissionDuration();
		}
		
		return duration;
	}
	
	
	GRAD_BC_TransmissionPointComponent GetNearestTPC(vector center) {
		GRAD_BC_TransmissionPointComponent nearestPoint = null;	
		array<GRAD_BC_TransmissionPointComponent> transmissionPoints = GetTransmissionPoints();	
		
		int transmissionPointsCount = transmissionPoints.Count();
		// if transmission points exist, find out which one is the nearest
		if (transmissionPointsCount > 0) {
			float minDistance = 999999;
			
			PrintFormat("Found %1 transmission points", transmissionPointsCount);

			foreach (GRAD_BC_TransmissionPointComponent TPCAntenna : transmissionPoints)
			{
				float distance = vector.Distance(TPCAntenna.GetOrigin(), center);

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
	
	array<GRAD_BC_TransmissionPointComponent> GetTransmissionPoints() {
		array<GRAD_BC_TransmissionPointComponent> allPoints;
		
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


	/*

	//Check if garage is nearby
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fGarageSearchRadius, FindFirstGarage, FilterGarage);
		return (m_GarageManager);
	}

	//------------------------------------------------------------------------------------------------
	bool FilterGarage(IEntity ent)
	{
		return (ent.FindComponent(EL_GarageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	bool FindFirstGarage(IEntity ent)
	{
		m_GarageManager = EL_GarageManagerComponent.Cast(ent.FindComponent(EL_GarageManagerComponent));
		if (!m_GarageManager)
			return true; //Continue search

		return false; //Stop search
	}

	*/






	/*
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_SetMarkerVisibility(bool isVisible)
	{
		Print("BC Debug - RpcAsk_Authority_SetMarkerVisibility()", LogLevel.NORMAL);

		m_mapDescriptorComponent.Item().SetVisible(isVisible);

		m_bIsVisible = isVisible;

		Replication.BumpMe();

		Rpc(RpcDo_Broadcast_SetMarkerVisibility, isVisible);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Broadcast_SetMarkerVisibility(bool isVisible)
	{
		Print("BC Debug - RpcDo_Broadcast_SetMarkerVisibility()", LogLevel.NORMAL);

		m_mapDescriptorComponent.Item().SetVisible(isVisible);
	}
	*/
}
