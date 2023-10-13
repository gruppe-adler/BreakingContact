class GRAD_BC_ToggleRadioTransmission : ScriptedUserAction
{
	// This scripted user action if triggered runs on all clients and server
	
	private GRAD_BC_RadioTruckComponent m_radioTruckComponent;

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
	    return false;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
	    return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return CanBePerformedScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		Print("BC Debug - PerformAction() ToggleRadioTransmission", LogLevel.NORMAL);

		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return;
		}
		
		if(m_radioTruckComponent.GetRadioTransmissionState() == ERadioTransmissionState.TRANSMITTING)
			m_radioTruckComponent.SetRadioTransmissionState(ERadioTransmissionState.INTERRUPTED);
		else
			m_radioTruckComponent.SetRadioTransmissionState(ERadioTransmissionState.TRANSMITTING);
	}
	
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_radioTruckComponent)
		{
			Print("BC Debug - m_radioTruckComponent is null", LogLevel.ERROR);
			return false;
		}
		
		if (m_radioTruckComponent.GetRadioTransmissionState() == ERadioTransmissionState.TRANSMITTING)
		{
			outName = "Stop Radio Transmission";
		} else
		{
			outName = "Start Radio Transmission";
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_radioTruckComponent = GRAD_BC_RadioTruckComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_RadioTruckComponent));
	}
}