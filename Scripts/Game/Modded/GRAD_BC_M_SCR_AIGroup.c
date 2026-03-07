modded class SCR_AIGroup
{
	override void EOnInit(IEntity owner)
	{
		// PSCore's PS_M_SCR_AIGroup.EOnInit (line 22) crashes because Event_OnInit
		// is null when m_bSetPlayable is true. Initialize it early so PSCore's
		// Insert() call doesn't null-deref.
		if (!Event_OnInit)
			Event_OnInit = new ScriptInvoker();

		super.EOnInit(owner);
	}
}
