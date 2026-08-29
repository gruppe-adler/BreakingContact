// Adds a BreakingContact-specific editor mode so admins get a way back to spectator from inside
// the Game Master editor. Entering GM from spectator otherwise leaves no route back.
//
// WHY A NEW VALUE INSTEAD OF REUSING EEditorMode.SPECTATE:
// SPECTATE (1 << 2) already exists in the base enum and vanilla has its own prefab/handling bound
// to it, which conflicts when redefined. A fresh bit is inert until we wire it up ourselves.
//
// The base enum ends at PHOTO_SAVE = 1 << 6, so 1 << 7 is the first free bit:
//   EDIT       = 1
//   STRATEGY   = 1 << 1
//   SPECTATE   = 1 << 2
//   PHOTO      = 1 << 3
//   ADMIN      = 1 << 4
//   BUILDING   = 1 << 5
//   PHOTO_SAVE = 1 << 6
//
// Wiring this up takes three more pieces, all outside this file:
//   1. A mode entity prefab (duplicate of EditorModeEdit.et) pointing at its own mode layout
//   2. A container in the EditorManagerCore.conf override referencing that prefab, with
//      m_Mode GRAD_BC_SPECTATE
//   3. m_Flags on that container controlling who sees the entry (ADMIN = admins only)
modded enum EEditorMode
{
	GRAD_BC_SPECTATE = 1 << 7, //< BreakingContact: return-to-spectator mode for admins
};
