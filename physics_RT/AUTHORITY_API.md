# physics_RT authority API v1

`PhysicsRT_GetApi(1)` is the only exported entry point added by the authority
interface. It returns a size-tagged function table declared in
`PhysicsRTApi.h`; unsupported versions return `NULL`. The same implementation
is compiled into the Win32 `physics_RT.dll` and `physics_RTStatic`.

## Lifetime and threading

- Call `acquire_world` with the owning `CKContext *`, passed as `void *` so the
  C header does not expose CK types.
- Every world and body call must run on the thread that constructed that
  world's `CKIpionManager`. Calls from another thread return
  `PHYSICSRT_ERROR_WRONG_THREAD` without touching CK or IVP.
- Body handles are monotonically generated tokens, not pointers. Physicalize
  creates a new handle. Unphysicalize, CK object deletion, world reset, and
  manager destruction invalidate it immediately.
- `create_ball` physicalizes an existing `CK3dEntity` selected by CK ID; it
  creates no render object. An empty collision group allows player balls to
  collide with each other.
- `capture_ball_desc` reads a native `IVP_Ball` into the same pure-C
  `PhysicsRT_BallDesc` accepted by `create_ball`, including exact radius, mass,
  material, damping, collision group and current rigid-body state. Replace
  `ck_id` (and normally pose/velocities) to instantiate another ball from the
  loaded map's real archetype. Static, polygon and multi-ball bodies are
  rejected instead of being approximated.
- `enumerate_bodies` is sorted by CK ID. Call it once with a null output buffer
  to obtain the required count, then supply that many `PhysicsRT_BodyRef`
  entries.

## State and authority stepping

- `get_body_states` and `set_body_states` operate on complete batches at one
  owner-thread physics boundary. Set validates every handle, size, flag, finite
  value, quaternion, and static/active state before mutating any body.
- Quaternion order is `x,y,z,w`. Linear and angular velocities are in world
  coordinates; angular velocity is radians per second.
- `reconcile_body_states` additionally requires authority mode. It restores
  pose, linear/angular velocity, collision and sleep state, and clears queued
  velocity changes before the next tick.
- Authority mode disables `PostProcess`'s legacy frame-driven simulation.
  `step_fixed` is then the only supported scheduler: each tick is exactly
  `1/66` second, and a call is capped at eight ticks to bound catch-up work.
- Leaving authority mode restores the original smoothed frame-delta path.
- Client mirrors call `set_gameplay_writes_enabled(world, 0)` while an
  authoritative room is running. This pauses behavior-graph force, impulse,
  wake/reset, gravity/time-factor, constraint, buoyancy, collision-surface
  deletion, and unphysicalize writes while leaving Physicalize available for
  stable body binding. Deferred mutating callbacks remain pending and existing
  continuous force controllers resume when the policy is restored.
- The policy is enabled by default. It does not gate this C ABI: atomic state
  reconcile, authority force/impulse commands, body creation/destruction, and
  fixed stepping remain available to the client prediction/reconcile backend.

## Build identity

`get_build_info` reports the source Git SHA (with `-dirty` for tracked local
changes), the solver compatibility ID, ABI version, fixed step, and catch-up
limit. A network authority session must compare both IDs before accepting
physics input or snapshots.
