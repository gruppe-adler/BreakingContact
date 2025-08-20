# Projectile Tracking for Replay System

## Overview

The projectile tracking system has been adapted for Arma Reforger's architecture. Unlike Arma 3, Reforger doesn't have easily accessible projectile entities, so we use a different approach.

## How It Works

### 1. Event-Based Tracking
Instead of tracking individual projectile entities, we hook into weapon firing events and record:
- Muzzle position
- Muzzle direction and velocity
- Ammunition type
- Fire time

### 2. Ballistic Calculation
During replay recording, we calculate projectile positions using simple ballistics:
- Position = Initial Position + (Velocity × Time)
- Apply gravity: Y Position -= 9.81 × Time² × 0.5

## Implementation Options

### Option A: Manual Integration (Current)
The current `GRAD_BC_ProjectileTracker` component is designed to be attached to weapon entities, but requires manual event hookup.

**To use:**
1. Attach `GRAD_BC_ProjectileTracker` to weapon prefabs
2. Call `OnWeaponFired()` from your weapon firing code
3. The tracker will automatically register projectiles with the replay manager

### Option B: Weapon Component Integration (Recommended)
For better integration, you should hook into Reforger's weapon system directly:

```c
// In your weapon firing code:
GRAD_BC_ReplayManager replayManager = GRAD_BC_ReplayManager.GetInstance();
if (replayManager)
{
    replayManager.RecordProjectileFired(muzzlePosition, velocity, ammoType);
}
```

### Option C: Game Mode Integration
Integrate directly in your game mode's weapon event handling:

```c
// Listen for weapon fire events in your game mode
void OnWeaponFired(IEntity weapon, vector muzzlePos, vector muzzleDir, string ammo)
{
    GRAD_BC_ReplayManager replayManager = GRAD_BC_ReplayManager.GetInstance();
    if (replayManager)
    {
        vector velocity = muzzleDir * GetMuzzleVelocity(ammo);
        replayManager.RecordProjectileFired(muzzlePos, velocity, ammo);
    }
}
```

## Configuration

Projectile recording can be enabled/disabled in the replay manager:
- `m_bRecordProjectiles`: Enable/disable projectile recording
- `m_fMaxProjectileDistance`: Maximum distance for recording (500m default)

## Performance Considerations

- Only records projectile initial data (not continuous tracking)
- Uses ballistic calculations instead of real-time entity tracking
- Automatically cleans up old projectile data
- Minimal performance impact compared to entity-based tracking

## Limitations

1. **Simplified Ballistics**: Uses basic gravity calculation, no air resistance
2. **No Collision Detection**: Projectiles continue through terrain/objects in replay
3. **Manual Integration**: Requires weapon system integration to work properly
4. **No Real Entity Access**: Cannot access actual projectile entities for advanced features

## Future Improvements

1. **Advanced Ballistics**: Add air resistance, wind effects
2. **Collision Detection**: Calculate impact points with terrain
3. **Automatic Integration**: Hook into engine weapon events when available
4. **Projectile Types**: Different ballistic profiles per ammunition type
