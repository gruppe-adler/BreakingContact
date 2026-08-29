//------------------------------------------------------------------------------------------------
//! Suppresses player/AI nametags while a BC "clean frame" camera is active (ArmaVision / photo),
//! so recorded footage and screenshots are not covered in floating names.
//!
//! NOT THE ONLY MECHANISM - and probably not the primary one.
//! ArmaVision's V-key overlay panel has a "Player character effects" row whose value is "Name tags";
//! that is the EEditorMenuOverlayLayer.PLAYER layer, and setting it to "None" is the game's own way
//! of turning these off. GRAD_BC_CleanFrameCameraComponent does exactly that. This gate is a second,
//! independent belt-and-braces path that also covers contexts where no overlay component exists.
//! If the overlay route proves sufficient in testing, this file can be deleted.
//!
//! THE CONDITION LIVES ON THE CAMERA, NOT HERE
//! This gate only asks GRAD_BC_CleanFrameCameraComponent whether a clean-frame camera is up. That
//! component sets its flag from its own EOnCameraInit/EOnCameraExit lifecycle, which is the only
//! signal proven to fire in ArmaVision - BC's cursor-hide component sits on the same camera prefab
//! and its hide works there (confirmed in game).
//!
//! An earlier version of this file gated on SCR_EditorManagerEntity.IsOpenedInstance() plus an
//! EEditorMode mask. It never fired: ArmaVision is not an editor mode, so IsOpenedInstance() is
//! false and the gate returned early every time. Do not reintroduce an editor-state check here - if
//! the set of cameras that want a clean frame changes, add or remove
//! GRAD_BC_CleanFrameCameraComponent on those camera prefabs instead.
//!
//! WHY CanDisplayNameTag AND NOT DisplayUpdate/StopUpdate
//! CanDisplayNameTag() is vanilla's own per-entity gate, called from InitializeTag() for every
//! entity except the locally controlled one. Returning false there is the same path the game's own
//! nametag filter settings use, so nothing else has to be understood or kept in sync. StopUpdate()
//! and CleanupAllTags() are display-wide and would have to be manually undone on exit; letting the
//! existing gate answer false keeps the normal lifecycle intact.
//!
//! THE LOCAL PLAYER'S OWN TAG IS OUT OF REACH HERE
//! InitializeTag() returns early for the controlled entity BEFORE calling CanDisplayNameTag(), so
//! this cannot suppress it. Not a problem in practice: on a camera the player has no controlled
//! character to tag.
//!
//! EXISTING TAGS ARE REBUILT BY THE CAMERA COMPONENT
//! CanDisplayNameTag() is consulted only when a tag is first created, so a gate alone would not
//! clear tags that already existed when the camera appeared. GRAD_BC_CleanFrameCameraComponent
//! triggers the display's own refresh on init and on exit, which clears every tag and re-runs
//! InitializeTag - back through this gate.
modded class SCR_NameTagDisplay
{
	//------------------------------------------------------------------------------------------------
	override protected bool CanDisplayNameTag(notnull IEntity entity)
	{
		if (GRAD_BC_CleanFrameCameraComponent.IsCleanFrameActive())
			return false;

		return super.CanDisplayNameTag(entity);
	}
}
