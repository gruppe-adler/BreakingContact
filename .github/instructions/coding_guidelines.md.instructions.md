---
applyTo: '**'
---
# BreakingContact Mod - Coding Guidelines

## Project Overview

**BreakingContact** is an Arma Reforger game mode mod featuring:
- **Two-phase gameplay**: OPFOR spawning phase → BLUFOR vs OPFOR conflict phase
- **Transmission mechanics**: OPFOR must establish/protect radio transmissions, BLUFOR must destroy them
- **Budget-based vehicle spawning**: Limited supply system for vehicle deployment
- **Replay system**: Post-game map visualization with player movements, projectiles, and transmission states
- **Faction-specific actions**: Actions visible/usable only by specific factions

---

## Language: Enforce Script

This project uses **Enforce Script**, Bohemia Interactive's scripting language for Arma Reforger. It is C-like but has significant differences from C/C++/C#.

---

## Critical Coding Rules

### ❌ **NEVER Use Ternary Operators**
Enforce Script does **NOT** support ternary operators (`? :`).

**❌ WRONG:**
```c
float iconSize = m_bIsVehicle ? 70.0 : 48.0;
```

**✅ CORRECT:**
```c
float iconSize;
if (m_bIsVehicle)
    iconSize = 70.0;
else
    iconSize = 48.0;
```

---

## Common Type and API Gotchas

### Base Object Type
- **Use `Managed`** for base managed object types, NOT `Object`
- `Object` does not exist in Enforce Script

**✅ CORRECT:**
```c
Managed obj = Replication.FindItem(rplId);
RplComponent rplComp = RplComponent.Cast(obj);
```

### RplComponent Entity Access
- Use `GetEntity()` not `GetOwner()`

**✅ CORRECT:**
```c
IEntity entity = rplComp.GetEntity();
```

### User Action Progress Tracking
- Override `GetActionProgressScript(float fProgress, float timeSlice)` for timed actions
- Do NOT try to override `GetActionProgress()` (doesn't exist in base class)

**✅ CORRECT:**
```c
override float GetActionProgressScript(float fProgress, float timeSlice)
{
    float elapsed = (System.GetTickCount() - m_fStartTime) / 1000.0;
    return Math.Clamp(elapsed / ACTION_DURATION, 0.0, 1.0);
}
```

---

## Project Architecture

### Component Structure

#### Map Modules (inherit from `SCR_MapModuleBase`)
- `GRAD_MapMarkerManager` - Handles transmission circle markers on map
- `GRAD_BC_ReplayMapLayer` - Replay visualization overlay

#### Player Components (attached to PlayerController)
- `GRAD_PlayerComponent` - Per-player state management
  - Contains `GRAD_IconMarkerUI` instance (NOT a map module!)

#### Transmission System
- `GRAD_BC_TransmissionComponent` - Attached to transmission antenna entities
  - States: `OFF`, `TRANSMITTING`, `INTERRUPTED`, `DISABLED`, `DONE`
  - Use `GetTransmissionState()` for action conditions, NOT just `GetTransmissionActive()`

#### Vehicle Spawning
- `GRAD_BC_VehicleSupplyComponent` - Tracks available supplies
- `GRAD_BC_VehicleSpawnAction` - Modded `SCR_CampaignBuildingStartUserAction`
- `GRAD_BC_VehicleSpawnListener` - Global spawn event listener

#### Replay System
- `GRAD_BC_ReplayManager` - Records and plays back game events
- `GRAD_BC_ReplayData` - Frame-based replay data structure

---

## Class Hierarchy Notes

### GRAD_IconMarkerUI is NOT a Map Module
- It's a regular class instantiated per-player
- Cannot be accessed via `mapEntity.GetMapModule()`
- Each player has their own instance in `GRAD_PlayerComponent`

### User Actions Hierarchy
```
ScriptedUserAction (base)
├── GRAD_BC_DestroyRadioTransmission (antenna disable)
├── GRAD_BC_DisableRadioTruck (radio truck disable)
└── GRAD_BC_VehicleSpawnAction (modded SCR_CampaignBuildingStartUserAction)
```

---

## Faction System

### Faction Keys
- **BLUFOR**: `"US"`
- **OPFOR**: `"USSR"`

### Faction-Specific Action Pattern
```c
protected bool IsUserBlufor(IEntity user)
{
    SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
    if (!character)
        return false;
    
    string factionKey = character.GetFactionKey();
    return (factionKey == "US");
}

override bool CanBeShownScript(IEntity user)
{
    if (!IsUserBlufor(user))
        return false;
    
    return true;
}
```

---

## RPC and Networking

### RPC Patterns
```c
// Server → Owner (single client)
[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
protected void RpcDo_Owner_SomeAction() { }

// Client → Server
[RplRpc(RplChannel.Reliable, RplRcver.Server)]
protected void RpcDo_Server_SomeRequest() { }

// Broadcast to all
[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
protected void RpcDo_Broadcast_SomeEvent() { }
```

### Replication Considerations
- Always call `Replication.BumpMe()` after changing `[RplProp()]` variables
- Check `Replication.IsServer()` for server-side logic
- Use `HasLocalEffectOnlyScript()` to control where actions execute

---

## Icon Sizing Standards

- **Infantry icons**: 48x48 pixels
- **Vehicle icons**: 70x70 pixels
- Track state with `m_bIsVehicle` boolean flag

---

## Debugging

### Print Levels
```c
Print("Message", LogLevel.NORMAL);   // Standard logging
Print("Warning", LogLevel.WARNING);  // Warnings
Print("Error", LogLevel.ERROR);      // Errors
```

### Common Debug Patterns
- Prefix debug messages with component name: `"BC Debug - ..."`
- Log RplIds when tracking replicated entities
- Log state transitions in transmission components

---

## Performance Considerations

### Periodic Updates
- Vehicle state checking: Every 2000ms (2 seconds)
- Map marker updates: Use event-driven approach, not polling
- Replay frame rate: Adaptive based on total duration

### Cleanup
- Always remove widgets with `widget.RemoveFromHierarchy()`
- Clear arrays with `.Clear()` when done
- Remove CallLater callbacks when stopping: `GetGame().GetCallqueue().Remove(Method)`

---

## File Structure

```
Scripts/
├── Game/
│   ├── Components/           # Core game components
│   │   ├── GRAD_BC_BreakingContactManager.c
│   │   ├── GRAD_BC_ReplayManager.c
│   │   ├── GRAD_BC_TransmissionComponent.c
│   │   ├── GRAD_BC_VehicleSupplyComponent.c
│   │   └── GRAD_PlayerComponent.c
│   ├── Map/
│   │   └── ComponentsUI/     # Map UI components
│   │       ├── GRAD_IconMarkerUI.c
│   │       └── GRAD_MapMarkerUI.c
│   ├── UI/
│   │   └── Map/              # Map modules
│   │       ├── GRAD_MapMarkerManager.c
│   │       └── GRAD_BC_ReplayMapLayer.c
│   └── UserActions/          # Player interaction actions
│       ├── GRAD_BC_DestroyRadioTransmission.c
│       ├── GRAD_BC_DisableRadioTruck.c
│       └── GRAD_BC_VehicleSpawnAction.c
```

---

## Common Mistakes to Avoid

1. ❌ Using ternary operators
2. ❌ Using `Object` type instead of `Managed`
3. ❌ Calling `GetOwner()` on RplComponent (use `GetEntity()`)
4. ❌ Treating `GRAD_IconMarkerUI` as a map module
5. ❌ Checking `GetTransmissionActive()` instead of `GetTransmissionState()`
6. ❌ Overriding non-existent base class methods (check API first!)
7. ❌ Forgetting to call `Replication.BumpMe()` after changing RplProp
8. ❌ Not checking `IsServer()` before server-only logic
9. ❌ Forgetting to remove CallLater callbacks
10. ❌ Not using absolute file paths for resource references

---

## Testing Checklist

### Transmission Actions
- [ ] Blufor can see and execute antenna disable action
- [ ] Opfor cannot see antenna disable action
- [ ] Action only available when transmission is TRANSMITTING
- [ ] Radio truck disable takes 30 seconds
- [ ] Radio truck disable only visible to Blufor

### Vehicle Spawning
- [ ] Action grays out when supplies insufficient
- [ ] Supplies are deducted only once per spawn
- [ ] Building interface updates when supplies change

### Map Markers
- [ ] Infantry icons show at 48x48
- [ ] Vehicle icons show at 70x70
- [ ] Icons update when player enters/exits vehicle
- [ ] Live markers cleared when replay starts

### Replay
- [ ] Transmission markers cleared before replay
- [ ] Player positions recorded correctly
- [ ] Projectile trajectories visible
- [ ] Replay speed adaptive to fit within 2 minutes

---

## Version Control

- All `.c` files are tracked
- `.meta` files are tracked (contain important prefab data)
- `resourceDatabase.rdb` is tracked
- Test logs in root should be `.gitignore`d

---

**Last Updated**: January 3, 2026
**Arma Reforger Version**: 1.6+
**Enforce Script Version**: Latest stable
