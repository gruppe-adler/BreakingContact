# JIP (Join in Progress) Compatibility Guide

## Overview

This document explains how BreakingContact handles JIP (Join in Progress) synchronization to ensure players who join an ongoing game receive the current game state properly.

## Core Concepts

### What is JIP?

JIP (Join in Progress) refers to players joining a multiplayer game that has already started. These players need to receive:
1. Current game state (phase, scores, objectives)
2. Visual state (UI elements, markers, animations)
3. Entity state (vehicle positions, transmission status)

### Arma Reforger Replication System

**RplProp Variables:**
- Automatically replicate to all clients, including JIP players
- JIP players receive the **current value** but don't experience the **state transition**
- `onRplName` callbacks only fire on **delta changes**, not on initial sync for JIP players

**RPC Types:**
| Type | JIP Behavior | Use Case |
|------|--------------|----------|
| `Owner` | NOT replayed | Per-player UI updates |
| `Server` | Received by JIP | Client-to-server requests |
| `Broadcast` | Received by JIP | Shared state changes |

## Implementation Pattern

### Component: GRAD_BC_BreakingContactManager

**Problem:** JIP players don't receive state change notifications because replication callbacks don't fire on initial sync.

**Solution:** Manual callback triggering in `SyncJIPState()`

```c
override void OnPostInit(IEntity owner)
{
    SetEventMask(owner, EntityEvent.INIT);

    if (Replication.IsServer())
    {
        // Server initialization
    }
    else
    {
        // Client-side JIP synchronization
        GetGame().GetCallqueue().CallLater(SyncJIPState, 1000, false);
    }
}

protected void SyncJIPState()
{
    if (Replication.IsServer())
        return; // Only run on clients

    // Check for spectators
    SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
    if (!playerController || !playerController.GetControlledEntity())
        return; // Skip spectators

    // Defer execution to avoid race with UI initialization
    GetGame().GetCallqueue().CallLater(SyncJIPStateDeferred, 200, false);
}

protected void SyncJIPStateDeferred()
{
    // Manually trigger replication callbacks
    if (m_iBreakingContactPhase != EBreakingContactPhase.LOADING)
        OnBreakingContactPhaseChanged();
    
    if (m_aTransmissionPositions.Count() > 0)
        OnTransmissionMarkerDataChanged();
}
```

### Component: GRAD_BC_RadioTruckComponent

**Problem:** Visual state (antenna position, lights, props) must be reconstructed from replicated state.

**Solution:** Instant state application without animation

```c
override void OnPostInit(IEntity owner)
{
    // ... initialization ...
    
    // Schedule JIP sync for clients
    GetGame().GetCallqueue().CallLater(SyncJIPState, 1000, false);
}

protected void SyncJIPState()
{
    // Only process on clients
    if (m_RplComponent && m_RplComponent.IsMaster())
        return;

    // Apply antenna state INSTANTLY (no animation for JIP)
    if (m_bAntennaStateRaised)
    {
        m_fAnimationProgress = 1.0;
        m_bAntennaExtended = true;
        UpdateAntennaBones(1.0);
    }
    
    // Apply prop states
    if (m_bRedLightPropSpawned && !m_RedLightPropEntity)
        SpawnAntennaProp();
}
```

## Critical Patterns

### 1. Timing and Race Conditions

**Problem:** Multiple components initialize at the same time (1000ms delay)

**Solution:** Add buffer delays

```c
// BCM schedules JIP sync at +1000ms
GetGame().GetCallqueue().CallLater(SyncJIPState, 1000, false);

// PlayerComponent initializes UI at +1000ms
GetGame().GetCallqueue().CallLater(InitMapMarkerUI, 1000, false);

// BCM defers callbacks to +1200ms (200ms buffer)
GetGame().GetCallqueue().CallLater(SyncJIPStateDeferred, 200, false);
```

### 2. Spectator Detection

**Always check for controlled entity before UI operations:**

```c
SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
if (!playerController)
    return;

IEntity controlledEntity = playerController.GetControlledEntity();
if (!controlledEntity)
{
    // Player is spectator, skip UI synchronization
    return;
}
```

### 3. Null Safety

**Check component instances before calling methods:**

```c
GRAD_PlayerComponent playerComponent = GRAD_PlayerComponent.GetInstance();
if (!playerComponent)
    return;

// Check UI is initialized
if (!playerComponent.GetIconMarkerUI())
    return;
```

### 4. Cleanup

**Always remove CallLater callbacks in OnDelete:**

```c
override void OnDelete(IEntity owner)
{
    if (GetGame() && GetGame().GetCallqueue())
    {
        GetGame().GetCallqueue().Remove(SyncJIPState);
        GetGame().GetCallqueue().Remove(SyncJIPStateDeferred);
    }
    super.OnDelete(owner);
}
```

## Component Status

| Component | JIP Compatible | Method | Notes |
|-----------|----------------|--------|-------|
| GRAD_BC_BreakingContactManager | ✅ Yes | Manual callbacks | Phase, transmissions, markers |
| GRAD_BC_RadioTruckComponent | ✅ Yes | State reconstruction | Antenna, lights, props |
| GRAD_BC_TransmissionComponent | ✅ Yes | RplProp auto-sync | State and progress |
| GRAD_BC_DraggableComponent | ✅ Yes | RplProp + callback | Drag state |
| GRAD_PlayerComponent | ✅ Yes | Indirect via BCM | Relies on BCM callbacks |
| GRAD_BC_ReplayManager | ⚠️ Partial | N/A | JIP during replay not supported |

## Testing Checklist

When testing JIP compatibility:

- [ ] **OPFOR Phase**: JIP player sees transmission circles and markers
- [ ] **GAME Phase**: JIP player sees all active transmissions and radio truck marker
- [ ] **GAMEOVER Phase**: JIP player enters spectator mode correctly
- [ ] **Spectator JIP**: No crashes or UI errors
- [ ] **USSR JIP**: Spawn position marker visible (if applicable)
- [ ] **US JIP**: Transmission markers visible
- [ ] **Radio Truck State**: Antenna position correct for JIP
- [ ] **Dragged Antenna**: Position correct for JIP

## Known Limitations

1. **Replay Data**: JIP players joining during GAMEOVER phase won't see the replay visualization (only spectate)
2. **Historical Hints**: JIP players miss tactical hints that were shown before they joined
3. **Animation Transitions**: JIP sees instant state, not smooth animations
4. **Owner RPCs**: Past UI updates sent via Owner RPC are not replayed

## Best Practices

1. **Use RplProp for State**: Prefer `[RplProp()]` over RPCs for persistent state
2. **Separate Visual from State**: State changes should update RplProp, visuals should react to state
3. **Defer Callbacks**: Add 200ms buffer when depending on other component initialization
4. **Check for Spectators**: Always validate controlled entity exists
5. **Retry Mechanisms**: Use retry loops for dependent entities that may not be streamed yet
6. **Instant Visual State**: Apply final state instantly for JIP, animate for live players

## Debugging

Enable debug mode in mission header to see JIP synchronization logs:

```c
if (GRAD_BC_BreakingContactManager.IsDebugMode())
    Print("BC Debug - JIP: SyncJIPState called", LogLevel.NORMAL);
```

Look for these log patterns:
- `BC Debug - JIP: SyncJIPState called` - JIP sync started
- `BC Debug - JIP: Player is spectator` - Spectator detected
- `BC Debug - JIP: SyncJIPStateDeferred executing` - Deferred callbacks running
- `BC Debug - JIP: SyncJIPState completed` - JIP sync finished

## Version History

- **2025-02-18**: Initial JIP compatibility implementation
  - Added SyncJIPState to BreakingContactManager
  - Added spectator detection
  - Added deferred callback execution
  - Documented patterns and best practices
