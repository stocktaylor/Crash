// crash - mid-asm hooks for the DLL module (DLL/CRASH.DLL.XEX), referenced
// from the [[modules.midasm_hook]] entries in crash_manifest.toml.
//
// Compiled into the module's shared library (lib<project>_CRASH_DLL.so, see
// CMakeLists.txt), not the executable: codegen calls these from the module's
// generated code. The executable doesn't export its symbols, so cvars defined
// there (crash_60fps, in crash_hooks.cpp) are read through the cvar registry.

#include <bit>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

namespace {
// The game's own per-frame delta, captured on the first frame before anything
// is written: 1/30 s (TargetElapsedTime 333,333 ticks) at a 60 Hz video mode.
bool g_have_game_dt = false;
float g_game_dt = 0.0f;
bool g_last_60fps = false;
}  // namespace

// Game time step, every frame (sub_8A1A718C's frame loop 0x8A1A7D48..0x8A1A867C;
// hook at 0x8A1A7D50, where each iteration loads the delta with
// `lfs f0,-1736(r30)`, r30 = 0x8AE40000).
//
// The game loop is .NET/XNA-style with a fixed time step. Before entering the
// frame loop, sub_8A1A718C computes the delta once from TargetElapsedTime
// (ticks / 10,000,000; the game sets that from half the refresh rate: 1/30 s)
// and stores it at 0x8AE3F938 (copy at 0x8AE3F93C). Each frame then derives the
// scaled delta (0x8AE42E14), total time and other copies from it.
//
// CrashSwapInterval presents every vblank when crash_60fps is on, so the delta
// must be halved then and restored when it's off. It's set here, per frame,
// because rexlauncher's in-game menu can toggle crash_60fps while the game runs
// and the swap interval follows it immediately: a delta fixed at start-up made
// the game run at half or double speed after a toggle.
void CrashFrameTimeStep(PPCRegister& r30) {
  auto* memory = rex::system::kernel_memory();
  auto* dt = memory->TranslateVirtual<uint8_t*>(r30.u32 - 1736);       // 0x8AE3F938
  auto* dt_copy = memory->TranslateVirtual<uint8_t*>(r30.u32 - 1732);  // 0x8AE3F93C
  uint32_t current_bits = rex::memory::load_and_swap<uint32_t>(dt);
  if (!g_have_game_dt) {
    g_game_dt = std::bit_cast<float>(current_bits);
    g_have_game_dt = true;
  }

  bool want_60fps = rex::cvar::Query<bool>("crash_60fps");
  float wanted = want_60fps ? g_game_dt * 0.5f : g_game_dt;
  uint32_t wanted_bits = std::bit_cast<uint32_t>(wanted);
  if (current_bits != wanted_bits) {
    rex::memory::store_and_swap<uint32_t>(dt, wanted_bits);
    rex::memory::store_and_swap<uint32_t>(dt_copy, wanted_bits);
  }
  if (want_60fps != g_last_60fps) {
    g_last_60fps = want_60fps;
    REXLOG_INFO("crash_60fps={}: game time step {:.5f} s", want_60fps, wanted);
  }
}
