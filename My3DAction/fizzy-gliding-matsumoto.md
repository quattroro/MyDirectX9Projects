# RedMat1 Hit-Effect Redesign: Ripple → Directional Slash Gash + Jiggle + Fading Scar

## Context

`RedMat1` (`/Game/My3DAction/Monster/Material/RedMat1`) drives the dragon monster's skin reaction when hit by the player's sword. Earlier this session we fixed two real bugs that made the effect appear in the wrong place entirely (a `GetClosestPointOnCollision` call that always queried the PhysicsAsset's root/pelvis body regardless of where the sword hit, and a WPO direction pin that had lost its amplitude gain). With those fixed, the effect now correctly triggers at the actual hit location, but its *shape* is a radially-expanding sine ripple — it reads as a "wave/bump," not "flesh being slashed by a blade."

The user wants: **a directional slash-shaped gash (anisotropic, oriented across the cut) combined with a damped jiggle wobble (2-3 decaying oscillations, not a continuous wave)**, plus **a faint scar mark that lingers for tens of seconds to about a minute before fading away completely** (pure exponential decay, no permanent floor).

This plan was validated by a design-review pass that caught three real bugs in the initial draft (a NaN-producing degenerate cross product, an unsafe `Power()` on a signed base, and an over-complicated manual ellipse mask) — the plan below already incorporates the fixes.

## Current state (verified via live `material.inspect` this session)

- MID params already flowing correctly per-hit: `HitPos` (Vector, world-space impact point — now correct after this session's collision fix), `HitTime` (Scalar, world time at hit). `HitDir` (Vector) exists as a parameter node in the graph but **is never set by C++** — `Monster_Usurper::Hit()`'s `dir` argument (the outward surface normal at impact, already computed correctly in `MainCharacter.cpp`) is currently dropped.
- Current WPO chain: radial `sin(distance*Frequency - ElapsedTime*Speed)` decaying by `exp(-distance*0.009)` and `exp(-elapsedTime*5)`, pushed along `Normalize(WorldPosition-HitPos)` (radiates outward from the hit point) times a strength constant (`DirStrength`=15, `DirScaled`) — this whole radial/traveling-wave approach is being replaced.
- `EmissiveColor = TextureSample(RedHPTex) * Constant(1.3)` (node id `Multiply_0`) — this is the only color output wired; `BaseColor` is unconnected (defaults to black), so the visible red comes entirely from Emissive. The scar tint splices into this chain.

## C++ change

**`Source/My3DAction/Monster_Usurper.cpp`**, in `AMonster_Usurper::Hit()`: add
```cpp
DynamicMaterialInst->SetVectorParameterValue(TEXT("HitDir"), dir);
```
alongside the existing `HitTime`/`HitPos` calls. (`dir` is already the correct outward surface normal at the impact point — no change needed in `MainCharacter.cpp`.)

This is a game-module C++ change → reachable via `unreal-cli.exe compile --wait` (hot reload), no editor restart needed.

## Material redesign — node-by-node plan

All edits applied live via `unreal-cli.exe raw --% --json "{...}"` (the `material` subcommand, wrapping `material.add-node` / `material.connect` / `material.set-node` / `material.delete-node`), through PowerShell with the `--%` stop-parsing token (established working pattern this session). Verify each step with `material.inspect --withValues` before moving to the next.

### Nodes/wires to delete (superseded)
The old sin-ripple chain and its supporting nodes (distance/time decay `Exponential`s feeding the old `Sine`, the `Normalize(WorldPosition-HitPos)` radial-direction node, `Multiply_9`), plus this session's now-obsolete `DirStrength`/`DirScaled` nodes. **Before deleting, re-run `material.inspect --withValues` and confirm nothing else still reads from these** — delete one at a time with `material delete-node --force`.

### Reuse unchanged
`WorldPosition` node, the `HitPos` `VectorParameter`, the existing `Subtract` computing `ToHit = WorldPosition - HitPos`, `HitTime` `ScalarParameter`, `Time` node, the existing `ElapsedTime` reroute (`Time - HitTime`), the `HitDir` `VectorParameter` (finally wired from C++), and `Multiply_0` (the Emissive texture chain — splice the scar lerp right after it, don't touch its inputs).

### New nodes — geometry frame (turns HitDir into an orthonormal basis for anisotropic masking)
1. `Normalize(HitDir)` → **N**
2. `Dot(N, Constant3Vector(0,0,1))` → `Abs` → **AbsUpDot**
3. `If(A=AbsUpDot, B=Constant(0.9), AGreaterThanB=Constant3Vector(1,0,0), AEqualsB=Constant3Vector(1,0,0), ALessThanB=Constant3Vector(0,0,1))` → **RefAxis** — robust fallback so step 4 never normalizes a near-zero vector when `N` is close to vertical (the naive `Cross(N, WorldUp)` degenerates there — this was bug #1 caught in review).
4. `Cross(N, RefAxis)` → `Normalize` → **T** (tangent, roughly "along the cut")
5. `Cross(N, T)` → **Bt** (bitangent; already unit-length, no Normalize needed)
6. `Dot(ToHit, N)` → **AlongN**; `Multiply(N, AlongN)` → `Subtract(ToHit, .)` → **TangentOffset** (ToHit with the normal component removed)
7. `Dot(TangentOffset, T)` → **AlongCut**; `Dot(TangentOffset, Bt)` → **AcrossCut**

### New nodes — anisotropic mask (use SphereMask, not manual Power/Sqrt — bug #2/#3 fix)
8. `ScalarParameter GashLength` (≈40), `ScalarParameter GashWidth` (≈8), `ScalarParameter GashHardness` (≈0.4)
9. `Divide(AlongCut, GashLength)`, `Divide(AcrossCut, GashWidth)` → `AppendVector(., ., 0)` → **ScaledOffset**
10. `SphereMask(A=ScaledOffset, B=Constant3Vector(0,0,0), Radius=1.0, Hardness=GashHardness)` → **SpatialMask** — replaces a whole manual Power/Sqrt/Saturate chain; also sidesteps the negative-base-into-`Power` NaN risk entirely. **Verify exact pin names for this engine build** via `material list-node-types --filter spheremask` before wiring (pin layout has shifted across UE versions).

### New nodes — damped jiggle envelope (reuses `ElapsedTime` reroute)
11. `ScalarParameter JiggleDecay` (≈4-6), `ScalarParameter JiggleFreq` (start ~15-20, tune live in PIE for "2-3 visible cycles" — don't trust an analytic 2π assumption, UE's `Cosine`/`Sine` node `Period` convention has varied across versions; inspect the node's default `Period` property once added)
12. `Multiply(ElapsedTime, Multiply(JiggleDecay, -1))` → `Exp` → **Envelope**
13. `Multiply(ElapsedTime, JiggleFreq)` → `Cosine` → `Multiply(., -1)` → `Multiply(., Envelope)` → **Jiggle** (negated cosine so it dips inward first, then springs back and rings down — no dedicated negate node exists, use `Multiply(x, -1)`)

### New nodes — final WPO (pushes along the real surface normal, not radially from HitPos — this is the actual "bump→cut" fix)
14. `ScalarParameter WobbleStrength`
15. `Multiply(Jiggle, SpatialMask)` → `Multiply(., N)` → `Multiply(., WobbleStrength)` → **FinalWPO**
16. `material connect --from FinalWPO --property WorldPositionOffset`

### New nodes — persistent scar (pure fade, no floor — per user's answer)
17. `ScalarParameter SlowDecay` (≈0.05-0.1, tune for "fades over tens of seconds to ~1 min")
18. `Multiply(ElapsedTime, Multiply(SlowDecay, -1))` → `Exp` → **ScarDecayPart**
19. `Multiply(SpatialMask, ScarDecayPart)` → `Saturate` → **ScarAmount**
20. `Constant3Vector ScarColor` (dark desaturated red — tune visually)
21. `LinearInterpolate(A=Multiply_0, B=ScarColor, Alpha=ScarAmount)` → reconnect to `EmissiveColor` (replaces the current direct `Multiply_0 → EmissiveColor` wire)

## Known v1 limitation (flagged, not being solved now)

Only one `HitPos`/`HitTime` slot exists, so a new hit elsewhere immediately recenters the mask and the old scar vanishes rather than accumulating multiple simultaneous marks. A true multi-scar system would need a runtime-painted damage texture or an array of hit-slot parameters cycled in C++ — out of scope for this pass.

## Pitfalls carried into execution

- Don't use `Power(x, 2)` on any signed value (`AlongCut`/`AcrossCut`) — HLSL's `pow()` can NaN on negative bases even for integer exponents. The plan above avoids this entirely by using `SphereMask`/dot-products instead.
- WPO is evaluated per-vertex — `GashWidth`≈8 may not resolve cleanly on a low-poly area of the mesh. After wiring, visually check vertex density near typical hit locations; widen `GashWidth` or lean more on the (pixel-shader, not vertex-limited) scar tint if the geometry gash looks faceted/invisible.
- Confirm `HitTime`'s authored Default Value is `0.0` via `material.inspect --withValues` (needed for the "self-solving before first hit" `Exp(-large)→0` argument to hold).
- Add a `Normalize` on `HitDir`'s output before using it as `N`, even though `MainCharacter.cpp` already normalizes — cheap insurance against param round-trip precision loss.

## Verification

1. `unreal-cli.exe compile --wait` after the `Monster_Usurper.cpp` edit; confirm no compile errors via `read-log --type error`.
2. After each batch of material node edits, `material.inspect --withValues` to confirm wiring matches the plan before proceeding (cheaper than discovering a bad connection after 20 more nodes).
3. `material compile --save` once the graph is complete.
4. In PIE, hit the monster at a few different body locations and confirm: (a) the gash/jiggle appears at the actual hit point and is elongated/directional rather than a circular ripple, (b) it damps out over ~1-2 sec instead of continuing to ripple, (c) a faint red-tinted mark lingers at the hit spot for tens of seconds before fully fading, (d) hitting a new spot recenters everything to the new location (expected v1 limitation, not a bug).
