// Turns the GRAD_BC_SPECTATE editor mode entry into a "return to spectator" button.
//
// The mode itself is a duplicate of Game Master (EditorModeBCSpectate.et), so selecting it would
// otherwise just open a second Game Master - the layout it loads is Mode_Edit.layout and its camera
// is ManualCameraEdit.et. We do not try to turn it into a real spectate mode: COALITION's spectator
// is a character + camera entity pair (COA_Spectator.et / COA_SpectatorCamera.et), NOT an editor
// mode, so an editor mode can never reproduce it.
//
// Instead the entry acts purely as a trigger: the moment the player switches to it, we close the
// editor and ask the server to put them back on their COALITION spectator entity, via the same
// RPC used elsewhere for that purpose.
//
// This is attached to the mode entity prefab itself, so it only runs for our mode.
class GRAD_BC_SpectateModeTriggerClass : ScriptComponentClass
{
}

class GRAD_BC_SpectateModeTrigger : ScriptComponent
{
	protected SCR_EditorManagerEntity m_EditorManager;
	protected bool m_bListenerRegistered = false;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// The editor manager is local to the player and may not exist yet when the mode entity is
		// created, so poll briefly rather than assuming it is ready.
		GetGame().GetCallqueue().CallLater(GRAD_BC_TryRegister, 500, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void GRAD_BC_TryRegister()
	{
		if (m_bListenerRegistered)
			return;

		m_EditorManager = SCR_EditorManagerEntity.GetInstance();
		if (!m_EditorManager)
			return;

		ScriptInvoker onModeChange = m_EditorManager.GetOnModeChange();
		if (!onModeChange)
			return;

		onModeChange.Insert(GRAD_BC_OnModeChange);
		m_bListenerRegistered = true;

		GetGame().GetCallqueue().Remove(GRAD_BC_TryRegister);

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - SpectateModeTrigger: registered mode-change listener", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! The invoker's callback parameters are not documented, so read the current mode from the
	//! manager rather than relying on an argument being passed.
	protected void GRAD_BC_OnModeChange()
	{
		if (!m_EditorManager)
			return;

		if (m_EditorManager.GetCurrentMode() != EEditorMode.GRAD_BC_SPECTATE)
			return;

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print("BC Debug - SpectateModeTrigger: GRAD_BC_SPECTATE selected, returning to spectator", LogLevel.NORMAL);

		// Deferred: closing the editor from inside its own mode-change event re-enters the editor
		// state machine. One frame later it has settled.
		GetGame().GetCallqueue().CallLater(GRAD_BC_ReturnToSpectator, 1, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void GRAD_BC_ReturnToSpectator()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.Cast(playerController.FindComponent(GRAD_PlayerComponent));
		if (!playerComponent)
		{
			Print("BC Debug - SpectateModeTrigger: GRAD_PlayerComponent not found, cannot return to spectator", LogLevel.ERROR);
			return;
		}

		// Ask_EnterSpectator closes the editor itself, then asks the server to restore the
		// COALITION spectator entity and re-possess it.
		playerComponent.Ask_EnterSpectator();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(GRAD_BC_TryRegister);
			GetGame().GetCallqueue().Remove(GRAD_BC_ReturnToSpectator);
		}

		if (m_EditorManager)
		{
			ScriptInvoker onModeChange = m_EditorManager.GetOnModeChange();
			if (onModeChange)
				onModeChange.Remove(GRAD_BC_OnModeChange);
		}

		super.OnDelete(owner);
	}
}
