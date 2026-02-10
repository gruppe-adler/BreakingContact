class GRAD_BC_ReleaseAntennaAction : ScriptedUserAction
{
	private GRAD_BC_DraggableComponent m_DraggableComponent;

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false; // Runs on server
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_DraggableComponent)
			return false;

		// Only show if currently being dragged by this user
		if (!m_DraggableComponent.IsDragged())
			return false;

		return IsUserTheDragger(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_DraggableComponent)
			return false;

		// Only allow if currently being dragged
		if (!m_DraggableComponent.IsDragged())
			return false;

		return IsUserTheDragger(user);
	}

	//------------------------------------------------------------------------------------------------
	// Check if this user is the one currently dragging the antenna
	protected bool IsUserTheDragger(IEntity user)
	{
		if (!user || !m_DraggableComponent)
			return false;

		IEntity dragger = m_DraggableComponent.GetDragger();
		if (!dragger)
			return false;

		return (dragger == user);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_DraggableComponent)
		{
			Print("BC Debug - ReleaseAntennaAction: m_DraggableComponent is null", LogLevel.ERROR);
			return;
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - ReleaseAntennaAction: PerformAction - releasing antenna", LogLevel.NORMAL);

		m_DraggableComponent.StopDrag();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Release Antenna";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_DraggableComponent = GRAD_BC_DraggableComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_DraggableComponent));
	}
}
