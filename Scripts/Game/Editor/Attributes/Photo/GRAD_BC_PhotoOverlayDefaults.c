modded class SCR_MenuOverlaysEditorComponent : SCR_BaseEditorComponent
{
	override void EOnEditorPostActivate()
	{
		super.EOnEditorPostActivate();

		SetLayerToNone(EEditorMenuOverlayLayer.VIGNETTE);
		SetLayerToNone(EEditorMenuOverlayLayer.HELPER);
		SetLayerToNone(EEditorMenuOverlayLayer.LOGO);
		SetLayerToNone(EEditorMenuOverlayLayer.LOGO_FRAME);

		// NOTE: the PLAYER layer here is the ArmaVision player-info OVERLAY FRAME, not the floating
		// nametags above characters. An SCR_EditorMenuOverlay is a single full-screen layout in the
		// editor menu (CreateWidget anchors it 0,0 -> 1,1), so it can never be a per-character label.
		// Nametags are SCR_NameTagDisplay, suppressed in
		// Scripts/Game/Modded/GRAD_BC_M_SCR_NameTagDisplay.c.
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
