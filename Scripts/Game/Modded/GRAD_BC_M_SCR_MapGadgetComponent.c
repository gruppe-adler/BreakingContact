//------------------------------------------------------------------------------------------------
//! Keeps the map open while a commander is picking the spawn position, so they cannot move around
//! before the team has spawned in.
//!
//! The lock is deliberately narrow, because trapping a player in the map for a whole round is far
//! worse than a commander walking a few metres early. Four things bound it:
//!
//!  1. Scripted closes always pass. BC closes the map itself on phase change, through this same
//!     ModeClear - GRAD_PlayerComponent.ToggleMap raises a bypass flag so the game can always close
//!     a map it opened. IsMapCloseLocked() is the single place that decision is made.
//!  2. It ends at confirmation, not at the phase change. ConfirmSpawn clears m_bChoosingSpawn.
//!  3. JIP players are never locked: IsChoosingSpawn() returns false outside OPFOR/BLUFOR, and
//!     ForceOpenMap only arms the flag for a commander who joined during a placement phase.
//!  4. A phase check backstops the flag, so a stale m_bChoosingSpawn cannot outlive the phases the
//!     lock is meant for.
modded class SCR_MapGadgetComponent
{
	//------------------------------------------------------------------------------------------------
	override void ModeClear(EGadgetMode mode)
	{
		if (GRAD_BC_IsMapCloseBlocked())
		{
			if (GRAD_BC_BreakingContactManager.IsDebugMode())
				Print("BC Debug - MapGadget: player close blocked, spawn selection in progress", LogLevel.NORMAL);

			return;
		}

		super.ModeClear(mode);
	}

	//------------------------------------------------------------------------------------------------
	//! True only when the LOCAL player is mid spawn-selection and this is a player-initiated close.
	protected bool GRAD_BC_IsMapCloseBlocked()
	{
		// Backstop: never hold the map shut outside the two placement phases, whatever the local
		// flags say. This is what guarantees the lock cannot survive into the match itself.
		GRAD_BC_BreakingContactManager bcm = GRAD_BC_BreakingContactManager.GetInstance();
		if (!bcm)
			return false;

		EBreakingContactPhase phase = bcm.GetBreakingContactPhase();
		if (phase != EBreakingContactPhase.OPFOR && phase != EBreakingContactPhase.BLUFOR)
			return false;

		if (!GRAD_BC_IsLocalPlayerGadget())
			return false;

		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
		if (!playerComponent)
			return false;

		return playerComponent.IsMapCloseLocked();
	}

	//------------------------------------------------------------------------------------------------
	//! True when this gadget belongs to the locally controlled character. The component also runs on
	//! other players' map gadgets, and their spawn state is not ours to enforce.
	protected bool GRAD_BC_IsLocalPlayerGadget()
	{
		IEntity localEntity = SCR_PlayerController.GetLocalControlledEntity();
		if (!localEntity)
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		return owner.GetRootParent() == localEntity;
	}
}
