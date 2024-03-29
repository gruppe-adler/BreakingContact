[ComponentEditorProps(category: "Gruppe Adler/Breaking Contact", description: "Attach to a character. Allows to define a character role that can later be accessed to distinguish between different roles")]
class GRAD_CharacterRoleComponentClass : ScriptComponentClass
{
}

class GRAD_CharacterRoleComponent : ScriptComponent
{
	[Attribute(defvalue: "No special Role", uiwidget: UIWidgets.EditBox, desc: "Character Role", category: "Breaking Contact")]
	protected string m_sCharacterRole;
	
	//------------------------------------------------------------------------------------------------
	void SetCharacterRole(string characterRole)
	{
		m_sCharacterRole = characterRole;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetCharacterRole()
	{
		return m_sCharacterRole;
	}
}
