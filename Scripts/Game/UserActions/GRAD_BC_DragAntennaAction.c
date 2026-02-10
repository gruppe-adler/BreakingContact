class GRAD_BC_DragAntennaAction : ScriptedUserAction
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
		// Only show for OPFOR players
		if (!IsUserOpfor(user))
			return false;

		if (!m_DraggableComponent)
			return false;

		// Don't show if already being dragged
		if (m_DraggableComponent.IsDragged())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_DraggableComponent)
			return false;

		// Only allow for OPFOR players
		if (!IsUserOpfor(user))
			return false;

		return m_DraggableComponent.CanBeDraggedBy(user);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsUserOpfor(IEntity user)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		string factionKey = character.GetFactionKey();
		return (factionKey == "USSR");
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_DraggableComponent)
		{
			Print("BC Debug - DragAntennaAction: m_DraggableComponent is null", LogLevel.ERROR);
			return;
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - DragAntennaAction: PerformAction - starting drag", LogLevel.NORMAL);

		m_DraggableComponent.StartDrag(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Drag Antenna";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_DraggableComponent = GRAD_BC_DraggableComponent.Cast(pOwnerEntity.FindComponent(GRAD_BC_DraggableComponent));
	}
}
