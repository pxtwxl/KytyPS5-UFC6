# UFC 6 GPU workload capture

This is temporary diagnostic instrumentation. It is off unless one of the environment variables
below is set. Run the game manually and reach a fight before triggering the capture.

## Capture a fight window

From PowerShell, before starting `kyty_emulator.exe`:

```powershell
$env:UFC6_CAPTURE_GPU_WORK = '1'
$env:UFC6_CAPTURE_FRAMES = '60'
$env:UFC6_CAPTURE_TRIGGER = 'ufc6-fight.capture'
```

Remove any old trigger file before launching. At the desired fight scene, create that file in the
emulator's working directory (for example, `New-Item -ItemType File 'ufc6-fight.capture'`).
The first draw or dispatch after
the trigger begins a 60 presented-frame window. Alternatively, omit the trigger and set
`UFC6_CAPTURE_FIRST_FRAME` to an absolute presented-frame number. `UFC6_CAPTURE_GPU_WORK=60` is also
accepted as a shorthand for 60 frames.

The normal Kyty log contains `UFC6 GPU WORKLOAD SUMMARY`, one line per graphics or compute group,
and top-ten rankings. For a command-line run, `--printf-direction File --printf-output-file _kyty.txt`
writes the log to `_kyty.txt`. No line is written per draw. The summary is emitted when the next
draw or dispatch arrives after the capture window.

Graphics IDs are 16-digit hexadecimal signatures of the guest vertex/geometry/mesh and pixel shader
hashes, primitive topology, color target count and formats, depth format and test/write state, and
target extent. Compute
IDs are signatures of the guest compute shader hash. They do not contain frame numbers, draw order,
addresses, or Vulkan handles. Graphics groups sharing a shader can be separated by target or topology.
The `shaders=` field contains the three vertex-stage positions followed by the pixel shader; zero
means unused. The `geometry_instances` metric multiplies known vertex/index counts by instances.
GPU-read indirect counts are marked unknown. Rankings describe submitted work, not measured GPU time.
Each graphics line also reports the depth image's layer count and up to eight guest depth target
addresses seen during this capture. Addresses are diagnostic within one run and do not enter the
group ID. Compare the depth targets used by a candidate against the camera depth target before
classifying any depth-only group as a shadow pass.

## Isolate a group

Copy the hexadecimal ID from a `UFC6 GPU GRAPHICS` line, then restart the emulator with:

```powershell
$env:UFC6_DISABLE_GRAPHICS_GROUPS = '0123456789abcdef'
```

Comma-separated IDs are accepted. The selected draws are skipped after shader and render-target
discovery on their first matching state. The renderer then learns the guest shader and target state
for that exact selected group and bypasses later matching draws before shader, index, target, or
resource preparation. A state it has not learned still uses the full group check. Capture runs keep
the full path so their totals remain comparable. Set only one new candidate at a time and inspect
the fight visually. To clear the selection, remove the environment variable.

The first early skip prints `UFC6 GPU early graphics skips=1`; the count is printed again at each
100,000 skips. The original group suppression already prevented Vulkan draw commands, so this
fast path is intended to save CPU preparation work. It does not establish a new GPU time saving.

| Group ID | Visual classification |
|---|---|
| `818528f284512b2a` | Crowd body, confirmed by manual fight comparison |
| `fef7c490f625f599` | Remaining crowd head, confirmed when combined with body group |
| `0a543b500385a6a8` | Depth-only, no visible change when disabled |
| `60424d989e2e12d5` | Depth-only, no visible change when added |
| `9ad7ab644885ef3f` | Depth-only; no noticeable visual change in first trial, but that run ended in a buffer-cache GC abort. FPS effect unconfirmed. |

### Next fight-scene isolation

The saved 60-frame `ufc6-gpu-summary.txt` predates depth-target address logging. Its IDs are
**graphics group IDs**, not individual shader hashes. The first unclassified high-draw candidate
was `9ad7ab644885ef3f`: 15,720 draws (262 per frame), no color attachments, and an 8192x8192
depth attachment. This alone does not prove it is a shadow pass. The manual trial used:

```powershell
$env:UFC6_DISABLE_GRAPHICS_GROUPS = '818528f284512b2a,fef7c490f625f599,0a543b500385a6a8,60424d989e2e12d5,9ad7ab644885ef3f'
Remove-Item Env:UFC6_INTERNAL_RESOLUTION -ErrorAction SilentlyContinue
```

The log confirms the ID was skipped at frame 1938. The user saw no noticeable visual change,
reported low FPS, then hit `BufferCache::RunGarbageCollector` at `bufferCache.cpp:1150`. That
abort occurs when a page remains marked GPU-dirty after its byte ranges have already been
queued for readback. The collector now waits for a pending readback or clears the stale page
state; no image scaling code was restored. The FPS effect of this candidate remains unproven,
and the next run should omit it.

First verify that the updated emulator reaches a stable fight with the four previously used IDs:

```powershell
$env:UFC6_DISABLE_GRAPHICS_GROUPS = '818528f284512b2a,fef7c490f625f599,0a543b500385a6a8,60424d989e2e12d5'
Remove-Item Env:UFC6_INTERNAL_RESOLUTION -ErrorAction SilentlyContinue
```

The Release rebuild invalidates the shader cache, so compare FPS only after the fight has
settled and repeat the same scene with the next candidate. The capture records draw counts,
not measured GPU time.

The next **unclassified** depth-only candidate is `d253a4b3f3e3b126` (9,120 draws in the
capture). For its trial, restart the emulator with the four established IDs plus only this new
ID:

```powershell
$env:UFC6_DISABLE_GRAPHICS_GROUPS = '818528f284512b2a,fef7c490f625f599,0a543b500385a6a8,60424d989e2e12d5,d253a4b3f3e3b126'
```

Verify `UFC6 GPU graphics group first skipped: id=d253a4b3f3e3b126` in the new `_kyty.txt`.
Compare the same fight scene with the four-ID setting before attributing an FPS difference to
the group. Later candidates are `fd4d9948114b319f` (7,200 draws) and
`8979570cac27d7c6` (6,913 draws). Try one at a time; no extra group is enabled by default.

Compute suppression can break later draws or resource dependencies. It requires both variables:

```powershell
$env:UFC6_DISABLE_COMPUTE_GROUPS = '0123456789abcdef'
$env:UFC6_EXPERIMENTAL_DISABLE_COMPUTE = '1'
```

These IDs are examples only. Use IDs from your own capture. Keep the capture and the visual result
together when classifying fighters, octagon, cage, HUD, crowd, referee, arena, shadows, fog, and
post-processing. You can record known graphics classes for later captures with, for example,
`$env:UFC6_GRAPHICS_CLASSES = '0123456789abcdef:CROWD,1111111111111111:FIGHTER_1'`.
The labels appear as `UFC6 GPU CLASS` lines. No category is disabled by default.

## UFC 6 feature switches (classification required)

These switches only act while the loaded title ID is `PPSA23566`. They have no built-in group IDs.
The candidate groups in the table above are **not confirmed shadow maps**. Depth-only rendering
alone does not distinguish a light's shadow map from the camera depth prepass.

After a fight capture and visual/resource classification, provide confirmed 16-digit graphics IDs:

```powershell
$env:UFC6_SHADOW_GRAPHICS_GROUPS = '0123456789abcdef,1111111111111111'
$env:UFC6_DISABLE_SHADOWS = '1'
```

The shadow selector skips only depth-writing draws with no color attachment. It prevents their
Vulkan draw commands, but currently does not neutralize shadow sampling by receiving shaders.
Treat this as an experimental generation switch and inspect the resulting image for stale shadows.
Remove `UFC6_DISABLE_SHADOWS` (or set it to `0`) to restore normal behavior.

For confirmed optional arena fog graphics groups:

```powershell
$env:UFC6_FOG_GRAPHICS_GROUPS = '0123456789abcdef,1111111111111111'
$env:UFC6_DISABLE_ARENA_FOG = '1'
```

For confirmed optional fog compute groups, also set:

```powershell
$env:UFC6_FOG_COMPUTE_GROUPS = '0123456789abcdef'
$env:UFC6_EXPERIMENTAL_DISABLE_COMPUTE = '1'
```

Compute suppression does not supply neutral output resources yet. Only classify a dispatch after
checking its output consumers; otherwise it could break later rendering. Remove
`UFC6_DISABLE_ARENA_FOG` (or set it to `0`) to restore normal behavior. No simple-lighting switch
exists yet because fighter material shaders and their base-texture path have not been identified.
Replace every example ID above with a group ID from your classified fight capture.
The log reports skipped shadow draws, fog draws, fog dispatches, and known fog workgroups. A capture
summary includes these counts, and non-capture runs log the first skip and each 100,000th skip.
