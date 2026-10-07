TASK: UFC 6 — True Shadow Removal + Arena Fog/Volumetric Removal +
Ultra-Low Lighting Mode

Game:
EA SPORTS UFC 6
Title ID: PPSA23566
Version: 01.004.000

Hardware:

CPU:
AMD Ryzen 5 5600H
6 cores / 12 threads

GPU:
NVIDIA RTX 3050 Laptop GPU
4 GB VRAM

RAM:
16 GB DDR4-3200

OS:
Windows 11

Graphics API:
Vulkan

============================================================
CURRENT STATUS
============================================================

UFC 6 is currently running successfully.

The game reaches actual fights without crashing.

Current performance is approximately:

5 FPS

This is already an improvement over the previous ~3 FPS baseline.

I have already identified/suppressed shaders/groups associated with:

- crowd
- unnecessary arena models
- other nonessential visual objects

IMPORTANT:

Do NOT undo or interfere with those existing optimizations.

However, some previous removals may only make objects invisible rather
than preventing their draw workload.

Do not expand this task into a complete crowd-removal rewrite unless
required for correctness.

The focus of THIS stage is:

1. TRUE SHADOW WORKLOAD ELIMINATION
2. BLUE ARENA HAZE / FOG / VOLUMETRIC EFFECT ELIMINATION
3. OPTIONAL ULTRA-LOW / UNLIT MATERIAL LIGHTING

============================================================
PRIMARY OBJECTIVE
============================================================

The final UFC 6 UltraLow fight scene should prioritize:

KEEP:

Fighter 1
Fighter 2
fighter geometry
fighter animation
fighter base textures
face/tattoos
shorts/gloves
octagon
cage
HUD
basic arena shell

REMOVE / REDUCE:

shadows
shadow generation
shadow sampling
ray-traced shadow work if actually present
arena blue atmospheric haze
fog
volumetric lighting
atmospheric scattering
unnecessary arena lighting
complex PBR material lighting
IBL
complex specular
complex roughness
skin scattering
wetness
other expensive cosmetic lighting

Performance is much more important than visual fidelity.

However:

fighters must remain visible and recognizable.

============================================================
CRITICAL DESIGN RULE
============================================================

INVISIBLE != NOT RENDERED.

Do NOT solve this task merely by changing the final fragment output.

For example, this is NOT sufficient:

Shadow generated
    ↓
shadow resources created
    ↓
shadow scene rendered
    ↓
shadow sampled
    ↓
final shader ignores shadow

That still performs most of the expensive work.

Likewise:

Volumetric fog generated
    ↓
compute dispatch
    ↓
temporal filtering
    ↓
upsampling
    ↓
final composition ignores fog

is NOT the desired optimization.

Where technically safe, eliminate expensive work BEFORE execution.

Desired:

identified optional shadow/fog workload
             ↓
       UFC6 UltraLow?
         /       \
       YES        NO
        ↓          ↓
      SKIP       NORMAL
        ↓
provide neutral result/state if required
        ↓
continue rendering correctly

============================================================
IMPORTANT — DO NOT ASSUME SHADOWS ARE RAY TRACED
============================================================

UFC 6 may use ray-tracing functionality on PS5.

However:

DO NOT assume the shadows visible in Kyty are hardware ray-traced
shadows on the RTX 3050.

First determine exactly how this Kyty revision produces UFC 6 shadows.

Possible implementations include:

- rasterized shadow maps
- depth-only shadow passes
- screen-space shadows
- BVH/ray intersection operations
- software/fallback ray traversal
- synthetic/faked ray results
- combination of techniques

Prove which path is actually executing.

============================================================
PHASE 1 — ESTABLISH CURRENT BASELINE
============================================================

Before changing anything:

git status
git branch --show-current
git rev-parse HEAD

Record the current commit.

Do NOT reset existing UFC 6 optimizations.

Current manual performance baseline supplied by user:

approximately 5 FPS.

Do NOT automatically benchmark the game.

I will perform runtime testing.

============================================================
PHASE 2 — AUDIT SHADOW GENERATION
============================================================

Trace UFC 6 shadow-related GPU workloads.

Use the existing draw/dispatch capture infrastructure if available.

Look for characteristics such as:

- depth-only render passes
- repeated scene geometry
- no color attachment
- shadow-sized depth attachments
- repeated fighter geometry
- repeated arena geometry
- repeated cage geometry
- directional-light depth rendering
- spot-light depth rendering
- shadow atlas resources
- depth-array resources
- cube depth resources

Also inspect shader/recompiler handling for:

IMAGE_BVH_INTERSECT_RAY
BVH
INTERSECT_RAY
ray
ray query
acceleration structure

And Vulkan features/extensions such as:

VK_KHR_ray_query
VK_KHR_ray_tracing_pipeline
VK_KHR_acceleration_structure

Do NOT assume their presence means UFC 6 actually uses them.

============================================================
PHASE 3 — DETERMINE THE ACTUAL SHADOW PATH
============================================================

Explicitly report whether UFC 6 shadows in this revision are:

A. raster shadow maps

B. ray/BVH based

C. screen-space

D. hybrid

E. something else

If multiple shadow systems exist, identify each separately.

Do not implement the removal until this is understood.

============================================================
PHASE 4 — TRUE RASTER SHADOW REMOVAL
============================================================

If shadows are generated through identifiable raster shadow passes,
prevent confirmed shadow workload from reaching actual Vulkan draw
execution.

Conceptually:

Guest draw
    ↓
Kyty command processing
    ↓
classify render pass/draw
    ↓
confirmed UFC6 shadow caster?
       |
       +--- YES + UltraLow
       |          ↓
       |       DROP DRAW
       |
       +--- NO
                  ↓
              vkCmdDraw*
                  ↓
                GPU

Prefer suppression BEFORE:

vkCmdDraw
vkCmdDrawIndexed
vkCmdDrawIndirect
vkCmdDrawIndexedIndirect

where renderer state semantics allow it.

============================================================
PHASE 5 — DO NOT USE SHADER HASH ALONE
============================================================

Do NOT identify shadow draws using only one shader hash unless proven
unique.

Use a robust signature where possible.

Potential components:

VS identity
FS identity
pipeline identity
depth-only state
color attachment count
depth attachment characteristics
render target dimensions
primitive topology
resource signature
pass characteristics

We must not accidentally remove:

fighter main rendering
octagon
cage
HUD
required depth prepass

because they share some shader/pipeline component.

============================================================
PHASE 6 — DISTINGUISH SHADOW MAPS FROM DEPTH PREPASS
============================================================

This is critical.

Do NOT assume:

DEPTH ONLY == SHADOW.

A normal camera depth prepass may also render depth without color.

Determine whether the depth target corresponds to:

camera depth

or:

shadow/light depth.

Do not remove the primary camera depth buffer or required depth prepass.

============================================================
PHASE 7 — SHADOW RECEIVING
============================================================

After shadow generation is disabled, identify main material shaders
that sample shadow resources.

Do not leave them dependent on undefined/stale shadow textures.

For UltraLow mode, make shadow visibility resolve to:

FULLY LIT / NO SHADOW

where technically appropriate.

Conceptually:

shadow visibility = 1.0

rather than:

sample shadow map
compare depth
filter PCF
evaluate shadow

But implement this according to the actual shader representation.

Do NOT blindly edit arbitrary SPIR-V instructions.

============================================================
PHASE 8 — RAY/BVH SHADOW WORK
============================================================

If actual ray/BVH traversal contributes to shadows:

determine how:

IMAGE_BVH_INTERSECT_RAY

is handled in this revision.

If real traversal is performed, investigate an UFC6 UltraLow path that
returns an architecturally valid:

MISS / UN-OCCLUDED

result.

Conceptually:

UFC6 UltraLow
      ↓
shadow ray request
      ↓
skip expensive traversal
      ↓
return valid MISS
      ↓
surface treated as illuminated

DO NOT merely skip the guest instruction if doing so leaves destination
registers/results undefined.

If ray traversal is already skipped/faked in this revision:

DO NOT add redundant RT-disable logic.

Report that RT is already effectively bypassed.

============================================================
PHASE 9 — SHADOW RESOURCE SAVINGS
============================================================

After shadow generation has been safely removed, determine whether
shadow-only resources can also avoid unnecessary work.

Potential resources:

shadow maps
shadow atlases
shadow depth arrays
temporary shadow buffers

However:

DO NOT aggressively remove resource creation in the first patch if
later guest/render code expects those resources to exist.

Correctness first.

It is acceptable initially to allocate a small/neutral resource while
skipping expensive rendering.

============================================================
PHASE 10 — IDENTIFY BLUE ARENA HAZE
============================================================

The UFC 6 arena contains a clearly visible blue atmospheric haze/fog
over and behind the crowd.

This is the effect we want to remove.

Do NOT confuse this with normal fighter illumination.

Determine what generates it.

Potential implementations:

volumetric fog
volumetric lighting
atmospheric scattering
light scattering
aerial perspective
screen-space fog
particle fog
bloom + lighting
full-screen atmospheric composition
Frostbite-specific volumetric system

Do not assume which one it is.

============================================================
PHASE 11 — FIND FOG/VOLUMETRIC GPU WORK
============================================================

Search source/debug information where useful for:

fog
volume
volumetric
haze
mist
atmosphere
scattering
froxel
aerial
light scattering

But shader names may not survive translation.

Therefore also inspect GPU workloads.

Look for:

compute dispatches
3D textures
array textures
low-resolution intermediate targets
temporal/history buffers
depth sampling
lighting-volume resources
full-screen composition passes
upsampling passes

Use existing draw/dispatch capture data where possible.

============================================================
PHASE 12 — TRUE FOG/VOLUMETRIC REMOVAL
============================================================

Do NOT merely make the final fog composition transparent.

If positively identified and optional:

skip expensive fog generation work.

Potential sequence:

NORMAL:

depth
 ↓
volume construction
 ↓
light injection
 ↓
scattering
 ↓
temporal filtering
 ↓
upsample
 ↓
composition

ULTRALOW:

detect optional UFC6 fog workload
 ↓
skip expensive generation
 ↓
provide neutral no-fog resource/result if required
 ↓
skip/reduce composition
 ↓
continue rendering

============================================================
PHASE 13 — COMPUTE DISPATCH SAFETY
============================================================

Be much more conservative when skipping compute than graphics draws.

Do NOT blindly skip a compute shader because disabling it makes the
blue fog disappear.

First determine whether its output is used by:

skinning
animation
culling
indirect drawing
lighting
resource conversion
depth processing
other required rendering

Only skip compute workloads positively identified as optional
fog/volumetric processing.

============================================================
PHASE 14 — SIMPLE / UNLIT MATERIAL MODE
============================================================

After shadow and fog paths are understood, implement an OPTIONAL:

UFC6_SIMPLE_LIGHTING

or equivalent UltraLow mode.

The objective is NOT:

make everything black.

The objective is approximately:

FinalColor =
    BaseTexture * ConstantBrightness

or:

BaseTexture
+
very cheap diffuse illumination

according to what integrates safely with the existing shader
recompiler.

============================================================
PHASE 15 — FIGHTER VISIBILITY IS CRITICAL
============================================================

Fighters must remain recognizable.

Preserve:

base/albedo texture
skin color
face
tattoos
shorts
gloves
hair visibility
geometry
skinning
animation
depth

Lighting quality can be heavily reduced.

============================================================
PHASE 16 — REDUCE EXPENSIVE MATERIAL LIGHTING
============================================================

Where positively identified and safe, UltraLow may eliminate/simplify:

complex BRDF
multiple dynamic light evaluation
IBL
environment reflections
complex specular
texture-driven roughness
detail normals
micro normals
skin subsurface scattering
sweat/wetness
secondary material layers

Do NOT assume all these features exist.

Only modify features actually identified in UFC 6 shaders.

============================================================
PHASE 17 — DO NOT MODIFY VERTEX/SKINNING LOGIC
============================================================

Be extremely conservative with vertex shaders.

Do NOT remove:

bone transforms
skinning
vertex position transforms
morph/deformation
UV generation
depth position
required interpolants

Focus primarily on fragment/material lighting.

============================================================
PHASE 18 — THREE INDEPENDENT SWITCHES
============================================================

Implement the three major optimizations independently.

Conceptually:

UFC6_DISABLE_SHADOWS=0/1

UFC6_DISABLE_ARENA_FOG=0/1

UFC6_SIMPLE_LIGHTING=0/1

Use the repository's existing configuration mechanism where practical.

Do not hardcode all three together.

I need to benchmark each independently.

============================================================
PHASE 19 — REQUIRED BENCHMARK MATRIX
============================================================

I will manually test:

TEST A

Current baseline:

Crowd/nonessential optimizations ON
Shadows normal
Fog normal
Lighting normal

Expected reference:
~5 FPS

----------------------------------

TEST B

Shadows OFF only

----------------------------------

TEST C

Fog OFF only

----------------------------------

TEST D

Simple lighting only

----------------------------------

TEST E

Shadows OFF
Fog OFF

----------------------------------

TEST F

Shadows OFF
Fog OFF
Simple lighting ON

This lets us determine which system actually affects performance.

============================================================
PHASE 20 — DO NOT MIX CROWD CHANGES INTO THIS PATCH
============================================================

Existing crowd/nonessential model optimizations should remain intact.

Do NOT substantially redesign crowd suppression in this task.

Crowd pre-draw elimination can be improved separately.

============================================================
PHASE 21 — DO NOT IMPLEMENT INTERNAL RESOLUTION YET
============================================================

Do NOT implement:

800x600
960x540
dynamic resolution
global VkImage resizing

in this task.

Internal-resolution reduction will be measured separately.

============================================================
PHASE 22 — DO NOT CHANGE MEMORY MANAGEMENT
============================================================

Do NOT modify:

VRAM allocator
system-memory fallback
BufferCache memory policy
TextureCache memory policy

unless absolutely necessary for correctness.

Report memory problems instead.

============================================================
PHASE 23 — DO NOT CHANGE COMMAND SCHEDULER
============================================================

Do NOT optimize:

CommandScheduler
FlushAndWait
Finish
Wait
threading

as part of this task.

Synchronization profiling is a later independent stage.

============================================================
PHASE 24 — PERFORMANCE INSTRUMENTATION
============================================================

Where the existing capture infrastructure makes it cheap, record
before/after workload counts for each feature.

For shadows:

shadow draws/frame
shadow indices/vertices/frame
shadow dispatches/frame

For fog:

fog draws/frame
fog dispatches/frame
workgroups/frame

For simple lighting:

affected shader count
affected draw count

Do NOT claim exact GPU milliseconds unless actual GPU timestamp queries
measure them.

============================================================
PHASE 25 — KEEP LOGGING LIGHTWEIGHT
============================================================

Use existing kyty.txt logging.

Do not:

format strings every draw
write to disk every draw
flush logs every draw
introduce heavy mutex contention

Aggregate counters and emit concise summaries.

============================================================
PHASE 26 — SAFETY FALLBACK
============================================================

Unknown workloads must remain untouched.

Use:

KNOWN SHADOW
      ↓
can disable

KNOWN FOG
      ↓
can disable

KNOWN SAFE MATERIAL
      ↓
can simplify

UNKNOWN
      ↓
ORIGINAL BEHAVIOR

Do not aggressively classify unknown workloads just to maximize removal.

============================================================
PHASE 27 — BUILD REQUIREMENTS
============================================================

After source changes:

build Release using the existing working CMake/Ninja configuration.

Fix compilation errors introduced by this work.

Building is allowed.

============================================================
ABSOLUTE TESTING RULE
============================================================

DO NOT RUN TEST CASES.

Do NOT execute:

ctest
unit tests
integration tests
renderer tests
automated gameplay tests

Do NOT automatically launch UFC 6.

I will manually test the game.

============================================================
GIT STRATEGY
============================================================

Keep the work reversible.

Prefer separate commits.

Suggested:

Commit 1:

perf: add UFC6 shadow suppression

Commit 2:

perf: disable UFC6 arena volumetrics

Commit 3:

perf: add UFC6 simple lighting mode

If the investigation determines one of these features cannot yet be
safely implemented, DO NOT fake the implementation.

Report why.

============================================================
SUCCESS CRITERIA — SHADOWS
============================================================

[ ] actual UFC6 shadow mechanism identified

[ ] shadow maps distinguished from camera depth

[ ] confirmed shadow-generation work prevented from executing

[ ] receiving shaders do not depend on undefined shadow data

[ ] fighters remain visible

[ ] octagon remains visible

[ ] cage remains visible

[ ] HUD remains visible

[ ] no invalid Vulkan state introduced

============================================================
SUCCESS CRITERIA — FOG
============================================================

[ ] blue arena haze source identified

[ ] actual generation workload identified

[ ] optional generation skipped where safe

[ ] neutral resources/results supplied where required

[ ] fighters unaffected

[ ] cage/octagon unaffected

[ ] final composition remains valid

============================================================
SUCCESS CRITERIA — SIMPLE LIGHTING
============================================================

[ ] fighter base textures preserved

[ ] faces remain recognizable

[ ] tattoos remain visible

[ ] shorts/gloves remain visible

[ ] geometry remains correct

[ ] animation remains correct

[ ] expensive material lighting reduced

[ ] objects do not become unintentionally black

============================================================
FINAL REPORT REQUIRED
============================================================

After implementation provide:

1. BASE COMMIT

Exact commit/hash used.

2. SHADOW IMPLEMENTATION

Explain exactly how UFC 6 shadows were being generated.

Raster:
YES/NO

Ray/BVH:
YES/NO

Screen-space:
YES/NO

Hybrid:
YES/NO

Then explain what was actually disabled.

3. SHADOW DRAW REDUCTION

Report measured workload counts if available:

before:
...

after:
...

Do not invent values.

4. RAY TRACING

Explain whether the RTX 3050 was actually executing ray/BVH work for
UFC 6.

If not:

say so explicitly.

5. FOG/VOLUMETRICS

Explain exactly what produced the blue arena haze.

List:

shader/pipeline
draw/dispatch
resources
composition

as far as positively identified.

6. FOG WORKLOAD REDUCTION

Report:

draws removed
dispatches removed
workgroups removed

where measured.

7. SIMPLE LIGHTING

List exactly which material operations were simplified.

8. VISUAL FEATURES PRESERVED

Fighter geometry:
...

Animation:
...

Base texture:
...

Face:
...

Tattoos:
...

Hair:
...

Shorts:
...

Octagon:
...

Cage:
...

HUD:
...

9. FILES CHANGED

For every file:

path
change
reason

10. CONFIGURATION

Give exact instructions for:

shadows ON/OFF
fog ON/OFF
simple lighting ON/OFF

11. BUILD

Configure:
PASS/FAIL

Release build:
PASS/FAIL

Install:
PASS/FAIL/NOT REQUIRED

12. TESTS

Automated test cases:
NOT RUN — per user instruction

UFC 6 runtime:
NOT RUN — user will manually test

13. COMMITS

Provide commit hash + message for each commit.

============================================================
IMPORTANT PERFORMANCE INTERPRETATION
============================================================

Current manually observed performance is approximately:

5 FPS

That corresponds to roughly:

200 ms/frame.

Target:

15 FPS

requires approximately:

66.7 ms/frame.

Therefore we still need to eliminate approximately 133 ms of frame
time to reach 15 FPS.

Do NOT promise that these graphical changes will achieve that.

Their purpose is to determine how much of the remaining ~200 ms frame
time comes from:

shadows
volumetrics
material lighting

If all three are aggressively reduced and FPS barely changes, report
that clearly.

In that situation DO NOT continue destroying visual quality.

The next investigation should instead focus on:

CPU/GPU synchronization
CommandScheduler waits
pipeline/shader compilation
BufferCache synchronization
TextureCache transfers
guest GPU command processing
CPU emulation

============================================================
FINAL PRINCIPLE
============================================================

We are building an extreme UFC 6 performance mode for:

Ryzen 5 5600H
RTX 3050 Laptop 4 GB

Visual fidelity is secondary.

The desired fight scene can be visually simple.

But:

DO NOT make expensive work merely invisible.

When an optional feature is disabled, prevent its expensive GPU
workload from executing as early as safely possible.

KEEP:
fighters
animation
base textures
octagon
cage
HUD

REMOVE/REDUCE:
crowd (existing work)
nonessential models (existing work)
shadows
arena haze/fog
volumetrics
complex lighting
complex material effects

Build Release.

DO NOT RUN TEST CASES.

STOP after the build and provide the report.

I will manually run UFC 6 and provide the FPS, GPU utilization, VRAM,
shared GPU memory, RAM usage, and screenshots.