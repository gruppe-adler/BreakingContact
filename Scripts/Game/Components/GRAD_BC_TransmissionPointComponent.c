[ComponentEditorProps(category: "GRAD/Breaking Contact", description: "")]
class GRAD_BC_TransmissionPointComponentClass : ScriptComponentClass
{
}

class GRAD_BC_TransmissionPointComponent : ScriptComponent
{
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.EditBox, desc: "MinDistance", params: "", category: "Breaking Contact - Transmission Point")];
	protected int m_TransmissionPointMinDistance;
}