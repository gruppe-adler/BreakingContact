// Subclass of ACE_CarriableEntityComponent that removes the CTRL+X (ACE_StopCarrying)
// keybind for releasing. The antenna drag uses its own "Release Antenna" (F key) user action
// via GRAD_BC_ReleaseAntennaAction instead.
// Also prevents ACE from auto-releasing when a weapon is selected, since the carrier input
// restrictions already block weapon switching during drag.

class GRAD_BC_CarriableAntennaComponentClass : ACE_CarriableEntityComponentClass
{
}

class GRAD_BC_CarriableAntennaComponent : ACE_CarriableEntityComponent
{
	//------------------------------------------------------------------------------------------------
	// Override to skip registering the ACE_StopCarrying (CTRL+X) keybind
	// while keeping all other owner-side handlers (EOnFrame for input limits, weapon manager)
	override protected void AttachHandlersOwner()
	{
		// Intentionally skip: GetGame().GetInputManager().AddActionListener("ACE_StopCarrying", ...)
		// The release is handled by GRAD_BC_ReleaseAntennaAction (F key) instead

		SetEventMask(GetOwner(), EntityEvent.FRAME);

		if (!m_pCarrier)
			return;

		BaseWeaponManagerComponent weaponManager = m_pCarrier.GetWeaponManager();
		if (!m_bCarrierAllowWeapon && weaponManager)
			weaponManager.m_OnWeaponChangeStartedInvoker.Insert(OnCarrierWeaponSelected);

		SCR_CharacterControllerComponent charController = SCR_CharacterControllerComponent.Cast(m_pCarrier.GetCharacterController());
		if (!charController)
			return;

		// Put away weapon
		if (!m_bCarrierAllowWeapon)
			charController.SelectWeapon(null);

		// Make sure we are in an allowed stance
		switch (charController.GetStance())
		{
			case ECharacterStance.STAND:
			{
				if (m_bCarrierAllowStandStance)
					break;

				if (m_bCarrierAllowCrouchStance)
					charController.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOCROUCH);
				else
					charController.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOPRONE);
			}

			case ECharacterStance.CROUCH:
			{
				if (m_bCarrierAllowCrouchStance)
					break;

				if (m_bCarrierAllowStandStance)
					charController.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOERECTED);
				else
					charController.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOPRONE);

				break;
			}

			default:
			{
				if (m_bCarrierAllowProneStance)
					break;

				if (m_bCarrierAllowCrouchStance)
					charController.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOCROUCH);
				else
					charController.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOERECTED);

				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Override to skip removing the ACE_StopCarrying listener (since we never registered it)
	override protected void DetachHandlersOwner()
	{
		// Intentionally skip: GetGame().GetInputManager().RemoveActionListener("ACE_StopCarrying", ...)
		ClearEventMask(GetOwner(), EntityEvent.FRAME);

		if (!m_pCarrier)
			return;

		BaseWeaponManagerComponent weaponManager = m_pCarrier.GetWeaponManager();
		if (!m_bCarrierAllowWeapon && weaponManager)
			weaponManager.m_OnWeaponChangeStartedInvoker.Remove(OnCarrierWeaponSelected);
	}

	//------------------------------------------------------------------------------------------------
	// Override to prevent ACE from auto-releasing when a weapon is selected.
	// The EOnFrame input restrictions already block weapon switching while dragging.
	override protected void OnCarrierWeaponSelected(BaseWeaponComponent newWeaponSlot)
	{
		// Do nothing - weapon switching is blocked by input restrictions in EOnFrame
		// and the drag should only be released via GRAD_BC_ReleaseAntennaAction (F key)
	}
}
