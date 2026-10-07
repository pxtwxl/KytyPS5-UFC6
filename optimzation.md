TASK: Add a temporary UFC 6 GPU workload capture and selective group-disable
system to KytyPS5.

Repository baseline/tag:

KytyPS5-2026-10-05-0845bea

Game:

EA SPORTS UFC 6
PPSA23566
01.004.000

Hardware:

AMD Ryzen 5 5600H
RTX 3050 Laptop GPU 4 GB
16 GB DDR4
Windows 11
Vulkan

============================================================
OBJECTIVE
============================================================

We previously used draw-call capture/group isolation successfully while
investigating UFC 5.

Apply the same general methodology to UFC 6.

DO NOT begin by guessing which shader is the crowd, fog, shadow, etc.

Instead:

1. capture actual draw/dispatch workloads during a UFC 6 fight
2. generate stable workload/group identifiers
3. aggregate their activity
4. identify the most significant groups
5. allow selected groups to be disabled
6. I will manually launch UFC 6 and visually determine what disappears
7. we will then classify groups as:
   - fighters
   - octagon
   - cage
   - crowd
   - referee
   - arena
   - shadows
   - volumetric/fog
   - post-processing
   - UI
   - unknown

The immediate goal is instrumentation and selective suppression.

Do NOT permanently remove crowd/fog/shadows yet.

============================================================
IMPORTANT

DO NOT RUN AUTOMATED TEST CASES.

Building is allowed.

Do not automatically launch UFC 6.

I will manually run the game and provide the generated capture/log.

============================================================
PHASE 1 — FIND THE CENTRAL GPU SUBMISSION PATH
============================================================

Inspect the renderer and identify the smallest number of locations
through which UFC 6 GPU work passes.

Find handling for operations such as:

Draw
DrawIndexed
DrawIndirect
DrawIndexedIndirect

and:

Dispatch
DispatchIndirect

Also identify:

pipeline binding
shader/program identity
render target binding
depth target binding
descriptor/resource binding

Do NOT add logging separately throughout dozens of unrelated files if
the information can be captured centrally.

============================================================
PHASE 2 — GENERATE STABLE GROUP IDENTITIES
============================================================

Each workload needs a reasonably stable identifier.

Do NOT use only:

draw number

frame number

guest virtual address

because these may change between runs.

Construct a group/signature using information that is already safely
available.

Potential components:

graphics/compute pipeline identity
vertex shader identity/hash
fragment shader identity/hash
compute shader identity/hash
render-target characteristics
depth target characteristics
primitive topology
descriptor/resource signature where useful

Do NOT make the key unnecessarily huge.

We want groups such as:

GROUP 0001
GROUP 0002
GROUP 0003
...

mapped internally to stable signatures.

============================================================
PHASE 3 — CAPTURE DRAW METADATA
============================================================

For each unique graphics group, aggregate:

draw count

indexed draw count

non-indexed draw count

indirect draw count

total vertices if known

total indices if known

total instances

average vertices/indices per draw

maximum vertices/indices per draw

primitive topology

vertex shader identity

fragment shader identity

pipeline identity

number/format/extent of color render targets if cheaply available

depth target presence and extent

depth-only / color rendering characteristics where identifiable

Do NOT print one line for every draw call.

Aggregate statistics.

============================================================
PHASE 4 — CAPTURE COMPUTE DISPATCHES
============================================================

This is mandatory.

Do not instrument only graphics draws.

Capture:

Dispatch
DispatchIndirect

For each compute group aggregate:

dispatch count

workgroup dimensions where available

total workgroups

compute shader identity

pipeline identity

major bound output resource characteristics where cheaply available

This matters because:

volumetric fog
lighting
post-processing
culling
resource conversion

may use compute rather than ordinary draw calls.

============================================================
PHASE 5 — FRAME WINDOW
============================================================

Do not continuously produce gigantic logs.

Implement a controlled capture window.

For example:

UFC6_CAPTURE_GPU_WORK=1

and allow something equivalent to:

capture N frames

or:

capture between frame A and frame B.

Use the project's existing configuration/environment-variable style if
there is one.

A short stable fight window such as 30–120 frames should be enough.

After the capture window ends:

stop detailed collection

and write the aggregated summary.

============================================================
PHASE 6 — SUMMARY
============================================================

Generate a concise summary into the existing Kyty logging system.

Example concept:

=============================
UFC6 GPU WORKLOAD SUMMARY
=============================

Captured frames: 60

GRAPHICS GROUP 17

draws: 8400
draws/frame: 140
indices/frame: ...
instances/frame: ...
VS: ...
FS: ...
pipeline: ...
color targets: ...
depth target: ...

GRAPHICS GROUP 22

draws: 3600
...

COMPUTE GROUP 4

dispatches: 120
dispatches/frame: 2
workgroups: ...
CS: ...

Do NOT use these example numbers as real values.

============================================================
PHASE 7 — RANK CANDIDATES
============================================================

Generate useful rankings.

At minimum report:

highest draw-call groups

highest index/vertex workload groups

highest instance-count groups

highest dispatch/workgroup groups

Do NOT label something "most expensive GPU group" solely because it has
the most draw calls.

Without GPU timestamps we do not know actual GPU execution cost.

Use terminology such as:

HIGH DRAW COUNT

HIGH GEOMETRY WORKLOAD

HIGH INSTANCE COUNT

HIGH COMPUTE WORKLOAD

instead of claiming measured GPU cost.

============================================================
PHASE 8 — OPTIONAL GPU TIMESTAMPS
============================================================

If Kyty's current architecture makes Vulkan timestamp queries easy and
safe to integrate, investigate per-group GPU timing.

However:

DO NOT introduce synchronization stalls merely to read timestamps.

DO NOT vkDeviceWaitIdle() every frame.

DO NOT FlushAndWait() after every group.

If accurate asynchronous GPU timestamp collection would require major
renderer changes, leave it out of the first implementation.

The initial draw/dispatch grouping system is more important.

============================================================
PHASE 9 — SELECTIVE GROUP DISABLE
============================================================

Implement a temporary diagnostic mechanism allowing specific groups to
be suppressed.

Conceptually:

UFC6_DISABLE_GRAPHICS_GROUPS=17,22,31

UFC6_DISABLE_COMPUTE_GROUPS=4

Use the repository's existing configuration style where appropriate.

Do NOT hardcode random groups permanently.

============================================================
PHASE 10 — SAFE GRAPHICS SUPPRESSION
============================================================

For a selected graphics group:

skip the actual draw operation as early as safely possible.

Do not destroy:

pipeline objects
buffers
textures
descriptors

merely because the draw is suppressed.

Do not modify guest memory.

Do not alter unrelated renderer state.

The purpose is:

bind/setup state normally where required
        ↓
identify group
        ↓
skip selected draw
        ↓
continue processing subsequent commands correctly

============================================================
PHASE 11 — BE MORE CONSERVATIVE WITH COMPUTE
============================================================

Do NOT blindly skip arbitrary compute groups.

Compute shaders may generate data required later by:

fighters
animation
skinning
indirect drawing
culling
lighting
resource conversion
synchronization

Therefore:

capture compute groups first.

Allow compute suppression only behind an explicitly experimental option.

Clearly warn in logs when a compute group is disabled.

============================================================
PHASE 12 — IDENTIFY CROWD
============================================================

After I provide a capture, we will begin with candidate high-frequency
graphics groups.

I will disable ONE candidate at a time.

If:

crowd disappears

while:

fighters remain
octagon remains
cage remains
HUD remains

then mark that signature as a CROWD candidate.

Do not assume high draw count automatically means crowd.

============================================================
PHASE 13 — IDENTIFY SHADOW PASSES
============================================================

Look for groups exhibiting characteristics such as:

depth-only rendering

repeated scene geometry

shadow-sized depth targets

minimal/no color outputs

repeated fighter/arena geometry

But do NOT automatically disable them.

I will manually verify candidate groups.

Also determine separately whether UFC 6/Kyty uses:

raster shadow maps

BVH/ray operations

or both.

Do not assume all visible shadows are ray traced.

============================================================
PHASE 14 — IDENTIFY FOG / VOLUMETRICS
============================================================

The arena contains a large blue atmospheric haze/fog effect.

Potential workload may appear as:

compute dispatches

low-resolution render targets

3D/array textures

full-screen composition

temporal/history buffers

or some combination.

Capture enough metadata to identify likely candidates.

Do not search merely for a blue output color.

Do not disable candidate compute workloads until their dependencies are
understood.

============================================================
PHASE 15 — PROTECT CRITICAL GROUPS
============================================================

Once manually identified, maintain a diagnostic classification table:

FIGHTER_1
FIGHTER_2
OCTAGON
CAGE
HUD
REFEREE
CROWD
ARENA
SHADOW
FOG
POSTPROCESS
UNKNOWN

Critical groups:

fighters
octagon
cage
HUD

must never be included in the eventual UltraLow removal list.

============================================================
PHASE 16 — PERFORMANCE DATA
============================================================

For each capture window, where already available without introducing
heavy synchronization, report:

frames captured
graphics draws/frame
compute dispatches/frame
vertices/indices/frame
instances/frame

Also report any existing frame timing information.

Do not fake GPU cost estimates.

============================================================
PHASE 17 — KEEP INSTRUMENTATION LIGHT
============================================================

The instrumentation itself must not significantly destroy performance.

Do not:

allocate strings per draw
flush files per draw
lock a global mutex unnecessarily per draw
perform expensive formatting per draw

Prefer compact in-memory counters/maps and emit the summary only after
the capture window.

============================================================
PHASE 18 — DO NOT MIX OTHER OPTIMIZATIONS
============================================================

For this commit DO NOT implement:

permanent crowd removal
permanent fog removal
permanent shadow removal
ray tracing disable
800x600 internal rendering
shader simplification
memory allocator changes
CommandScheduler changes

This commit is specifically:

GPU workload capture
+
group classification support
+
diagnostic selective suppression

============================================================
BUILD
============================================================

Build Release after implementation.

Fix compilation errors.

DO NOT RUN TEST CASES.

DO NOT automatically launch UFC 6.

I will manually run UFC 6.

============================================================
SUGGESTED COMMIT
============================================================

Commit:

debug: add UFC6 GPU workload capture and group isolation

============================================================
FINAL REPORT
============================================================

Report:

1. Files changed

2. Central draw path instrumented

3. Central dispatch path instrumented

4. How graphics group IDs are generated

5. How compute group IDs are generated

6. Exact command/configuration required to start a capture

7. Exact command/configuration required to disable graphics groups

8. Exact command/configuration required to experimentally disable
   compute groups

9. Where the capture summary is written

10. Any expected instrumentation overhead

11. Build result

12. Commit hash

Explicitly state:

Automated tests:
NOT RUN — per user instruction

Runtime UFC 6 testing:
NOT RUN — user will manually test