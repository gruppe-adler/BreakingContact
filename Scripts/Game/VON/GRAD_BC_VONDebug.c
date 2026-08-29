// Read-only diagnostic for spectator voice.
//
// GOAL: spectators should hear each other regardless of distance, without push-to-talk.
//
// Vanilla routes voice by ECommMethod. DIRECT is position-attenuated; SQUAD_RADIO is not, so a
// radio entry is the way to get location-independent speech. COALITION's spectator character
// carries SpecRadioBag.et (encryption key "SPEC", turned on, transceiver range 50000m), which is
// exactly such a channel - IF it surfaces as a SCR_VONEntryRadio in the controller's entry list.
//
// SCR_VONController.ActivateVON gates radio transmission on the encryption key matching the LOCAL
// PLAYER'S FACTION key, otherwise it force-falls back to proximity:
//
//     if (radio.GetEncryptionKey() != m_sLocalEncryptionKey) { SetVONProximity(true); return false; }
//
// So this logs the two things that decide the outcome: the local faction key, and what entries
// actually exist. Interpretation:
//
//   entry count 0                  -> the SpecRadioBag is not producing a VON entry at all; the
//                                     entry has to be added manually via AddEntry()
//   radio entry present, faction   -> should already work; the player is just transmitting on
//   key "SPEC"                        DIRECT and needs the radio entry selected instead
//   radio entry present, faction   -> the encryption-key gate is rejecting it, and the faction
//   key NOT "SPEC"                    assignment is the real problem
//
// This class only ADDS a method - it overrides nothing, so it cannot change existing VON behaviour.
// Remove once the behaviour is understood.
modded class SCR_VONController
{
	protected bool m_bGRAD_BC_DebugListenerRegistered = false;

	//------------------------------------------------------------------------------------------------
	//! Vanilla Init() registers all the VON action listeners and sets s_bIsInit; hook the same
	//! point so our diagnostic key is bound for exactly as long as VON control itself is.
	override protected void Init(IEntity owner)
	{
		super.Init(owner);

		// Init() early-returns on dedicated servers and on secondary controllers of a hosted
		// server, in which case m_InputManager is not set and there is nothing to bind to.
		if (!m_InputManager || m_bGRAD_BC_DebugListenerRegistered)
			return;

		m_InputManager.AddActionListener("GRAD_BC_VONDebug", EActionTrigger.DOWN, GRAD_BC_OnDebugKey);
		m_bGRAD_BC_DebugListenerRegistered = true;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Cleanup()
	{
		if (m_bGRAD_BC_DebugListenerRegistered && m_InputManager)
		{
			m_InputManager.RemoveActionListener("GRAD_BC_VONDebug", EActionTrigger.DOWN, GRAD_BC_OnDebugKey);
			m_bGRAD_BC_DebugListenerRegistered = false;
		}

		super.Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	protected void GRAD_BC_OnDebugKey()
	{
		GRAD_BC_DumpVONState();
	}

	//------------------------------------------------------------------------------------------------
	//! Bound to F9 (GRAD_BC_VONDebug). Press while spectating.
	void GRAD_BC_DumpVONState()
	{
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();

		string factionKey = "<none>";
		string radioEncryptionKey = "<none>";

		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (factionManager)
		{
			SCR_Faction faction = SCR_Faction.Cast(factionManager.GetLocalPlayerFaction());
			if (faction)
			{
				factionKey = faction.GetFactionKey();
				radioEncryptionKey = faction.GetFactionRadioEncryptionKey();
			}
		}

		Print(string.Format("BC VONDebug: playerId=%1 factionKey=%2 factionRadioKey=%3 VONDisabled=%4",
			localPlayerId, factionKey, radioEncryptionKey, IsVONDisabled()), LogLevel.NORMAL);

		Print(string.Format("BC VONDebug: entry count=%1 LRRAvailable=%2 crossFactionAllowed=%3",
			GetVONEntryCount(), IsLRRAvailable(), GetGame().GetVONCanTransmitCrossFaction()), LogLevel.NORMAL);

		array<ref SCR_VONEntry> entries = {};
		GetVONEntries(entries);

		foreach (SCR_VONEntry entry : entries)
		{
			if (!entry)
				continue;

			SCR_VONEntryRadio radioEntry = SCR_VONEntryRadio.Cast(entry);
			if (!radioEntry)
			{
				Print(string.Format("BC VONDebug:   entry method=%1 (not a radio entry) usable=%2",
					entry.GetVONMethod(), entry.IsUsable()), LogLevel.NORMAL);
				continue;
			}

			string entryKey = "<no radio>";
			BaseTransceiver transceiver = radioEntry.GetTransceiver();
			if (transceiver)
			{
				BaseRadioComponent radio = transceiver.GetRadio();
				if (radio)
					entryKey = radio.GetEncryptionKey();
			}

			Print(string.Format("BC VONDebug:   RADIO entry freq=%1 encryptionKey=%2 longRange=%3 muted=%4 usable=%5",
				radioEntry.GetEntryFrequency(), entryKey, radioEntry.IsLongRange(),
				radioEntry.GetIsMuted(), radioEntry.IsUsable()), LogLevel.NORMAL);
		}

		SCR_VONEntry active = GetActiveEntry();
		if (active)
			Print(string.Format("BC VONDebug: ACTIVE entry method=%1 isRadio=%2",
				active.GetVONMethod(), SCR_VONEntryRadio.Cast(active) != null), LogLevel.NORMAL);
		else
			Print("BC VONDebug: NO active entry (direct speech only)", LogLevel.NORMAL);
	}
}
