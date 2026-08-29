//------------------------------------------------------------------------------------------------
//! Hides the mouse cursor while the manual/ArmaVision camera is being rotated, and restores it on
//! release. Purely visual - camera behaviour is untouched.
//!
//! ATTACH: add to a SCR_ManualCamera prefab's m_aComponents array in Workbench (the same array that
//! holds SCR_RotateManualCameraComponent / SCR_MovementInertiaManualCameraComponent). It is a
//! BaseContainerProps class, so it appears when adding an element to that array - NOT in the
//! entity "Add Component" menu.
//!
//! HOW THE HIDE WORKS
//! There is no ShowCursor() in Enfusion - WidgetManager exposes only SetCursor(int). Vanilla hides
//! the hardware pointer by pointing it at a cursor visual that has no pixels:
//!
//!   SCR_CursorCustom.REAL_CURSOR_HIDE == 12   // "empty" visual ID of real cursor used to "hide" it
//!
//! SCR_CursorCustom.SetCursorVisual() uses exactly this pair - SetCursor(REAL_CURSOR_HIDE) to hide,
//! SetCursor(0) to restore - so no custom cursor asset or new EEditorCursor value is required.
//!
//! HOW ROTATION IS DETECTED
//! MEASURED, not documented: SCR_ManualCameraParam.flag reads 117 (0b1110101) idle and 119
//! (0b1110111) while right-click rotating, so bit 1 (value 2) is the rotation bit.
//!
//! Do NOT use InputManager.GetActionValue("ManualCameraRotateYaw"/"RotatePitch") for this. Those
//! return mouse DELTA, not button state - they read zero on any frame where the mouse happens not
//! to move, which makes the state flicker on/off at frame rate and visibly strobes the cursor.
[BaseContainerProps()]
class GRAD_BC_HideCursorOnRotateCameraComponent : SCR_BaseManualCameraComponent
{
	//! EManualCameraFlag bit set while the camera is rotating (measured; see header).
	protected static const int ROTATE_BIT = 2;

	//! Cursor index restored when rotation ends. 0 is what vanilla SCR_CursorCustom restores to.
	protected static const int CURSOR_RESTORE = 0;

	[Attribute("1", UIWidgets.CheckBox, "Hide the cursor while rotating the camera")]
	protected bool m_bEnabled_HideCursor;


	//! Last index written, so the per-frame re-apply only calls SetCursor on an actual change.
	protected int m_iLastSetIndex = -999;

	//! Tracks whether we currently have the cursor hidden, so it can be restored on exit.
	protected bool m_bCursorHidden;

	//------------------------------------------------------------------------------------------------
	override bool EOnCameraInit()
	{
		m_iLastSetIndex = -999;
		m_bCursorHidden = false;

		// Return true so EOnCameraFrame is evaluated every frame.
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnCameraFrame(SCR_ManualCameraParam param)
	{
		if (!param || !m_bEnabled_HideCursor)
			return;

		bool rotating = (param.flag & ROTATE_BIT) != 0;

		// Re-applied EVERY frame while rotating rather than once on the transition:
		// SCR_CursorEditorUIComponent re-asserts its own cursor through UpdateCursor()/SetCursorType()
		// whenever editor state changes, so a one-shot SetCursor() gets overwritten.
		ApplyCursor(rotating);
	}

	//------------------------------------------------------------------------------------------------
	//! Restore the cursor if the camera goes away mid-rotation, so it is never left hidden.
	override void EOnCameraExit()
	{
		if (m_bCursorHidden)
			ApplyCursor(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyCursor(bool rotating)
	{
		int index = CURSOR_RESTORE;
		if (rotating)
			index = SCR_CursorCustom.REAL_CURSOR_HIDE;

		bool changed = (index != m_iLastSetIndex);

		// MEASURED: with a change-only guard the cursor intermittently reappeared mid-rotation, so
		// something else DOES write the cursor behind our back - SCR_CursorEditorUIComponent
		// .UpdateCursor() calls SetCursorType() whenever editor state changes (hover, state, mode,
		// map toggle, menu focus), and each of those silently undoes our hide while m_iLastSetIndex
		// still says 12.
		//
		// So while hiding we re-assert UNCONDITIONALLY every frame - the hide has to win the last
		// write of the frame. Same shape as the replay map panels, where SetVisible(false) was undone
		// by OnMenuUpdate's SetPosX every frame.
		//
		// The restore side keeps the guard: once we are back to CURSOR_RESTORE there is nothing to
		// defend, and letting the editor own its cursor again is exactly what we want.
		if (!changed && !rotating)
			return;

		WidgetManager.SetCursor(index);

		m_iLastSetIndex = index;
		m_bCursorHidden = rotating;

		// Logged only on an actual change - the per-frame path would otherwise flood the log
		// (the earlier probe produced 2060 lines in one short session).
		if (changed && GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_HideCursorOnRotate: cursor %1 (SetCursor(%2))",
				rotating, index), LogLevel.NORMAL);
	}
}
