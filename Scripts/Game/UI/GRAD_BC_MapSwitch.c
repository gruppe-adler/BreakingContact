// Static registry of the Breaking Contact maps offered by the in-game map switcher,
// plus the authoritative (server-side) permission check used to validate a switch request.
//
// The map list is shared by client and server so the RPC only has to transmit an INDEX
// into this table. A client can therefore never inject an arbitrary ResourceName - the
// bounds check on the index is the whole validation.

class GRAD_BC_MapEntry
{
	string m_sDisplayName;
	ResourceName m_sMission;
	ResourceName m_sWorld;

	void GRAD_BC_MapEntry(string displayName, ResourceName mission, ResourceName world)
	{
		m_sDisplayName = displayName;
		m_sMission = mission;
		m_sWorld = world;
	}
}

class GRAD_BC_MapSwitch
{
	// The five BC maps. Mission .conf is passed to the transition - NOT the .ent world -
	// because the header carries the GRAD_BC_MissionHeader settings (debug logs,
	// faction-elimination skip, traffic overrides). Passing the world would skip them.
	// The world resource is stored only to identify which map is currently running.
	protected static ref array<ref GRAD_BC_MapEntry> s_aMaps = {
		new GRAD_BC_MapEntry("Anizay",    "{46CD90C57D75FBA0}Missions/BreakingContact_Anizay.conf",    "{6B6A9EB28DB993AD}Worlds/MP/BC_anizay.ent"),
		new GRAD_BC_MapEntry("Bystrany",  "{D7F769B5784CC684}Missions/BreakingContact_Bystrany.conf",  "{225FFD333E93ED0E}Worlds/MP/BC_bystrany.ent"),
		new GRAD_BC_MapEntry("Everon",    "{1D68D4BCCA7512AF}Missions/BreakingContact_Everon.conf",    "{2BB7B6635841693E}Worlds/MP/BC_everon.ent"),
		new GRAD_BC_MapEntry("Kolgujev",  "{D77FE3831F3534A3}Missions/BreakingContact_Kolgujev.conf",  "{045FDDED6588C176}Worlds/MP/BC_kolgujev.ent"),
		new GRAD_BC_MapEntry("Mogadishu", "{D540A4B60A970238}Missions/BreakingContact_Mogadishu.conf", "{3E1397CE897070BB}Worlds/MP/BC_mogadishu.ent")
	};

	//------------------------------------------------------------------------------------------------
	static int GetMapCount()
	{
		return s_aMaps.Count();
	}

	//------------------------------------------------------------------------------------------------
	static string GetDisplayName(int index)
	{
		if (index < 0 || index >= s_aMaps.Count())
			return string.Empty;

		return s_aMaps[index].m_sDisplayName;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName GetMission(int index)
	{
		if (index < 0 || index >= s_aMaps.Count())
			return ResourceName.Empty;

		return s_aMaps[index].m_sMission;
	}

	//------------------------------------------------------------------------------------------------
	//! Index of the currently running mission, or -1 if it isn't one of ours.
	//! Used to mark the active map in the dropdown.
	//!
	//! Matches on the world resource rather than the display name: GetGame().GetMissionHeader()
	//! returns the base MissionHeader (m_sName lives on SCR_MissionHeader), and GetWorldResourceName()
	//! is an exact GUID comparison instead of a substring guess.
	static int GetCurrentMapIndex()
	{
		ResourceName currentWorld = ResourceName.Empty;

		MissionHeader header = GetGame().GetMissionHeader();
		if (header)
			currentWorld = header.GetWorldResourceName();

		if (!currentWorld.IsEmpty())
		{
			for (int i = 0; i < s_aMaps.Count(); i++)
			{
				if (s_aMaps[i].m_sWorld == currentWorld)
					return i;
			}

			Print(string.Format("BC Debug - MapSwitch: world '%1' matches none of the %2 known maps", currentWorld, s_aMaps.Count()), LogLevel.WARNING);
			return -1;
		}

		// No usable mission header. This is the NORMAL case in Workbench Play sessions -
		// GetMissionHeader() stays null for the whole session, not just briefly at startup
		// (BC's own traffic/vehicle managers hit the same null and fall back to defaults).
		//
		// The loaded world is still known regardless of how the session was started, so identify
		// the map from the world file instead. Matching on the bare file name because
		// GetWorldFile() has no {GUID} prefix, unlike the ResourceName entries in s_aMaps.
		string worldFile = GetGame().GetWorldFile();
		if (worldFile.IsEmpty())
		{
			Print("BC Debug - MapSwitch: no mission header and no world file, cannot identify current map", LogLevel.WARNING);
			return -1;
		}

		for (int j = 0; j < s_aMaps.Count(); j++)
		{
			string worldFileName = GetWorldFileName(j);
			if (!worldFileName.IsEmpty() && worldFile.Contains(worldFileName))
				return j;
		}

		Print(string.Format("BC Debug - MapSwitch: world file '%1' matches none of the %2 known maps", worldFile, s_aMaps.Count()), LogLevel.WARNING);
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Bare world file name for entry i, e.g. "BC_kolgujev.ent". s_aMaps stores full ResourceNames
	//! with a {GUID} prefix; GetWorldFile() returns a plain path, so matching happens on this.
	protected static string GetWorldFileName(int index)
	{
		if (index < 0 || index >= s_aMaps.Count())
			return string.Empty;

		string world = s_aMaps[index].m_sWorld;

		int slash = world.LastIndexOf("/");
		if (slash < 0)
			return world;

		return world.Substring(slash + 1, world.Length() - slash - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! AUTHORITATIVE permission check - safe to call on the server to validate an RPC.
	//!
	//! Deliberately NOT SCR_Global.IsAdmin() / COA_HasUnlimitedEditorAccess(): those read
	//! LOCAL player state and are meaningless on a dedicated server. GetPlayerRoles() is
	//! the engine's server-side role lookup, and COA_PermissionManager's moderator list is
	//! an [RplProp()] array so it is valid server-side too.
	static bool IsPlayerAdminServer(int playerId)
	{
		if (playerId <= 0)
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		EPlayerRole roles = playerManager.GetPlayerRoles(playerId);
		if ((roles & EPlayerRole.ADMINISTRATOR) != 0)
			return true;

		COA_PermissionManager permissionManager = COA_PermissionManager.GetInstance();
		if (permissionManager && permissionManager.IsModerator(playerId))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT-SIDE visibility check only - never trust this for authorization.
	//! Reuses COA's own combined spectator/moderator/admin helper so BC inherits their rules.
	static bool CanLocalPlayerSwitchMap()
	{
		return SCR_EditorManagerEntity.COA_HasUnlimitedEditorAccess();
	}
}
