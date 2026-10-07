#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GPUWORKLOADCAPTURE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GPUWORKLOADCAPTURE_H_

#include <array>
#include <cstdint>

namespace Libs::Graphics {

// Diagnostic only. IDs are FNV-1a hashes of the fields below, never Vulkan handles or guest
// addresses. All calls are made on the renderer's serialized command path.
struct GraphicsWorkload {
	std::array<uint64_t, 4> shader_hashes {};
	std::array<uint32_t, 8> color_formats {};
	uint32_t                topology        = 0;
	uint32_t                color_count     = 0;
	uint32_t                depth_format    = 0;
	uint32_t                depth_flags     = 0;
	uint32_t                depth_layers    = 0;
	uint32_t                width           = 0;
	uint32_t                height          = 0;
	// Diagnostic address for distinguishing attachment uses within one run; excluded from ID.
	uint64_t                depth_address   = 0;
	uint32_t                count           = 0;
	uint32_t                instances       = 0;
	bool                    indexed         = false;
	bool                    indirect        = false;
	bool                    count_known     = true;
	bool                    instances_known = true;
};

struct ComputeWorkload {
	uint64_t                shader_hash = 0;
	std::array<uint32_t, 3> groups {};
	uint32_t                storage_images  = 0;
	uint32_t                written_buffers = 0;
	bool                    indirect        = false;
};

class GpuWorkloadCapture {
public:
	static GpuWorkloadCapture& Instance();
	[[nodiscard]] bool         GraphicsEnabled() const;
	// Keep capture runs on the full path so their workload totals remain comparable.
	[[nodiscard]] bool FastGraphicsEnabled() const;
	[[nodiscard]] bool FastGraphicsGroup(const GraphicsWorkload& work) const;
	[[nodiscard]] bool ComputeEnabled() const;
	// Return true when this diagnostic group should be suppressed.
	bool Graphics(uint64_t frame, const GraphicsWorkload& work);
	bool Compute(uint64_t frame, const ComputeWorkload& work);

private:
	GpuWorkloadCapture();
	struct State;
	State* m_state;
};

} // namespace Libs::Graphics

#endif
