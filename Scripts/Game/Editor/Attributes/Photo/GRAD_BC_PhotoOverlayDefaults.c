modded class SCR_MenuOverlaysEditorComponent : SCR_BaseEditorComponent
{
	override void EOnEditorPostActivate()
	{
		super.EOnEditorPostActivate();

		SetLayerToNone(EEditorMenuOverlayLayer.VIGNETTE);
		SetLayerToNone(EEditorMenuOverlayLayer.HELPER);
		SetLayerToNone(EEditorMenuOverlayLayer.LOGO);
		SetLayerToNone(EEditorMenuOverlayLayer.LOGO_FRAME);
	}

	protected void SetLayerToNone(EEditorMenuOverlayLayer layerType)
	{
		SCR_EditorMenuOverlayLayer layer = GetOverlayLayer(layerType);
		if (layer)
			layer.SetCurrentOverlay(0);
	}
}
