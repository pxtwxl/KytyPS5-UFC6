#include "graphics/host_gpu/renderer/gpuWorkloadCapture.h"

#include "common/logging/log.h"
#include "loader/systemContent.h"

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Libs::Graphics {
namespace {

uint64_t Mix(uint64_t hash, uint64_t value) {
	for (uint32_t i = 0; i < 8; ++i) {
		hash = (hash ^ (value & 0xffu)) * 1099511628211ull;
		value >>= 8u;
	}
	return hash;
}

uint64_t GraphicsId(const GraphicsWorkload& work) {
	uint64_t hash = Mix(14695981039346656037ull, 0x4752415048494353ull);
	for (const auto shader: work.shader_hashes)
		hash = Mix(hash, shader);
	hash = Mix(hash, work.topology);
	hash = Mix(hash, work.color_count);
	for (uint32_t i = 0; i < work.color_count; ++i)
		hash = Mix(hash, work.color_formats[i]);
	hash = Mix(hash, work.depth_format);
	hash = Mix(hash, work.depth_flags);
	hash = Mix(hash, work.width);
	hash = Mix(hash, work.height);
	return hash;
}

uint64_t ComputeId(const ComputeWorkload& work) {
	return Mix(Mix(14695981039346656037ull, 0x434f4d5055544520ull), work.shader_hash);
}

std::unordered_set<uint64_t> ParseIds(const char* name) {
	std::unordered_set<uint64_t> ids;
	const char*                  value = std::getenv(name);
	if (value == nullptr) return ids;
	while (*value != '\0') {
		char*      end = nullptr;
		const auto id  = std::strtoull(value, &end, 16);
		if (end != value) ids.insert(id);
		value = end != value ? end : value + 1;
		while (*value != '\0' && *value != ',')
			++value;
		if (*value == ',') ++value;
	}
	return ids;
}

std::unordered_map<uint64_t, std::string> ParseClasses() {
	std::unordered_map<uint64_t, std::string> classes;
	const char*                               value = std::getenv("UFC6_GRAPHICS_CLASSES");
	if (value == nullptr) return classes;
	while (*value != '\0') {
		char*      end = nullptr;
		const auto id  = std::strtoull(value, &end, 16);
		if (end != value && *end == ':') {
			const char* first = ++end;
			while (std::isalnum(static_cast<unsigned char>(*end)) || *end == '_')
				++end;
			if (end != first)
				classes.emplace(id, std::string(first, static_cast<size_t>(end - first)));
		}
		value = end != value ? end : value + 1;
		while (*value != '\0' && *value != ',')
			++value;
		if (*value == ',') ++value;
	}
	return classes;
}

uint64_t ReadNumber(const char* name, uint64_t fallback) {
	const char* value = std::getenv(name);
	if (value == nullptr || *value == '\0') return fallback;
	char*      end    = nullptr;
	const auto number = std::strtoull(value, &end, 10);
	return end != value && *end == '\0' ? number : fallback;
}

struct GraphicsStats {
	GraphicsWorkload sample;
	std::unordered_set<uint64_t> depth_addresses;
	uint64_t         draws             = 0;
	uint64_t         indexed           = 0;
	uint64_t         indirect          = 0;
	uint64_t         elements          = 0;
	uint64_t         geometry          = 0;
	uint64_t         instances         = 0;
	uint32_t         max_count         = 0;
	uint64_t         unknown_counts    = 0;
	uint64_t         unknown_instances = 0;
};

struct ComputeStats {
	ComputeWorkload         sample;
	uint64_t                dispatches     = 0;
	uint64_t                indirect       = 0;
	uint64_t                workgroups     = 0;
	uint64_t                unknown_groups = 0;
	std::array<uint32_t, 3> max_groups {};
};

template <typename Stats, typename Score>
void LogRanking(const char* title, const std::unordered_map<uint64_t, Stats>& groups, Score score) {
	std::vector<std::pair<uint64_t, uint64_t>> ordered;
	ordered.reserve(groups.size());
	for (const auto& [id, stat]: groups)
		ordered.emplace_back(id, score(stat));
	std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
		return a.second != b.second ? a.second > b.second : a.first < b.first;
	});
	LOGF("UFC6 GPU %s:", title);
	for (size_t i = 0; i < std::min<size_t>(ordered.size(), 10); ++i) {
		LOGF(" %016" PRIx64 "=%" PRIu64, ordered[i].first, ordered[i].second);
	}
	LOGF("\n");
}

} // namespace

struct GpuWorkloadCapture::State {
	const uint64_t frame_count = [] {
		const auto setting = ReadNumber("UFC6_CAPTURE_GPU_WORK", 0);
		return setting == 1 ? ReadNumber("UFC6_CAPTURE_FRAMES", 60) : setting;
	}();
	const uint64_t                     first_frame = ReadNumber("UFC6_CAPTURE_FIRST_FRAME", 0);
	const char*                        trigger     = std::getenv("UFC6_CAPTURE_TRIGGER");
	const std::unordered_set<uint64_t> disabled_graphics = ParseIds("UFC6_DISABLE_GRAPHICS_GROUPS");
	const std::unordered_set<uint64_t> disabled_compute  = ParseIds("UFC6_DISABLE_COMPUTE_GROUPS");
	const bool ufc6 = [] {
		std::string title_id;
		return Loader::SystemContentParamSfoGetString("TITLE_ID", &title_id) &&
		       title_id == "PPSA23566";
	}();
	const bool disable_shadows = ufc6 && ReadNumber("UFC6_DISABLE_SHADOWS", 0) == 1;
	const bool disable_fog = ufc6 && ReadNumber("UFC6_DISABLE_ARENA_FOG", 0) == 1;
	const std::unordered_set<uint64_t> shadow_graphics = ParseIds("UFC6_SHADOW_GRAPHICS_GROUPS");
	const std::unordered_set<uint64_t> fog_graphics = ParseIds("UFC6_FOG_GRAPHICS_GROUPS");
	const std::unordered_set<uint64_t> fog_compute = ParseIds("UFC6_FOG_COMPUTE_GROUPS");
	const std::unordered_map<uint64_t, std::string> graphics_classes = ParseClasses();
	const bool experimental_compute = ReadNumber("UFC6_EXPERIMENTAL_DISABLE_COMPUTE", 0) == 1;
	std::unordered_set<uint64_t> logged_disabled_graphics;
	std::unordered_map<uint64_t, GraphicsStats> graphics;
	std::unordered_map<uint64_t, ComputeStats>  compute;
	uint64_t                                    start                 = UINT64_MAX;
	uint64_t                                    checked_trigger_frame = UINT64_MAX;
	bool                                        finished              = false;
	bool                                        trigger_seen          = false;
	uint64_t                                    shadow_draws           = 0;
	uint64_t                                    fog_draws              = 0;
	uint64_t                                    fog_dispatches         = 0;
	uint64_t                                    capture_shadow_draws   = 0;
	uint64_t                                    capture_fog_draws      = 0;
	uint64_t                                    capture_fog_dispatches = 0;
	uint64_t                                    capture_fog_workgroups = 0;
	uint64_t                                    capture_fog_indirect   = 0;

	State() {
		if (frame_count != 0) {
			LOGF("UFC6 GPU capture armed: frames=%" PRIu64 " first=%" PRIu64 " trigger=%s\n",
			     frame_count, first_frame, trigger != nullptr ? trigger : "none");
		}
		if (!disabled_graphics.empty()) {
			LOGF("UFC6 GPU graphics suppression armed: %zu signatures\n", disabled_graphics.size());
			std::vector<uint64_t> ids(disabled_graphics.begin(), disabled_graphics.end());
			std::sort(ids.begin(), ids.end());
			for (const auto id: ids)
				LOGF("UFC6 GPU graphics suppression ID=%016" PRIx64 "\n", id);
		}
		if (!disabled_compute.empty()) {
			LOGF("UFC6 GPU compute suppression %s: %zu signatures; skipped outputs may break later "
			     "work\n",
			     experimental_compute ? "EXPERIMENTAL armed"
			                          : "ignored (set UFC6_EXPERIMENTAL_DISABLE_COMPUTE=1)",
			     disabled_compute.size());
		}
		if (disable_shadows) {
			LOGF("UFC6 shadow draw suppression armed: %zu configured signatures; "
			     "shadow receiving is not neutralized\n", shadow_graphics.size());
		}
		if (disable_fog) {
			LOGF("UFC6 arena fog suppression armed: %zu graphics, %zu compute signatures; "
			     "compute requires UFC6_EXPERIMENTAL_DISABLE_COMPUTE=1\n",
			     fog_graphics.size(), fog_compute.size());
		}
	}

	void Summary() {
		finished                  = true;
		uint64_t total_draws      = 0;
		uint64_t total_elements   = 0;
		uint64_t total_geometry   = 0;
		uint64_t total_instances  = 0;
		uint64_t total_dispatches = 0;
		for (const auto& [id, stat]: graphics) {
			total_draws += stat.draws;
			total_elements += stat.elements;
			total_geometry += stat.geometry;
			total_instances += stat.instances;
		}
		for (const auto& [id, stat]: compute)
			total_dispatches += stat.dispatches;
		LOGF("UFC6 GPU WORKLOAD SUMMARY: frames=%" PRIu64 " start=%" PRIu64
		     " graphics_groups=%zu compute_groups=%zu draws=%" PRIu64
		     " draws_per_frame=%.2f elements=%" PRIu64 " geometry_instances=%" PRIu64
		     " instances=%" PRIu64 " dispatches=%" PRIu64 " dispatches_per_frame=%.2f\n",
		     frame_count, start, graphics.size(), compute.size(), total_draws,
		     static_cast<double>(total_draws) / frame_count, total_elements, total_geometry,
		     total_instances, total_dispatches,
		     static_cast<double>(total_dispatches) / frame_count);
		for (const auto& [id, stat]: graphics) {
			const auto& w = stat.sample;
			LOGF("UFC6 GPU GRAPHICS %016" PRIx64 " draws=%" PRIu64 " indexed=%" PRIu64
			     " nonindexed=%" PRIu64 " indirect=%" PRIu64 " elements=%" PRIu64
			     " geometry_instances=%" PRIu64 " instances=%" PRIu64
			     " avg_count=%.2f max_count=%u unknown_counts=%" PRIu64
			     " unknown_instances=%" PRIu64 " topology=%u depth_flags=%u shaders=%016" PRIx64
			     ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64
			     " colors=%u depth=%u depth_layers=%u extent=%ux%u formats=",
			     id, stat.draws, stat.indexed, stat.draws - stat.indexed, stat.indirect,
			     stat.elements, stat.geometry, stat.instances,
			     stat.draws == stat.unknown_counts
			         ? 0.0
			         : static_cast<double>(stat.elements) / (stat.draws - stat.unknown_counts),
			     stat.max_count, stat.unknown_counts, stat.unknown_instances, w.topology,
			     w.depth_flags, w.shader_hashes[0], w.shader_hashes[1], w.shader_hashes[2],
			     w.shader_hashes[3], w.color_count, w.depth_format, w.depth_layers,
			     w.width, w.height);
			for (uint32_t i = 0; i < w.color_count; ++i)
				LOGF("%s%u", i == 0 ? "" : ",", w.color_formats[i]);
			LOGF(" depth_targets=%zu", stat.depth_addresses.size());
			size_t shown = 0;
			for (uint64_t address: stat.depth_addresses) {
				if (shown++ == 8) break;
				LOGF(" %016" PRIx64, address);
			}
			LOGF("\n");
		}
		for (const auto& [id, stat]: compute) {
			LOGF("UFC6 GPU COMPUTE %016" PRIx64 " dispatches=%" PRIu64 " indirect=%" PRIu64
			     " workgroups=%" PRIu64 " unknown_groups=%" PRIu64
			     " max_groups=%ux%ux%u shader=%016" PRIx64
			     " storage_images=%u written_buffers=%u\n",
			     id, stat.dispatches, stat.indirect, stat.workgroups, stat.unknown_groups,
			     stat.max_groups[0], stat.max_groups[1], stat.max_groups[2],
			     stat.sample.shader_hash, stat.sample.storage_images, stat.sample.written_buffers);
		}
		for (const auto& [id, label]: graphics_classes) {
			LOGF("UFC6 GPU CLASS %016" PRIx64 " %s\n", id, label.c_str());
		}
		LogRanking("HIGH DRAW COUNT", graphics, [](const auto& s) { return s.draws; });
		LogRanking("HIGH GEOMETRY WORKLOAD", graphics, [](const auto& s) { return s.geometry; });
		LogRanking("HIGH INSTANCE COUNT", graphics, [](const auto& s) { return s.instances; });
		LogRanking("HIGH DISPATCH COUNT", compute, [](const auto& s) { return s.dispatches; });
		LogRanking("HIGH COMPUTE WORKLOAD", compute, [](const auto& s) { return s.workgroups; });
		LOGF("UFC6 feature skips in capture: shadow_draws=%" PRIu64 " fog_draws=%" PRIu64
		     " fog_dispatches=%" PRIu64 " fog_workgroups=%" PRIu64
		     " fog_indirect_unknown_workgroups=%" PRIu64 "\n",
		     capture_shadow_draws, capture_fog_draws, capture_fog_dispatches,
		     capture_fog_workgroups, capture_fog_indirect);
		LOGF("UFC6 GPU END SUMMARY (counts are submitted work, not GPU time)\n");
	}

	bool Capturing(uint64_t frame) {
		if (frame_count == 0 || finished || frame < first_frame) return false;
		if (start == UINT64_MAX) {
			if (trigger != nullptr && !trigger_seen && checked_trigger_frame != frame) {
				checked_trigger_frame = frame;
				if (FILE* file = std::fopen(trigger, "rb"); file != nullptr) {
					std::fclose(file);
					trigger_seen = true;
				}
			}
			if (trigger != nullptr && !trigger_seen) return false;
			start = frame;
			LOGF("UFC6 GPU capture started: frame=%" PRIu64 " frames=%" PRIu64 "\n", start,
			     frame_count);
		}
		if (frame - start < frame_count) return true;
		Summary();
		return false;
	}
};

GpuWorkloadCapture::GpuWorkloadCapture(): m_state(new State) {}

GpuWorkloadCapture& GpuWorkloadCapture::Instance() {
	static GpuWorkloadCapture capture;
	return capture;
}

bool GpuWorkloadCapture::GraphicsEnabled() const {
	return m_state->frame_count != 0 || !m_state->disabled_graphics.empty() ||
	       (m_state->disable_shadows && !m_state->shadow_graphics.empty()) ||
	       (m_state->disable_fog && !m_state->fog_graphics.empty());
}

bool GpuWorkloadCapture::FastGraphicsEnabled() const {
	return m_state->frame_count == 0 && !m_state->disabled_graphics.empty();
}

bool GpuWorkloadCapture::FastGraphicsGroup(const GraphicsWorkload& work) const {
	// Feature skips must pass through their guards and counters every time. Only the existing
	// diagnostic selector may teach the early CPU skip cache.
	if (!FastGraphicsEnabled()) return false;
	const auto id = GraphicsId(work);
	const bool shadow = m_state->disable_shadows && work.color_count == 0 &&
	                    work.depth_format != 0 && (work.depth_flags & 2u) != 0 &&
	                    m_state->shadow_graphics.contains(id);
	const bool fog = m_state->disable_fog && m_state->fog_graphics.contains(id);
	return !shadow && !fog && m_state->disabled_graphics.contains(id);
}

bool GpuWorkloadCapture::ComputeEnabled() const {
	return m_state->frame_count != 0 ||
	       (m_state->experimental_compute &&
	        (!m_state->disabled_compute.empty() ||
	         (m_state->disable_fog && !m_state->fog_compute.empty())));
}

bool GpuWorkloadCapture::Graphics(uint64_t frame, const GraphicsWorkload& work) {
	auto& state = *m_state;
	if (!GraphicsEnabled()) return false;
	const auto id = GraphicsId(work);
	const bool capturing = state.Capturing(frame);
	if (capturing) {
		auto [it, added] = state.graphics.try_emplace(id);
		if (added) it->second.sample = work;
		auto& stat = it->second;
		++stat.draws;
		if (work.depth_address != 0) stat.depth_addresses.insert(work.depth_address);
		stat.indexed += work.indexed;
		stat.indirect += work.indirect;
		if (work.instances_known)
			stat.instances += work.instances;
		else
			++stat.unknown_instances;
		if (work.count_known) {
			stat.elements += work.count;
			if (work.instances_known)
				stat.geometry += static_cast<uint64_t>(work.count) * work.instances;
			stat.max_count = std::max(stat.max_count, work.count);
		} else {
			++stat.unknown_counts;
		}
	}
	if (state.disable_shadows && work.color_count == 0 && work.depth_format != 0 &&
	    (work.depth_flags & 2u) != 0 && state.shadow_graphics.contains(id)) {
		if (capturing) ++state.capture_shadow_draws;
		if (++state.shadow_draws == 1 || state.shadow_draws % 100000 == 0)
			LOGF("UFC6 shadow draws skipped=%" PRIu64 "\n", state.shadow_draws);
		return true;
	}
	if (state.disable_fog && state.fog_graphics.contains(id)) {
		if (capturing) ++state.capture_fog_draws;
		if (++state.fog_draws == 1 || state.fog_draws % 100000 == 0)
			LOGF("UFC6 fog draws skipped=%" PRIu64 "\n", state.fog_draws);
		return true;
	}
	if (!state.disabled_graphics.contains(id)) return false;
	if (state.logged_disabled_graphics.insert(id).second) {
		LOGF("UFC6 GPU graphics group first skipped: id=%016" PRIx64 " frame=%" PRIu64 "\n",
		     id, frame);
	}
	return true;
}

bool GpuWorkloadCapture::Compute(uint64_t frame, const ComputeWorkload& work) {
	auto& state = *m_state;
	if (!ComputeEnabled())
		return false;
	const auto id = ComputeId(work);
	const bool capturing = state.Capturing(frame);
	if (capturing) {
		auto [it, added] = state.compute.try_emplace(id);
		if (added) it->second.sample = work;
		auto& stat = it->second;
		++stat.dispatches;
		stat.indirect += work.indirect;
		if (work.indirect) {
			++stat.unknown_groups;
		} else {
			stat.workgroups +=
			    static_cast<uint64_t>(work.groups[0]) * work.groups[1] * work.groups[2];
			for (uint32_t i = 0; i < 3; ++i)
				stat.max_groups[i] = std::max(stat.max_groups[i], work.groups[i]);
		}
	}
	if (state.disable_fog && state.experimental_compute && state.fog_compute.contains(id)) {
		++state.fog_dispatches;
		if (capturing) ++state.capture_fog_dispatches;
		if (capturing) {
			if (work.indirect)
				++state.capture_fog_indirect;
			else
				state.capture_fog_workgroups +=
				    static_cast<uint64_t>(work.groups[0]) * work.groups[1] * work.groups[2];
		}
		if (state.fog_dispatches == 1 || state.fog_dispatches % 100000 == 0)
			LOGF("UFC6 fog dispatches skipped=%" PRIu64 "\n", state.fog_dispatches);
		return true;
	}
	return state.experimental_compute && state.disabled_compute.contains(id);
}

} // namespace Libs::Graphics
