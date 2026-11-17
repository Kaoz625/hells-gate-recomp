#pragma once

#include <cstdint>

namespace renderer {

// Render-thread counters for the performance log.
struct RenderStats {
  uint64_t shaders_compiled = 0;
  uint64_t pipelines_created = 0;
  // Draws skipped while their pipeline was still being built (dante_async_pipelines).
  uint64_t draws_waiting_pipeline = 0;
};

inline RenderStats g_render_stats;

}  // namespace renderer
