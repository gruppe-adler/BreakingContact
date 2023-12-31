enum ETransmissionState
{
	OFF,
	TRANSMITTING,
	INTERRUPTED,
	DISABLED,
	DONE
}

[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_TransmissionPointComponentClass : ScriptComponentClass
{
}

class GRAD_BC_TransmissionPointComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "MinDistance", params: "", category: "Breaking Contact - Transmission Point")];
	protected int m_TransmissionPointMinDistance;

	[RplProp()]
	protected ETransmissionState m_eTransmissionState;

	[RplProp()]
	protected float m_iTransmissionProgress;

	static float m_iTransmissionDuration = 120.0; // todo make param, 120s for debug
	static float m_iTransmissionUpdateTickSize = 1.0 /m_iTransmissionDuration;

	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private IEntity m_transmissionPoint;

	private RplComponent m_RplComponent;

	protected bool m_bTransmissionActive;

	void YourComponent(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		//Print("BC Debug - OnPostInit()", LogLevel.NORMAL);

		m_transmissionPoint = IEntity.Cast(GetOwner());

		m_mapDescriptorComponent = SCR_MapDescriptorComponent.Cast(m_transmissionPoint.FindComponent(SCR_MapDescriptorComponent));

		m_RplComponent = RplComponent.Cast(m_transmissionPoint.FindComponent(RplComponent));

		//PrintFormat("BC Debug - IsMaster(): %1", m_RplComponent.IsMaster()); // IsMaster() does not mean Authority
		//PrintFormat("BC Debug - IsProxy(): %1", m_RplComponent.IsProxy());
		//PrintFormat("BC Debug - IsOwner(): %1", m_RplComponent.IsOwner());

		// Initially set transmission state to off and disable the map marker
		//SetTransmissionState(m_eTransmissionState);
		GetGame().GetCallqueue().CallLater(SetTransmissionState, 5000, false, m_eTransmissionState);

		if (m_RplComponent.IsMaster())
			GetGame().GetCallqueue().CallLater(MainLoop, 1000, true);
	}
	//------------------------------------------------------------------------------------------------
	int GetTransmissionDuration()
	{
		//RpcAsk_Authority_SyncVariables();

		return m_iTransmissionProgress;
	}

	//------------------------------------------------------------------------------------------------
	ETransmissionState GetTransmissionState()
	{
		return m_eTransmissionState;
	}

	//------------------------------------------------------------------------------------------------
	void SetTransmissionState(ETransmissionState transmissionState)
	{
		if (m_eTransmissionState != transmissionState) {
			m_eTransmissionState = transmissionState;

			if (m_eTransmissionState == ETransmissionState.TRANSMITTING)
				SetTransmissionPointMarkerVisibility(true);
			else
				SetTransmissionPointMarkerVisibility(false);
		}
			/*
			int playerId = GetGame().GetDataCollector().GetPlayerData(GetGame().GetPlayerController().GetPlayerId());
			if (playerId) {
				SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			*/
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController) return;
		GRAD_BC_Transmission info = GRAD_BC_Transmission.Cast(playerController.FindComponent(GRAD_BC_Transmission));
		if (!info) {
			PrintFormat("TPC - No Info Panel found");
			return;
		}

		switch (m_eTransmissionState)
			{
				case ETransmissionState.TRANSMITTING: {
						info.TransmissionStarted();
					break;
				}

				case ETransmissionState.INTERRUPTED: {
						info.TransmissionInterrupted();
					break;
				}

				case ETransmissionState.DISABLED: {
					info.TransmissionInterrupted();
					break;
				}

				case ETransmissionState.DONE: {
					info.TransmissionDone();
					break;
				}

				default: {
					break;
				}
			}

	}


	//------------------------------------------------------------------------------------------------
	void SetTransmissionActive(bool setState) {
		if (m_bTransmissionActive != setState) {
			m_bTransmissionActive = setState;

			if (m_bTransmissionActive &&
				(
					GetTransmissionState() == ETransmissionState.OFF ||
					GetTransmissionState() == ETransmissionState.INTERRUPTED
				)
			) {
				SetTransmissionState(ETransmissionState.TRANSMITTING);
			};

			if (!m_bTransmissionActive &&
				(
					GetTransmissionState() == ETransmissionState.TRANSMITTING
				)
			) {
				SetTransmissionState(ETransmissionState.INTERRUPTED);

			};
			PrintFormat("TPC - SetTransmissionActive : %1", GetTransmissionState());

		}
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

	//------------------------------------------------------------------------------------------------
	protected void MainLoop()
	{
		// this function runs on server-side only


		if (m_mapDescriptorComponent) {
			MapItem item;
			item = m_mapDescriptorComponent.Item();
			MapDescriptorProps props = item.GetProps();

			if (GetTransmissionState() == ETransmissionState.TRANSMITTING)
			{
				m_iTransmissionProgress += m_iTransmissionUpdateTickSize;
				float currentProgress = Math.Floor(m_iTransmissionProgress * 100);

				PrintFormat("m_iTransmissionDuration: %1", m_iTransmissionDuration);
				PrintFormat("m_iTransmissionUpdateTickSize: %1", m_iTransmissionUpdateTickSize);
				PrintFormat("m_iTransmissionProgress: %1", m_iTransmissionProgress);

				string progressString = string.Format("Antenna: %1 \%", currentProgress); // % needs to be escaped

				item.SetDisplayName(progressString);
				props.SetIconVisible(true);
				props.SetBackgroundColor(Color.Red);
				props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
				props.SetImageDef("{534DF45C06CFB00C}UI/Textures/Map/transmission_active.edds");
				props.SetFrontColor(Color.FromRGBA(0, 0, 0, 0));
				props.SetOutlineColor(Color.Black);
				props.SetTextColor(Color.White);
				props.SetTextSize(60.0, 30.0, 60.0);
				props.SetIconSize(32, 0.3, 0.3);
				props.Activate(true);
				item.SetProps(props);
			} else if (GetTransmissionState() == ETransmissionState.DISABLED) {
				props.SetIconVisible(true);
				props.SetBackgroundColor(Color.Black);
				props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
				props.SetImageDef("{97BB746698125B85}UI/Textures/Map/transmission_destroyed.edds");
				props.SetFrontColor(Color.FromRGBA(0, 0, 0, 0));
				props.SetTextColor(Color.White);
				props.SetTextSize(30.0, 30.0, 30.0);
				props.SetIconSize(32, 0.3, 0.3);
				props.Activate(true);
				item.SetProps(props);
			} else {
				props.SetIconVisible(true);
				props.SetTextColor(Color.Gray75);
				props.SetFont("{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt");
				props.SetImageDef("{97BB746698125B85}UI/Textures/Map/transmission_default.edds");
				props.SetFrontColor(Color.FromRGBA(0, 0, 0, 0));
				props.SetTextColor(Color.Black);
				props.SetTextSize(30.0, 30.0, 30.0);
				props.SetIconSize(32, 0.3, 0.3);
				props.Activate(true);
				item.SetProps(props);
			}

		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SetTransmissionPointMarkerVisibility(bool enableMarkerVisibility)
	{
		// this function runs on server-side only

		if (!m_mapDescriptorComponent)
		{
			Print("BC Debug - m_mapDescriptorComponent is null", LogLevel.ERROR);
			return;
		}

		if (enableMarkerVisibility && !(m_mapDescriptorComponent.Item().IsVisible()))
		{
			m_mapDescriptorComponent.Item().SetVisible(true);
			//Rpc(RpcAsk_Authority_SetMarkerVisibility, true);
			//SCR_HintManagerComponent.GetInstance().ShowCustomHint("MARKER ON");
			Print("BC Debug - marker on", LogLevel.NORMAL);
		}
		else if (!enableMarkerVisibility && (m_mapDescriptorComponent.Item().IsVisible()))
		{
			m_mapDescriptorComponent.Item().SetVisible(false);
			//Rpc(RpcAsk_Authority_SetMarkerVisibility, false);
			//SCR_HintManagerComponent.GetInstance().ShowCustomHint("MARKER OFF");
			Print("BC Debug - marker off", LogLevel.NORMAL);
		}
	}
}
