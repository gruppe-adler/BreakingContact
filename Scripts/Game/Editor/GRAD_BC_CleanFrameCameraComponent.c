//------------------------------------------------------------------------------------------------
//! Gives the ArmaVision / photo camera a clean frame: no floating nametags, no menu overlays
//! (logo, vignette, helper, player frame). Restores both when the camera goes away.
//!
//! ATTACH: add to the SCR_ManualCamera prefab's m_aComponents array in Workbench - the same array
//! that holds GRAD_BC_HideCursorOnRotateCameraComponent. It is a BaseContainerProps class, so it
//! appears when adding an element to that array, NOT in the entity "Add Component" menu.
//!
//! WHY A CAMERA COMPONENT AND NOT AN EDITOR HOOK
//! Two earlier attempts failed because they keyed on the EDITOR:
//!
//!   1. GRAD_BC_PhotoOverlayDefaults (modded SCR_MenuOverlaysEditorComponent.EOnEditorPostActivate)
//!      - an editor component only receives editor events. ArmaVision is not an editor mode, so it
//!        never ran there. It also loses to the attribute write-back described below.
//!   2. A modded SCR_NameTagDisplay gated on SCR_EditorManagerEntity.IsOpenedInstance()
//!      - returns false when no editor is open, so the gate never fired in ArmaVision at all.
//!
//! What IS proven to run on this camera is a manual-camera component: BC's own
//! GRAD_BC_HideCursorOnRotateCameraComponent sits on this prefab and its cursor hide works in
//! ArmaVision (confirmed in game). So the reliable signal for "a BC camera is live" is this
//! component's own lifecycle - EOnCameraInit / EOnCameraExit - not any editor state.
//!
//! THE STATIC FLAG
//! s_bCleanFrameActive is what GRAD_BC_M_SCR_NameTagDisplay reads. It is static because the
//! nametag display is a separate object (an SCR_InfoDisplayExtended on the player controller's HUD
//! manager) with no reference to this camera. Only one manual camera is active at a time, so a
//! single static is sufficient; the counter guards against a second camera existing briefly during
//! a switch and clearing the flag while the first is still up.
//!
//! WHAT THE OVERLAY LAYERS ACTUALLY ARE
//! These are the rows in ArmaVision's V-key "Editing: Scenario properties -> Overlays" panel:
//!     Frame                   -> LOGO_FRAME
//!     Logo                    -> LOGO
//!     Player character effects-> PLAYER      <-- THIS IS THE NAMETAG CONTROL ("Name tags")
//!     Vignette                -> VIGNETTE
//! Earlier comments in this file and in GRAD_BC_PhotoOverlayDefaults.c claimed the PLAYER layer
//! could not affect nametags because an SCR_EditorMenuOverlay is a single full-screen widget. That
//! was WRONG - the panel proves PLAYER drives the nametag display. The modded SCR_NameTagDisplay
//! gate is therefore a second, independent mechanism for the same goal, not the only one.
//!
//! INDEX 0 IS NOT "NONE" ON EVERY LAYER
//! This code used to call SetCurrentOverlay(0, true), assuming index 0 meant "none". MEASURED: on
//! the LOGO layer that made the watermark change from the Bohemia logo to the ArmaVision logo
//! instead of disappearing - so index 0 there is simply a DIFFERENT logo. Layers that already read
//! "None" (Frame/Vignette) are the ones where 0 happens to be the empty entry, which is why the
//! same call looked correct on them.
//!
//! So the empty entry is resolved per layer BY NAME via SCR_EditorMenuOverlay.GetDisplayName(),
//! and cached. If no entry matches, the layer is skipped and the real names are logged once - it
//! deliberately does NOT fall back to 0, because a wrong logo is worse than an unchanged one.
//!
//! OVERLAYS - why the clear is re-asserted every frame
//! SCR_MenuOverlayEditorAttribute.WriteVariable() calls layer.SetCurrentOverlay(var.GetInt())
//! WITHOUT the forced flag, pushing the stored index back over a one-shot clear.
//! SCR_BaseEditorAttribute.IsSerializable() is true, so that value also persists in session saves.
//! A single write on init therefore loses. Re-applying each frame is the same shape BC already uses
//! for the cursor here and for the replay map panels - the clear has to win the last write of the
//! frame. (Pressing R / "Revert changes" in the panel is the direct test of this.)
//!
//! SetCurrentOverlay(index, true) MUST pass forced:true: the method opens with
//!     if (!m_aOverlays || (index == m_iCurrentOverlay && !forced)) return;
//! so a layer already CONFIGURED to that index is a silent no-op while its widget is on screen.
[BaseContainerProps()]
class GRAD_BC_CleanFrameCameraComponent : SCR_BaseManualCameraComponent
{
	//! True while a camera carrying this component is alive. Read by the modded SCR_NameTagDisplay.
	protected static bool s_bCleanFrameActive;

	//! Number of live cameras with this component, so overlapping cameras cannot clear the flag early.
	protected static int s_iActiveCount;

	[Attribute("1", UIWidgets.CheckBox, "Hide floating nametags while this camera is active")]
	protected bool m_bHideNameTags;

	[Attribute("1", UIWidgets.CheckBox, "Hide the editor menu overlays (logo, vignette, helper, player frame)")]
	protected bool m_bHideOverlays;

	//! Overlay layers cleared while this camera is active.
	//! NOTE: the panel also shows a "Grids" row, but no GRIDS member is referenced anywhere in this
	//! project and its enum name could not be verified - adding an unverified member would break the
	//! compile. Grids already reads "None", so nothing is lost by omitting it.
	protected static const ref array<EEditorMenuOverlayLayer> BC_OVERLAY_LAYERS = {
		EEditorMenuOverlayLayer.LOGO,
		EEditorMenuOverlayLayer.LOGO_FRAME,
		EEditorMenuOverlayLayer.VIGNETTE,
		EEditorMenuOverlayLayer.HELPER,
		EEditorMenuOverlayLayer.PLAYER
	};

	//! Substrings that identify the "empty" overlay entry. Matched case-insensitively against
	//! GetDisplayName(), which may be a rendered literal ("None") or a localisation key
	//! ("#AR-Editor_Overlay_None"), so a substring test covers both.
	protected static const ref array<string> BC_NONE_TOKENS = { "none", "empty", "off" };

	//! Resolved "none" index per layer. Populated on first use so the name scan runs once, not
	//! every frame. -1 means "scanned and not found" - the layer is then skipped for good.
	protected ref map<EEditorMenuOverlayLayer, int> m_mNoneIndex = new map<EEditorMenuOverlayLayer, int>();

	//------------------------------------------------------------------------------------------------
	//! True while a clean-frame camera is live. Static so the nametag display can ask without
	//! holding a reference to the camera.
	static bool IsCleanFrameActive()
	{
		return s_bCleanFrameActive;
	}

	//------------------------------------------------------------------------------------------------
	override bool EOnCameraInit()
	{
		s_iActiveCount++;

		if (m_bHideNameTags)
		{
			s_bCleanFrameActive = true;

			// Tags for entities that already exist were built before this camera appeared, and
			// CanDisplayNameTag is only consulted in InitializeTag - so rebuild them once here to
			// put every existing tag back through the gate.
			BC_RefreshNameTags();
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CleanFrame: camera init (active=%1, tags=%2, overlays=%3)",
				s_iActiveCount, m_bHideNameTags, m_bHideOverlays), LogLevel.NORMAL);

		// Return true so EOnCameraFrame runs every frame - required for the overlay re-assert.
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnCameraFrame(SCR_ManualCameraParam param)
	{
		if (m_bHideOverlays)
			BC_ClearOverlays();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnCameraExit()
	{
		s_iActiveCount--;
		if (s_iActiveCount < 0)
			s_iActiveCount = 0;

		if (s_iActiveCount == 0 && s_bCleanFrameActive)
		{
			s_bCleanFrameActive = false;

			// Bring the tags back for whoever the player becomes next.
			BC_RefreshNameTags();
		}

		if (GRAD_BC_BreakingContactManager.IsDebugMode())
			Print(string.Format("GRAD_BC_CleanFrame: camera exit (active=%1)", s_iActiveCount),
				LogLevel.NORMAL);

		// Overlays are deliberately NOT restored here. They are owned by the editor attribute
		// system, which rewrites them from its own stored value; forcing an index back would fight
		// it and could leave a layer showing something the user never chose.
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuild every nametag so existing ones are re-evaluated against the gate.
	//! RefreshTags() is protected on SCR_NameTagDisplay, so this goes through the public
	//! DisplayControlledEntityChanged path, which vanilla itself uses to trigger a refresh.
	protected void BC_RefreshNameTags()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		// Called inline, matching how BC reaches info displays elsewhere
		// (GRAD_PlayerComponent.Rpc_ShowBCLogo_Local, GRAD_BC_BreakingContactManager) - avoids
		// naming the manager's concrete type.
		if (!playerController.GetHUDManagerComponent())
			return;

		array<BaseInfoDisplay> infoDisplays = {};
		playerController.GetHUDManagerComponent().GetInfoDisplays(infoDisplays);

		IEntity controlled = playerController.GetControlledEntity();

		foreach (BaseInfoDisplay baseDisplay : infoDisplays)
		{
			SCR_NameTagDisplay nameTagDisplay = SCR_NameTagDisplay.Cast(baseDisplay);
			if (!nameTagDisplay)
				continue;

			// Vanilla's own refresh entry point: it clears every tag and re-runs InitializeTag,
			// which is where CanDisplayNameTag is consulted.
			nameTagDisplay.DisplayControlledEntityChanged(controlled, controlled);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Force every listed overlay layer onto its "none" entry, resolved by name per layer.
	//! Silently does nothing when no editor overlay component exists - in that case there is no
	//! overlay to clear and the modded SCR_NameTagDisplay gate still covers the nametags.
	protected void BC_ClearOverlays()
	{
		SCR_MenuOverlaysEditorComponent overlays = SCR_MenuOverlaysEditorComponent.Cast(
			SCR_MenuOverlaysEditorComponent.GetInstance(SCR_MenuOverlaysEditorComponent));
		if (!overlays)
			return;

		foreach (EEditorMenuOverlayLayer layerType : BC_OVERLAY_LAYERS)
		{
			SCR_EditorMenuOverlayLayer layer = overlays.GetOverlayLayer(layerType);
			if (!layer)
				continue;

			int noneIndex = BC_ResolveNoneIndex(layerType, layer);

			// No empty entry on this layer - already logged once by the resolver. Deliberately NOT
			// falling back to 0: that is what turned the Bohemia logo into the ArmaVision logo.
			if (noneIndex < 0)
				continue;

			// Already on the empty entry - the common case, so the per-frame re-assert is free.
			if (layer.GetCurrentOverlayIndex() == noneIndex)
				continue;

			layer.SetCurrentOverlay(noneIndex, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Index of the "empty" overlay on a layer, or -1 when the layer has none.
	//! Scans display names once per layer and caches the answer, including the negative result.
	protected int BC_ResolveNoneIndex(EEditorMenuOverlayLayer layerType, notnull SCR_EditorMenuOverlayLayer layer)
	{
		// Contains/Get/Set rather than Find(out) - the pattern already used by
		// GRAD_BC_BreakingContactManager.m_mGroupNameCounters.
		if (m_mNoneIndex.Contains(layerType))
			return m_mNoneIndex.Get(layerType);

		// A layer with no overlays configured would make SetCurrentOverlay log an
		// "index out of bounds" ERROR, so treat it as unresolvable.
		array<SCR_EditorMenuOverlay> configured = {};
		int count = layer.GetOverlays(configured);
		if (count < 1)
		{
			m_mNoneIndex.Set(layerType, -1);
			return -1;
		}

		string names;

		for (int i = 0; i < count; i++)
		{
			string displayName = configured[i].GetDisplayName();

			if (!names.IsEmpty())
				names = names + ", ";
			names = names + string.Format("[%1]'%2'", i, displayName);

			string lowered = displayName;
			lowered.ToLower();

			foreach (string token : BC_NONE_TOKENS)
			{
				if (lowered.Contains(token))
				{
					m_mNoneIndex.Set(layerType, i);

					if (GRAD_BC_BreakingContactManager.IsDebugMode())
						Print(string.Format("GRAD_BC_CleanFrame: layer %1 -> none index %2 ('%3')",
							typename.EnumToString(EEditorMenuOverlayLayer, layerType), i, displayName),
							LogLevel.NORMAL);

					return i;
				}
			}
		}

		// Logged once per layer (the cache below stops it repeating). The names are printed so the
		// token list above can be extended without another guessing round.
		Print(string.Format("GRAD_BC_CleanFrame: no 'none' entry on layer %1 - leaving it alone. Entries: %2",
			typename.EnumToString(EEditorMenuOverlayLayer, layerType), names), LogLevel.WARNING);

		m_mNoneIndex.Set(layerType, -1);
		return -1;
	}
}
