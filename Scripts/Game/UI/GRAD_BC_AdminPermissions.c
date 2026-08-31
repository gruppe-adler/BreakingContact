// Admin permission checks used to gate BC's admin-only actions.
//
// Was GRAD_BC_MapSwitch, which also carried the in-game map switcher. The switcher was removed
// when the BC maps were split into standalone addons: its map table hardcoded mission .conf
// GUIDs that no longer resolve from this project. The permission helpers stayed behind because
// the admin spectator switch and the preview-menu buttons depend on them.

class GRAD_BC_AdminPermissions
{
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
	static bool HasLocalAdminAccess()
	{
		return SCR_EditorManagerEntity.COA_HasUnlimitedEditorAccess();
	}
}
