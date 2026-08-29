//------------------------------------------------------------------------------------------------
//! Suppresses player/AI nametags while an admin is in the editor camera (ArmaVision / Game Master /
//! BC spectate), so recorded footage and screenshots are not covered in floating names.
//!
//! WHY HERE AND NOT IN THE PHOTO OVERLAY DEFAULTS:
//! GRAD_BC_PhotoOverlayDefaults sets EEditorMenuOverlayLayer.PLAYER to "none", which looks like it
//! should do this but cannot. An SCR_EditorMenuOverlay is ONE full-screen layout stretched
//! corner-to-corner in the editor MENU (see SCR_EditorMenuOverlay.CreateWidget: it sets AnchorMin
//! 0,0 / AnchorMax 1,1 on a single widget). It is the ArmaVision frame furniture. Nametags are a
//! completely separate system - SCR_NameTagDisplay, an SCR_InfoDisplayExtended on the player
//! controller's HUD manager, drawing one widget per tracked entity that tracks it in world space.
//! Setting an overlay layer to none can never remove them.
//!
//! WHY CanDisplayNameTag AND NOT DisplayUpdate/StopUpdate:
//! CanDisplayNameTag() is vanilla's own per-entity gate, called from InitializeTag() for every
//! entity except the locally controlled one. Returning false there is the same path the game's own
//! nametag filter settings use, so nothing else has to be understood or kept in sync.
//!
//! Two things this deliberately does NOT do:
//!  - It does not touch the LOCAL player's own tag. InitializeTag() returns early for the
//!    controlled entity BEFORE calling CanDisplayNameTag(), so that tag is out of reach here. In
//!    the editor camera the admin has no controlled character anyway.
//!  - It does not call StopUpdate() or CleanupAllTags(). Those are display-wide and would have to
//!    be manually undone on exit; letting the existing gate answer false keeps the normal
//!    lifecycle intact.
//!
//! WHICH MODES ARE SILENCED - and why not simply "the editor is open":
//! IsOpenedInstance() alone would also strip tags in EDIT/ADMIN, i.e. from a Game Master actively
//! running the mission, who needs to see who is who. The camera modes used for watching and
//! filming are the ones that want a clean frame, so the gate lists those explicitly:
//!   PHOTO / PHOTO_SAVE  - ArmaVision
//!   GRAD_BC_SPECTATE    - BC's return-to-spectator mode (see GRAD_BC_EEditorMode.c)
//! EEditorMode is a flags enum, so this is a bitmask test. Any mode not listed keeps its tags -
//! including EDIT/ADMIN (a Game Master running the mission needs to see who is who) and SPECTATE
//! (spectators watch to follow the action, so names stay on).
//!
//! REFRESH ON TOGGLE - why no extra hook is needed: InitializeTag() only runs once per entity, so
//! a gate alone would not clear tags that already exist when the editor opens. It does not have to.
//! Opening the editor moves the player onto the editor camera (SCR_EditorManagerEntity "keeps the
//! player on its camera while open" - the reason GRAD_PlayerComponent.Ask_EnterSpectator must close
//! it before possessing). That is a controlled-entity change, so vanilla fires
//! DisplayControlledEntityChanged -> RefreshTags(), which calls CleanupAllTags() and re-runs
//! ProcessFiltered() -> InitializeTag() for everything - back through this gate. Closing the editor
//! restores control and refreshes again, so tags come back on their own.
//!
//! If a build ever enters the editor WITHOUT a controlled-entity change, stale tags would linger
//! until the next refresh; the fix then is to call RefreshTags() on editor open, not to widen this
//! gate.
modded class SCR_NameTagDisplay
{
	//! Camera modes that should render a clean frame. Flags enum, combined as a mask.
	//!
	//! SPECTATE is deliberately NOT here: spectators watch the match to follow who is doing what, so
	//! they keep their nametags. Only the filming/photo cameras are stripped.
	protected static const EEditorMode BC_TAGLESS_MODES =
		EEditorMode.PHOTO | EEditorMode.PHOTO_SAVE | EEditorMode.GRAD_BC_SPECTATE;

	//------------------------------------------------------------------------------------------------
	//! Reject nametags while the local player is in one of the viewing/filming editor cameras.
	//!
	//! GetInstance() + GetCurrentMode() is the same pair GRAD_BC_SpectateModeTrigger uses, so the
	//! call shape is already proven in this codebase. The manager is per-player and may not exist
	//! yet, hence the null check - a missing manager means no editor, so tags stay.
	override protected bool CanDisplayNameTag(notnull IEntity entity)
	{
		if (BC_IsInTaglessCamera())
			return false;

		return super.CanDisplayNameTag(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected bool BC_IsInTaglessCamera()
	{
		if (!SCR_EditorManagerEntity.IsOpenedInstance())
			return false;

		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (!editorManager)
			return false;

		return (editorManager.GetCurrentMode() & BC_TAGLESS_MODES) != 0;
	}
}
