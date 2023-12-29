[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_RadioTruckComponentClass : ScriptComponentClass
{
}

class GRAD_BC_RadioTruckComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "Update Interval", params: "", category: "Breaking Contact - Radio Truck")];
	protected int m_iRadioTransmissionUpdateInterval;

	static float m_iMaxTransmissionDistance = 500.0;

	private bool m_bIsTransmitting;

	private Vehicle m_radioTruck;

	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private VehicleWheeledSimulation_SA m_VehicleWheeledSimulationComponent;

	private RplComponent m_RplComponent;

	private GRAD_BC_TransmissionPointComponent m_nearestTransmissionPoint;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		//Print("BC Debug - OnPostInit()", LogLevel.NORMAL);

		m_radioTruck = Vehicle.Cast(GetOwner());

		m_mapDescriptorComponent = SCR_MapDescriptorComponent.Cast(m_radioTruck.FindComponent(SCR_MapDescriptorComponent));
		m_VehicleWheeledSimulationComponent = VehicleWheeledSimulation_SA.Cast(m_radioTruck.FindComponent(VehicleWheeledSimulation_SA));

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
			MapItem item;
			item = m_mapDescriptorComponent.Item();
			MapDescriptorProps props = item.GetProps();

				if (m_bIsTransmitting)
				{
					Print(string.Format("Breaking Contact RTC - Transmitting..."), LogLevel.NORMAL);

					// todo
					IEntity TPCAntenna = getNearestTransmissionPoint(m_radioTruck.GetOrigin());
					if (!TPCAntenna) {
						Print(string.Format("Breaking Contact RTC -  No Transmission Point found"), LogLevel.NORMAL);
						return;
					}

					GRAD_BC_TransmissionPointComponent TPC = GRAD_BC_TransmissionPointComponent.Cast(TPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
					if (TPC) {
						TPC.SetTransmissionState(true);
						Print(string.Format("Breaking Contact RTC - TPCAntenna: %1 - Component: %2", TPCAntenna, TPC), LogLevel.NORMAL);
					} else {
						Print(string.Format("Breaking Contact RTC - No GRAD_BC_TransmissionPointComponent found"), LogLevel.NORMAL);
					}

					string progressString = string.Format("Radio Truck active");

					item.SetDisplayName(progressString);
					props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
					props.SetImageDef("{243D963F2E18E435}UI/Textures/Map/radiotruck_active.edds");
					props.SetIconVisible(true);
					props.SetFrontColor(Color.FromRGBA(0, 0, 0, 0));
					props.SetOutlineColor(Color.Black);
					props.SetTextColor(Color.Red);
					props.SetTextSize(30.0, 30.0, 30.0);
					props.SetIconSize(30, 30, 30);
					props.Activate(true);
					item.SetProps(props);
				} else {
					props.SetIconVisible(true);
					props.SetFrontColor(Color.FromRGBA(0, 0, 0, 0));
					props.SetOutlineColor(Color.Black);
					props.SetTextColor(Color.FromRGBA(0, 0, 0, 0));
					props.SetTextSize(30.0, 30.0, 30.0);
					props.SetIconSize(3, 3, 3);
					props.Activate(true);
					item.SetProps(props);
				}

				
			
			
		}

		Print(string.Format("Breaking Contact RTC -  Main Loop Tick"), LogLevel.NORMAL);
	}




	//------------------------------------------------------------------------------------------------
	protected IEntity getNearestTransmissionPoint(vector center)
	{
		GRAD_BreakingContactManager BCM = GRAD_BreakingContactManager.Cast(GetGame().FindEntity("GRAD_BreakingContactManager"));
		if (BCM) {
			array<IEntity> transmissionPoints = BCM.GetTransmissionPoints();
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
				if (!selectedPoint) {
					selectedPoint = createTransmissionPoint(center, BCM);
				}
				return selectedPoint;
			}
			selectedPoint = createTransmissionPoint(center, BCM);
			return selectedPoint;
		}

		return null;
	}


	//------------------------------------------------------------------------------------------------
	IEntity createTransmissionPoint(vector center, GRAD_BreakingContactManager BCM) {
		// if no transmission point exists, create one
		IEntity TPCAntenna = BCM.spawnTransmissionPoint(center, 10);
		Print(string.Format("Breaking Contact RTC -  Create TransmissionPoint: %1", TPCAntenna), LogLevel.NORMAL);

		AddTransmissionMarker(TPCAntenna, m_iMaxTransmissionDistance);
		return TPCAntenna;
	}

	//------------------------------------------------------------------------------------------------
	void AddTransmissionMarker(IEntity TPCAntenna, float radius)
	{

		vector center = TPCAntenna.GetOrigin();

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetAllPlayers(playerIds);

		foreach (int playerId : playerIds)
		{

			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));

			if (!playerController)
				return;

			playerController.AddCircleMarker(
				center[0] - radius,
				center[2] + radius,
				center[0] + radius,
				center[2] + radius
			);
		}
	}


	bool GetTransmissionState() {
		return m_bIsTransmitting;
	}

	void SetTransmissionActive(bool setTo) {
		m_bIsTransmitting = setTo;

		// disable transmissions for every transmission point
		if (!m_bIsTransmitting) {
			GRAD_BreakingContactManager BCM = GRAD_BreakingContactManager.Cast(GetGame().FindEntity("GRAD_BreakingContactManager"));
			if (BCM) {
				array<IEntity> transmissionPoints = BCM.GetTransmissionPoints();

				foreach (IEntity TPCAntenna : transmissionPoints)
				{
					GRAD_BC_TransmissionPointComponent TPC = GRAD_BC_TransmissionPointComponent.Cast(TPCAntenna.FindComponent(GRAD_BC_TransmissionPointComponent));
					if (TPC) {
						TPC.SetTransmissionState(false);
						Print(string.Format("Breaking Contact RTC -  Disabling Transmission at: %1", TPCAntenna), LogLevel.NORMAL);
					}
				}
			}
		}
	}

	int GetTransmissionDuration() {
		int duration;

		// todo get nearest transmission, get duration of that

		return duration;
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
