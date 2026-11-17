// Builds without the DiligentCore native renderer (DANTESINFERNO_NATIVE_RENDERER=OFF, e.g. iPhone/iPad on
// blz/ios): the SDK's rexgpu-xenos plugin draws every frame. d3d_hooks.cpp and dantes_inferno_app.h still call
// the native renderer's hooks, so this file gives them no-op bodies. Nothing here changes guest behaviour:
// every hook only observed or mirrored draws for the native renderer.
//
// The `renderer` setting is fixed to "xenos" here (renderer_config.cpp, which defines it on desktop, needs
// windows.h); ConfigureSyncOnlyGpu therefore keeps gpu_host_rendering on.

#include "renderer.h"
#include "renderer_config.h"
#include "ui_overlay.h"

#include <rex/cvar.h>

REXCVAR_DEFINE_STRING(renderer, "xenos", "Graphics",
                      "Graphics system. Builds without the native renderer only have 'xenos' (the "
                      "rexgpu-xenos plugin renders)")
    .allowed({"xenos"});

namespace renderer {

uint32_t CommandBufferWritePtr(uint32_t, const uint8_t*) { return 0; }

bool Start(rex::ui::Window*, const std::filesystem::path&, std::string& error) {
  error = "This build has no native renderer (renderer = xenos).";
  return false;
}

PrecompileStatus GetPrecompileStatus() { return {}; }

// Nothing to precompile: the caller runs its continuation itself when this returns false.
bool RunWhenPrecompiled(std::function<void()>) { return false; }

void Stop() {}

void AfterClear(const CallArgs&, const uint8_t*) {}
void AfterResolve(const CallArgs&, const uint8_t*) {}
void BeforeDraw(uint32_t, const uint8_t*) {}
void AfterDraw(const CallArgs&, DrawKind, const uint8_t*) {}
void AfterBeginVertices(const CallArgs&, uint32_t) {}
void AfterEndVertices(const uint8_t*) {}
void BeforeProgramFlush(uint32_t, const uint8_t*) {}
void AfterProgramFlush(uint32_t, const uint8_t*) {}
void AfterCommandBufferReserve(uint32_t) {}
void RecordSegmentSwitch(uint32_t, uint32_t) {}
uint32_t SegmentSwitchCount() { return 0; }

void BoundShaderHashes(uint32_t, const uint8_t*, uint64_t& vs, uint64_t& ps) { vs = ps = 0; }
void LastIndexBuffer(uint32_t& base, uint32_t& count, uint32_t& format) { base = count = format = 0; }

void SetButtonGlyphStyleSource(std::function<GlyphStyle()>) {}
void SetButtonGlyphBindingsSource(std::function<KeyBindings()>) {}

PresentCount PresentedFrames() { return {}; }
void AfterSwap(const CallArgs&, const uint8_t*, bool) {}

void ConfigureSyncOnlyGpu() {}

void SetOverlayFrameRequest(std::function<void()>) {}

// Only constructed when renderer == "dante", which these builds never select; the bodies keep the vtable.
std::unique_ptr<rex::ui::ImmediateTexture> OverlayDrawer::CreateTexture(uint32_t, uint32_t,
                                                                         rex::ui::ImmediateTextureFilter,
                                                                         bool, const uint8_t*) {
  return nullptr;
}
void OverlayDrawer::Begin(rex::ui::UIDrawContext&, float, float) {}
void OverlayDrawer::BeginDrawBatch(const rex::ui::ImmediateDrawBatch&) {}
void OverlayDrawer::Draw(const rex::ui::ImmediateDraw&) {}
void OverlayDrawer::End() {}
void OverlayDrawer::PublishEmpty() {}

}  // namespace renderer
