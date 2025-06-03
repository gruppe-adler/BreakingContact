class GRAD_BC_GetRadioTransmissionDuration : ScriptedUserAction
{
	// This scripted user action if triggered runs on all clients and server
	// But in code execution is filtered on performing user und server
	
	private GRAD_BC_RadioTruckComponent m_radioTruckComponent;
	
	private RplComponent m_RplComponent;

	// comment from discord:
	// if HasLocalEffectOnly returns true, it will be executing only on the client where the action has been trigerred 
	// if HasLocalEffectOnly returns false, then it will be exeucted only on the client where the action has been trigered and server --> perhaps wrong
	// if HasLocalEffectOnly returns false and CanBroadcast returns true, then it will be exeucted on client where the action has been trigerred and server and everybody else.    
	
	// comment from discord:
	// if HasLocalEffectOnlyScript() TRUE: actions script run only locally.
	// if FALSE:  "CanBeShownScript()" and "CanBePerformedScript()" run locally on client but "PerformAction()" run on server
	    
	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
	    return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
	    return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		
		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return false;
		}
		
		if (m_radioTruckComponent.GetTransmissionActive())
		{
			return true;
		} else
		{
			return false;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		//Print("BC Debug - PerformAction() GetRadioTransmissionDuration", LogLevel.NORMAL);
		
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		
		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return;
		}
		
		if(m_RplComponent.IsMaster() && m_radioTruckComponent) {
			m_radioTruckComponent.SyncVariables();
		}
		
		bool isTransmitting = m_radioTruckComponent.GetTransmissionActive();
		
		GRAD_BC_BreakingContactManager BCM = GRAD_BC_BreakingContactManager.GetInstance();
		if (!BCM) return;
		
		GRAD_BC_TransmissionComponent TPC = BCM.GetNearestTransmissionPoint(pUserEntity.GetOrigin(), isTransmitting);
		if (!TPC) return;

		if (playerId == GetGame().GetPlayerController().GetPlayerId())
		{
			string message = string.Format("Progress: %1 %%", Math.Floor(TPC.GetTransmissionDuration() * 100));
			SCR_HintManagerComponent.GetInstance().ShowCustomHint(message, "Breaking Contact", 10.0);		
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_RadioTruckComponent));
		
		m_RplComponent = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
	}
}