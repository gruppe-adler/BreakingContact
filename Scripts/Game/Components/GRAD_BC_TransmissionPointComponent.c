enum ETransmissionState
{
	OFF,
	TRANSMITTING,
	INTERRUPTED,
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
	protected int m_iTransmissionProgress;
	
	static int m_iTransmissionDuration = 120; // todo make param, 120s for debug
	static int m_iTransmissionUpdateTickSize = 1/m_iTransmissionDuration;

	private SCR_MapDescriptorComponent m_mapDescriptorComponent;
	private IEntity m_transmissionPoint;

	private RplComponent m_RplComponent;

	protected bool m_bTransmissionActive;

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

		if (GetTransmissionState() == ETransmissionState.TRANSMITTING)
		{
			m_iTransmissionProgress += m_iTransmissionUpdateTickSize;
			float currentProgress = m_iTransmissionProgress/m_iTransmissionDuration;
			currentProgress = Math.Floor(currentProgress);
			
			PrintFormat("m_iTransmissionProgress: %1", m_iTransmissionProgress);
						
			if (m_mapDescriptorComponent) {
				MapItem item;
				item = m_mapDescriptorComponent.Item();	
				MapDescriptorProps props = item.GetProps();
				
				string progressString = string.Format("%1 %", currentProgress.ToString());
				
				item.SetDisplayName(progressString);
				props.SetDetail(96);
			
				Color textColor = Color(0, 0, 0, 1);
				Color outlineColor = Color(1, 1, 1, 1);
			
				props.SetOutlineColor(outlineColor);
				props.SetTextColor(textColor);
				props.SetTextBold();
				props.SetGroupScale(5);
				props.SetTextSize( 20.0, 10.0, 20.0 );
			
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
