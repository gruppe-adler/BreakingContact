[ComponentEditorProps(category: "Gruppe Adler/Breaking Contact", description: "Attach to a character. Handles stuff")]
class GRAD_PlayerComponentClass : ScriptComponentClass
{
}

class GRAD_PlayerComponent : ScriptComponent
{
	protected PlayerManager m_PlayerManager;
	
	protected PlayerManager GetPlayerManager()
	{
		if (m_PlayerManager == null)
			m_PlayerManager = GetGame().GetPlayerManager();
		
		return m_PlayerManager;
	}
	
	//------------------------------------------------------------------------------------------------
	void Ask_TeleportPlayer(vector location)
	{
		Rpc(RpcDo_Owner_TeleportPlayer, location);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_TeleportPlayer(vector location)
	{
		SCR_Global.TeleportLocalPlayer(location, SCR_EPlayerTeleportedReason.DEFAULT);
	}
}