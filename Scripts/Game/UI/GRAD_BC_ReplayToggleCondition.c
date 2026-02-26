//------------------------------------------------------------------------------------------------
//! Condition for showing the "Hide Empty Vehicles" action hint during replay
//! Only shows when replay mode is active and empty vehicles are currently visible
[BaseContainerProps()]
class GRAD_BC_HideEmptyVehiclesCondition : SCR_AvailableActionCondition
{
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.GetInstance();
		if (!replayLayer)
		{
			Print("BC Debug - HideEmptyVehiclesCondition: replayLayer is null", LogLevel.WARNING);
			return false;
		}

		bool result = replayLayer.IsInReplayMode() && !replayLayer.IsHidingEmptyVehicles();
		Print(string.Format("BC Debug - HideEmptyVehiclesCondition: IsInReplayMode=%1 IsHidingEmptyVehicles=%2 result=%3", replayLayer.IsInReplayMode(), replayLayer.IsHidingEmptyVehicles(), result), LogLevel.WARNING);
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! Condition for showing the "Show Empty Vehicles" action hint during replay
//! Only shows when replay mode is active and empty vehicles are currently hidden
[BaseContainerProps()]
class GRAD_BC_ShowEmptyVehiclesCondition : SCR_AvailableActionCondition
{
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.GetInstance();
		if (!replayLayer)
			return false;
		
		return replayLayer.IsInReplayMode() && replayLayer.IsHidingEmptyVehicles();
	}
}

//------------------------------------------------------------------------------------------------
//! Condition for showing the "Hide Civilians" action hint during replay
//! Only shows when replay mode is active and civilians are currently visible
[BaseContainerProps()]
class GRAD_BC_HideCiviliansCondition : SCR_AvailableActionCondition
{
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.GetInstance();
		if (!replayLayer)
			return false;
		
		return replayLayer.IsInReplayMode() && !replayLayer.IsHidingCivilians();
	}
}

//------------------------------------------------------------------------------------------------
//! Condition for showing the "Show Civilians" action hint during replay
//! Only shows when replay mode is active and civilians are currently hidden
[BaseContainerProps()]
class GRAD_BC_ShowCiviliansCondition : SCR_AvailableActionCondition
{
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{
		GRAD_BC_ReplayMapLayer replayLayer = GRAD_BC_ReplayMapLayer.GetInstance();
		if (!replayLayer)
			return false;
		
		return replayLayer.IsInReplayMode() && replayLayer.IsHidingCivilians();
	}
}
