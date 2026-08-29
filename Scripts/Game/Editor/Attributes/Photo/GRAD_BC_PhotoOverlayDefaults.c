//------------------------------------------------------------------------------------------------
//! Clears the editor menu overlays (logo, vignette, helper, player frame) when an EDITOR mode
//! activates - Game Master and BC spectate.
//!
//! SCOPE - this does NOT cover ArmaVision. EOnEditorPostActivate is an editor-component event, and
//! ArmaVision is not an editor mode, so nothing here runs while that camera is up. The overlay
//! clear for ArmaVision lives in GRAD_BC_CleanFrameCameraComponent, which hangs off the camera
//! prefab instead and re-asserts every frame.
//!
//! It also only wins once. SCR_MenuOverlayEditorAttribute.WriteVariable() calls
//! layer.SetCurrentOverlay(var.GetInt()) WITHOUT forced, pushing the prefab-authored index back
//! over this clear whenever the attribute system writes - and SCR_BaseEditorAttribute
//! .IsSerializable() is true, so that value persists in session saves. If the logo returns in an
//! editor mode, that write-back is why, and the fix is the same per-frame re-assert the camera
//! component uses.
modded class SCR_MenuOverlaysEditorComponent : SCR_BaseEditorComponent
{
	override void EOnEditorPostActivate()
	{
		super.EOnEditorPostActivate();

		SetLayerToNone(EEditorMenuOverlayLayer.VIGNETTE);
		SetLayerToNone(EEditorMenuOverlayLayer.HELPER);
		SetLayerToNone(EEditorMenuOverlayLayer.LOGO);
		SetLayerToNone(EEditorMenuOverlayLayer.LOGO_FRAME);

		// The PLAYER layer IS the nametag control - it appears in ArmaVision's V-key overlay panel as
		// "Player character effects" with the value "Name tags". An earlier comment here claimed the
		// opposite (that a full-screen overlay widget could never be a per-character label); that was
		// wrong. Scripts/Game/Modded/GRAD_BC_M_SCR_NameTagDisplay.c is a second, independent gate on
		// the same behaviour.
		//
		// WARNING: index 0 is NOT "none" on every layer - on LOGO it is a different logo. This call
		// is only correct for layers whose empty entry happens to be first. The camera-side clear in
		// GRAD_BC_CleanFrameCameraComponent resolves the right index by name instead; prefer that
		// approach if this editor-mode path ever needs to actually work.
		SetLayerToNone(EEditorMenuOverlayLayer.PLAYER);
	}

	//------------------------------------------------------------------------------------------------
	//! Force overlay index 0 ("none") on a layer.
	//!
	//! MUST pass forced:true. SetCurrentOverlay() opens with
	//!     if (!m_aOverlays || (index == m_iCurrentOverlay && !forced)) return;
	//! so when a layer is already CONFIGURED to index 0 the call is a silent no-op - and the widget
	//! has already been created by super.EOnEditorPostActivate() -> PostActivateLayer(), which itself
	//! ends with SetCurrentOverlay(m_iCurrentOverlay, true). That is why the LOGO layer kept showing
	//! despite being listed here.
	//!
	//! Forcing re-runs the body, which does DeleteWidget() on the current overlay before creating
	//! index 0, so the visible overlay is actually removed.
	protected void SetLayerToNone(EEditorMenuOverlayLayer layerType)
	{
		SCR_EditorMenuOverlayLayer layer = GetOverlayLayer(layerType);
		if (!layer)
			return;

		// A layer with no overlays configured would make SetCurrentOverlay(0) log an
		// "index out of bounds" ERROR, so skip it.
		array<SCR_EditorMenuOverlay> overlays = {};
		if (layer.GetOverlays(overlays) < 1)
			return;

		layer.SetCurrentOverlay(0, true);

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_PhotoOverlayDefaults: layer %1 -> overlay 0 (of %2)",
				typename.EnumToString(EEditorMenuOverlayLayer, layerType), overlays.Count()),
				LogLevel.NORMAL);
	}
}
