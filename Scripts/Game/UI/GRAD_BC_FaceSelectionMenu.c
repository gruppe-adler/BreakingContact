// -------------------------------------------------------
//! A single button entry in the face-selection list.
//! Stores which face index it belongs to and calls back
//! to the menu when clicked.
// -------------------------------------------------------
class GRAD_BC_FaceButtonHandler
{
protected GRAD_BC_FaceSelectionMenu m_Menu;
protected int m_iIndex;

void GRAD_BC_FaceButtonHandler(GRAD_BC_FaceSelectionMenu menu, int index)
{
m_Menu   = menu;
m_iIndex = index;
}

void OnClick()
{
m_Menu.SelectFaceByIndex(m_iIndex);
}
}

// -------------------------------------------------------
//! Face selection overlay shown from the lobby/spectator menu.
//!
//! Behaviour
//! - Opens as a full-screen dim overlay with a centred panel.
//! - Left column: scrollable list of face buttons.
//! - Right column: display name of the currently selected face.
//! - Selection is persisted to the local profile via LocalProfileValues
//!   so it survives game restarts.
//! - The selected face ID is made available via GetSelectedFaceId()
//!   so it can be applied to the spawned character.
// -------------------------------------------------------
class GRAD_BC_FaceSelectionMenu
{
// ---- persistence keys ---------------------------------
protected static const string PROFILE_KEY_IDX = "GRAD_BC_SelectedFaceIdx";
protected static const string PROFILE_KEY_ID  = "GRAD_BC_SelectedFaceId";

// ---- layout resource ----------------------------------
protected static const ResourceName LAYOUT =
"{67BC000000000020}UI/Layouts/HUD/GRAD_BC_FaceSelection/GRAD_BC_FaceSelectionMenu.layout";
protected static const ResourceName ITEM_LAYOUT =
"{67BC000000000021}UI/Layouts/HUD/GRAD_BC_FaceSelection/GRAD_BC_FaceSelectionItem.layout";

// ---- face catalogue -----------------------------------
// Display names and face IDs for each entry.
// The face IDs must correspond to identifiers accepted by the
// game's character appearance system. Update them once the exact
// resource names are known from the game files.
protected ref array<string> m_aFaceNames;
protected ref array<string> m_aFaceIds;

// ---- state --------------------------------------------
protected Widget m_wRoot;
protected bool   m_bIsOpen;
protected int    m_iSelectedIndex;

// Widget references (set on Open, cleared on Close)
protected VerticalLayoutWidget m_wFaceList;
protected TextWidget           m_wSelectedFaceName;
protected TextWidget           m_wSelectedFaceLabel;

// Keep handlers alive so the GC does not collect them
protected ref array<ref GRAD_BC_FaceButtonHandler> m_aButtonHandlers;

// -------------------------------------------------------
void GRAD_BC_FaceSelectionMenu()
{
m_aButtonHandlers = new array<ref GRAD_BC_FaceButtonHandler>();
InitFaceCatalogue();
m_iSelectedIndex = LoadSelectedIndex();
}

// ---- public API ---------------------------------------

//! Open (or re-open) the overlay parented to parentWidget.
void Open(Widget parentWidget)
{
if (m_bIsOpen || !parentWidget)
return;

m_wRoot = GetGame().GetWorkspace().CreateWidgets(LAYOUT, parentWidget);
if (!m_wRoot)
{
Print("GRAD_BC_FaceSelectionMenu: failed to create widgets", LogLevel.ERROR);
return;
}

m_wFaceList         = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("FaceList"));
m_wSelectedFaceName = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedFaceName"));
m_wSelectedFaceLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedFaceLabel"));

// Close button
ButtonWidget closeBtn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("CloseButton"));
if (closeBtn)
closeBtn.m_OnClick.Insert(Close);

PopulateFaceList();
UpdateDisplay();

m_bIsOpen = true;
Print("GRAD_BC_FaceSelectionMenu: opened", LogLevel.NORMAL);
}

//! Close and destroy the overlay.
void Close()
{
if (!m_bIsOpen)
return;

if (m_wRoot)
{
m_wRoot.RemoveFromHierarchy();
m_wRoot = null;
}

m_aButtonHandlers.Clear();

m_wFaceList          = null;
m_wSelectedFaceName  = null;
m_wSelectedFaceLabel = null;

m_bIsOpen = false;
Print("GRAD_BC_FaceSelectionMenu: closed", LogLevel.NORMAL);
}

//! Toggle open / close state.
void Toggle(Widget parentWidget)
{
if (m_bIsOpen)
Close();
else
Open(parentWidget);
}

bool IsOpen()
{
return m_bIsOpen;
}

//! Called by GRAD_BC_FaceButtonHandler when a list entry is clicked.
void SelectFaceByIndex(int index)
{
if (index < 0 || index >= m_aFaceNames.Count())
return;

m_iSelectedIndex = index;
SaveSelectedIndex(index);
UpdateDisplay();
RefreshHighlights();

Print(string.Format("GRAD_BC_FaceSelectionMenu: selected '%1' (id='%2')",
m_aFaceNames[index], m_aFaceIds[index]), LogLevel.NORMAL);
}

//! Returns the face ID that is currently persisted in the local profile.
//! Can be called from anywhere (e.g. GRAD_PlayerComponent at spawn time).
static string GetSelectedFaceId()
{
LocalProfileValues profile = GetGame().GetLocalProfileValues();
if (!profile)
return "";
return profile.GetStringValue(PROFILE_KEY_ID, "");
}

// ---- private ------------------------------------------

protected void InitFaceCatalogue()
{
m_aFaceNames = new array<string>();
m_aFaceIds   = new array<string>();

// Each pair: display name, game face ID.
// Face IDs correspond to identifiers in Arma Reforger's character
// appearance system. Replace with actual game resource names.
AddFace("Default",         "");
AddFace("Male - Light 01", "face_white_young_1");
AddFace("Male - Light 02", "face_white_young_2");
AddFace("Male - Middle 01","face_white_middle_1");
AddFace("Male - Middle 02","face_white_middle_2");
AddFace("Male - Older",    "face_white_old_1");
AddFace("Male - Tan 01",   "face_tan_young_1");
AddFace("Male - Tan 02",   "face_tan_middle_1");
AddFace("Male - Dark 01",  "face_black_young_1");
AddFace("Male - Dark 02",  "face_black_middle_1");
}

protected void AddFace(string displayName, string faceId)
{
m_aFaceNames.Insert(displayName);
m_aFaceIds.Insert(faceId);
}

protected int LoadSelectedIndex()
{
LocalProfileValues profile = GetGame().GetLocalProfileValues();
if (!profile)
return 0;

int idx = profile.GetIntValue(PROFILE_KEY_IDX, 0);
if (idx < 0 || idx >= m_aFaceNames.Count())
idx = 0;
return idx;
}

protected void SaveSelectedIndex(int index)
{
LocalProfileValues profile = GetGame().GetLocalProfileValues();
if (!profile)
return;

profile.SetIntValue(PROFILE_KEY_IDX, index);
if (index >= 0 && index < m_aFaceIds.Count())
profile.SetStringValue(PROFILE_KEY_ID, m_aFaceIds[index]);
}

protected void PopulateFaceList()
{
if (!m_wFaceList)
return;

SCR_WidgetHelper.RemoveAllChildren(m_wFaceList);
m_aButtonHandlers.Clear();

for (int i = 0; i < m_aFaceNames.Count(); i++)
{
Widget item = GetGame().GetWorkspace().CreateWidgets(ITEM_LAYOUT, m_wFaceList);
if (!item)
continue;

// Give each item a unique name so RefreshHighlights can find it
item.SetName(string.Format("FaceItem_%1", i));

TextWidget nameText = TextWidget.Cast(item.FindAnyWidget("FaceItemName"));
if (nameText)
nameText.SetText(m_aFaceNames[i]);

ButtonWidget btn = ButtonWidget.Cast(item.FindAnyWidget("FaceItemButton"));
if (btn)
{
GRAD_BC_FaceButtonHandler handler = new GRAD_BC_FaceButtonHandler(this, i);
m_aButtonHandlers.Insert(handler);
btn.m_OnClick.Insert(handler.OnClick);
}
}

// Apply initial highlight after populating
RefreshHighlights();
}

protected void UpdateDisplay()
{
if (m_iSelectedIndex < 0 || m_iSelectedIndex >= m_aFaceNames.Count())
return;

string name = m_aFaceNames[m_iSelectedIndex];

if (m_wSelectedFaceName)
m_wSelectedFaceName.SetText(name);

if (m_wSelectedFaceLabel)
m_wSelectedFaceLabel.SetText(string.Format("Selected: %1", name));
}

protected void RefreshHighlights()
{
if (!m_wFaceList)
return;

for (int i = 0; i < m_aFaceNames.Count(); i++)
{
Widget item = m_wFaceList.FindAnyWidget(string.Format("FaceItem_%1", i));
if (!item)
continue;

ImageWidget bg = ImageWidget.Cast(item.FindAnyWidget("FaceItemBackground"));
if (!bg)
continue;

if (i == m_iSelectedIndex)
bg.SetColor(new Color(0.2, 0.5, 0.8, 0.6));
else
bg.SetColor(new Color(0.0, 0.0, 0.0, 0.3));
}
}
}
