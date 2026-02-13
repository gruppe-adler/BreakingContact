[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "Makes an entity draggable by players")]
class GRAD_BC_DraggableComponentClass : ScriptComponentClass
{
}

class GRAD_BC_DraggableComponent : ScriptComponent
{
	// Offset behind the dragger (meters behind character facing direction)
	[Attribute("-1.5", UIWidgets.EditBox, "Distance behind the dragger to position the object")]
	protected float m_fDragOffsetBehind;

	// Height offset for the dragged object
	[Attribute("0.0", UIWidgets.EditBox, "Height offset for the dragged object (added to surface Y)")]
	protected float m_fDragHeightOffset;

	// Replicated: RplId of the entity currently dragging this object (-1 = not being dragged)
	[RplProp(onRplName: "OnDragStateChanged")]
	protected RplId m_DraggerRplId = RplId.Invalid();

	// Local reference to dragger character entity (resolved from m_DraggerRplId)
	protected IEntity m_DraggerEntity;

	// Position update interval in milliseconds
	protected static const int DRAG_UPDATE_INTERVAL = 100;

	// Reference to the RplComponent on this entity
	protected RplComponent m_RplComponent;

	// Cached reference to the transmission component
	protected GRAD_BC_TransmissionComponent m_TransmissionComponent;

	// Cached reference to the ACE carriable component
	protected ACE_CarriableEntityComponent m_CarriableComponent;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		m_TransmissionComponent = GRAD_BC_TransmissionComponent.Cast(owner.FindComponent(GRAD_BC_TransmissionComponent));
		m_CarriableComponent = ACE_CarriableEntityComponent.Cast(owner.FindComponent(ACE_CarriableEntityComponent));
	}

	//------------------------------------------------------------------------------------------------
	// Check if this object is currently being dragged
	bool IsDragged()
	{
		return m_DraggerRplId != RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	// Get the entity currently dragging this object
	IEntity GetDragger()
	{
		return m_DraggerEntity;
	}

	//------------------------------------------------------------------------------------------------
	// Get the RplId of the dragger
	RplId GetDraggerRplId()
	{
		return m_DraggerRplId;
	}

	//------------------------------------------------------------------------------------------------
	// Called on server to start dragging
	void StartDrag(IEntity draggerEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!draggerEntity)
			return;

		// Already being dragged
		if (IsDragged())
			return;

		RplComponent draggerRpl = RplComponent.Cast(draggerEntity.FindComponent(RplComponent));
		if (!draggerRpl)
			return;

		m_DraggerEntity = draggerEntity;
		m_DraggerRplId = Replication.FindId(draggerRpl);
		Replication.BumpMe();

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("BC Debug - DraggableComponent: StartDrag - dragger RplId=%1", m_DraggerRplId);

		// Start ACE carrying animation on the dragger
		StartACECarryAnimation(draggerEntity);

		// Start position update loop on server
		GetGame().GetCallqueue().CallLater(UpdateDragPosition, DRAG_UPDATE_INTERVAL, true);
	}

	//------------------------------------------------------------------------------------------------
	// Called on server to stop dragging
	void StopDrag()
	{
		if (!Replication.IsServer())
			return;

		if (!IsDragged())
			return;

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			PrintFormat("BC Debug - DraggableComponent: StopDrag - was dragged by RplId=%1", m_DraggerRplId);

		// Stop ACE carrying animation on the dragger
		StopACECarryAnimation(m_DraggerEntity);

		// Stop position update loop
		GetGame().GetCallqueue().Remove(UpdateDragPosition);

		// Snap to ground at final position
		IEntity owner = GetOwner();
		if (owner)
		{
			vector pos = owner.GetOrigin();
			float surfaceY = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
			pos[1] = surfaceY + m_fDragHeightOffset;
			owner.SetOrigin(pos);
		}

		// Update the transmission component position
		if (m_TransmissionComponent)
		{
			m_TransmissionComponent.SetPosition(owner.GetOrigin());
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BC Debug - DraggableComponent: Transmission position updated after drag", LogLevel.NORMAL);
		}

		// Update BCM marker data
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (bcm)
			bcm.UpdateTransmissionMarkerData();

		// Clear dragger reference
		m_DraggerEntity = null;
		m_DraggerRplId = RplId.Invalid();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	// Activate ACE Anvil carrying state on the dragger for drag animation
	protected void StartACECarryAnimation(IEntity draggerEntity)
	{
		if (!draggerEntity)
			return;

		SCR_ChimeraCharacter draggerChar = SCR_ChimeraCharacter.Cast(draggerEntity);
		if (!draggerChar)
			return;

		SCR_CharacterControllerComponent charController = SCR_CharacterControllerComponent.Cast(draggerChar.GetCharacterController());
		if (!charController)
			return;

		// Verify the antenna has ACE_CarriableEntityComponent before attempting ACE_Carry
		if (!m_CarriableComponent)
		{
			Print("BC Debug - DraggableComponent: ACE_CarriableEntityComponent not found on antenna, skipping carry animation", LogLevel.WARNING);
			return;
		}

		// Use ACE_Carry to put the dragger into the carrying animation state
		charController.ACE_Carry(GetOwner());

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - DraggableComponent: ACE carry animation started on dragger", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Deactivate ACE Anvil carrying state on the dragger
	protected void StopACECarryAnimation(IEntity draggerEntity)
	{
		if (!draggerEntity)
			return;

		SCR_ChimeraCharacter draggerChar = SCR_ChimeraCharacter.Cast(draggerEntity);
		if (!draggerChar)
			return;

		SCR_CharacterControllerComponent charController = SCR_CharacterControllerComponent.Cast(draggerChar.GetCharacterController());
		if (!charController)
			return;

		// Only release if still carrying (ACE may have already released via CTRL+X)
		if (!charController.ACE_IsCarrier())
			return;

		// Use ACE_ReleaseCarried to remove the carrying animation state
		charController.ACE_ReleaseCarried(GetOwner());

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - DraggableComponent: ACE carry animation stopped on dragger", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Server-side periodic update to move the dragged entity to follow the dragger
	protected void UpdateDragPosition()
	{
		if (!IsDragged() || !m_DraggerEntity)
		{
			// Dragger no longer valid, stop dragging
			StopDrag();
			return;
		}

		// Check if ACE carry was released (e.g. via CTRL+X keybind)
		if (m_CarriableComponent && !m_CarriableComponent.IsCarried())
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BC Debug - DraggableComponent: ACE carry released externally, stopping drag", LogLevel.NORMAL);
			StopDrag();
			return;
		}

		// Check if dragger is still alive
		SCR_ChimeraCharacter draggerChar = SCR_ChimeraCharacter.Cast(m_DraggerEntity);
		if (!draggerChar)
		{
			StopDrag();
			return;
		}

		CharacterControllerComponent charController = draggerChar.GetCharacterController();
		if (!charController)
		{
			StopDrag();
			return;
		}

		// If dragger is dead or unconscious, stop dragging
		if (charController.IsDead())
		{
			StopDrag();
			return;
		}

		// If dragger entered a vehicle, stop dragging
		if (draggerChar.IsInVehicle())
		{
			StopDrag();
			return;
		}

		// Get dragger position and direction
		vector draggerPos = m_DraggerEntity.GetOrigin();
		vector draggerDir = m_DraggerEntity.GetTransformAxis(2); // Forward direction (Z axis)

		// Place the antenna behind the dragger
		vector newPos = draggerPos - draggerDir * m_fDragOffsetBehind;

		// Snap to terrain
		float surfaceY = GetGame().GetWorld().GetSurfaceY(newPos[0], newPos[2]);
		newPos[1] = surfaceY + m_fDragHeightOffset;

		// Update entity position
		IEntity owner = GetOwner();
		if (owner)
			owner.SetOrigin(newPos);

		// Update the transmission component position for map markers
		if (m_TransmissionComponent)
			m_TransmissionComponent.SetPosition(newPos);
	}

	//------------------------------------------------------------------------------------------------
	// Called on clients when drag state is replicated
	protected void OnDragStateChanged()
	{
		if (m_DraggerRplId != RplId.Invalid())
		{
			// Resolve dragger entity from RplId
			Managed managedObj = Replication.FindItem(m_DraggerRplId);
			RplComponent rplComp = RplComponent.Cast(managedObj);
			if (rplComp)
				m_DraggerEntity = rplComp.GetEntity();

			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				PrintFormat("BC Debug - DraggableComponent: OnDragStateChanged - now dragged by %1", m_DraggerEntity);
		}
		else
		{
			m_DraggerEntity = null;
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BC Debug - DraggableComponent: OnDragStateChanged - no longer dragged", LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Check if a specific user can drag this entity
	bool CanBeDraggedBy(IEntity user)
	{
		// Can't drag if already being dragged
		if (IsDragged())
			return false;

		// Only allow during GAME phase
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
			return false;

		if (bcm.GetBreakingContactPhase() != EBreakingContactPhase.GAME)
			return false;

		// Check transmission state - don't allow dragging DONE or DISABLED transmissions
		if (m_TransmissionComponent)
		{
			ETransmissionState state = m_TransmissionComponent.GetTransmissionState();
			if (state == ETransmissionState.DONE || state == ETransmissionState.DISABLED)
				return false;
		}

		// Check if dragger is not already carrying something via ACE
		SCR_ChimeraCharacter userChar = SCR_ChimeraCharacter.Cast(user);
		if (userChar)
		{
			SCR_CharacterControllerComponent charController = SCR_CharacterControllerComponent.Cast(userChar.GetCharacterController());
			if (charController && charController.ACE_IsCarrier())
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// Clean up the update loop if still running
		if (IsDragged() && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(UpdateDragPosition);
	}
}
