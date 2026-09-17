# Street Soccer — Design Document

## Brazil Football Street / فوتبال خیابونی ایران

This document captures the gameplay design contract for the street soccer
mode shipped on the Kimia Engine.

## 1. Phased rollout

| Phase | Module | Status |
|-------|--------|--------|
| 1     | Pitch + Ball + Players + MatchState | done |
| 2     | Skill-Gated Power (Comeback Burst etc.) | done |
| 3     | AI traits + signature tricks | next |
| 4     | Street PowerPanel in EditorUI | next |
| 5     | Physics integration with `Engine/Physics` | next |

## 2. The Skill-Gated Power system

### Why it is not DDA

Classic DDA adjusts difficulty by tightening or loosening the game world for
the player. This is felt as artificial and unfair.

The Skill-Gated Power system instead **rewards demonstrated skill** with a
brief, finite advantage. If the player lacks the skill to use the advantage,
it expires wasted. The game itself never gets harder or easier; only the
player's effective stats change.

### Comeback Burst (headline power)

- **Trigger:** Team is losing by 2 or more goals.
- **Effect:** Player speed × 1.4 for 2 seconds.
- **Cooldown:** 60 seconds.
- **Mastery requirement:** 0.20 (modest) to use effectively.
- **Mastery gain:** +0.05 per successful use; capped at 1.0.
- **Mastery loss:** wasted activations cap mastery at 0.5.

### Mastery curve

A new player starts at 0 mastery. After ~4 successful activations, mastery
reaches the threshold (0.20) at which the boost becomes truly useful. After
~16 successful uses, mastery is full (1.0) and the player uses the boost
flawlessly.

A player who keeps wasting the boost caps at 0.5 mastery — they get the speed
multiplier but cannot consistently put it where it matters.

### Why this is fair

- Two players, same skill level, will both get the same boost when losing.
- A player with more skill will USE the boost more effectively; the boost is
  not the source of their win.
- The match result still depends on player input, not on system adjustments.

## 3. Power catalogue

| Power            | Trigger                       | Effect          | Duration | CD   |
|------------------|-------------------------------|-----------------|----------|------|
| Comeback Burst   | Losing by ≥ 2                 | Speed × 1.4     | 2 s      | 60 s |
| Last Stand       | Last 30% of time + losing     | Shot × 1.5      | 3 s      | 0    |
| Crowd Boost      | Tied score                    | Stamina drain × 0.5 | 5 s   | 90 s |
| Street Sense     | Opp has ball near own goal    | Vision × 2      | 1.5 s    | 45 s |
| Double or Nothing| 3 consecutive passes          | Shot × 1.3      | (single) | 30 s |

## 4. Skill metrics (rolling window)

The system tracks 5 metrics via exponential moving average (α = 0.05):

1. Shot accuracy (on-target / total shots)
2. Pass success (successful / total passes)
3. Possession retention (kept / received)
4. Tackle success (won / attempted)
5. Movement efficiency (useful distance / total distance)

These feed future power tuning and AI difficulty calibration. They are NOT
used to gate Comeback Burst itself — that is gated purely by score
difference, so a struggling player still has a path back into the match.

## 5. HUD contract

A single string is exposed via `powerHudString(state)`:

```
[Comeback Burst] 1.7s
```

Or empty if no power is active. Renderer is expected to overlay this string
near the active player's name tag with a per-power color.
