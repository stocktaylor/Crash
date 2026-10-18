// crash - mid-asm hooks referenced from crash_manifest.toml ([[midasm_hook]]).
//
// Codegen emits `extern void <name>(...);` and calls the hook at the given
// guest address, so these must stay at global scope with these exact names.

#include <chrono>
#include <cstdint>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(crash_60fps, false, "Crash",
                    "Present every vblank instead of every second one (60 fps at 60 Hz; the "
                    "360 version is capped at 30). Experimental: only correct if the game's "
                    "simulation uses measured frame time rather than a fixed step per frame");

namespace {
constexpr uint32_t kErrorIoPending = 997;  // ERROR_IO_PENDING
}  // namespace

// Gamertag -> XUID lookup on selecting Play (sub_821BA180, hook at 0x821BA234,
// right after the call to the Xbox Live library's XUserFindUsers).
//
// The game zeroes an XOVERLAPPED, calls XUserFindUsers, ignores the return
// value and sets state 1. The state machine (sub_821B9F58) then polls the
// overlapped and, on result 0, parses its uninitialised 32-byte reply buffer
// as FIND_USERS_RESPONSE {count, users*}. When the request fails synchronously
// (here: no Xbox Live, XLIVEBASE message 0x58017 fails), the library returns
// ERROR_FUNCTION_FAILED and only writes dwExtendedError, which is correct, so
// the overlapped never completes and its zeroed result reads as success. The
// game then walks heap garbage into the stack guard page at 0x70000000; inside
// its SEH scope that faults ~20k times a second (frozen menu, audio static).
// On a console this path only runs online, where the request goes async.
//
// Complete the overlapped with the synchronous failure, so the poll takes the
// game's own error path (state 3) instead.
void CrashFindUsersSyncFailure(PPCRegister& r3, PPCRegister& r30) {
  uint32_t status = r3.u32;
  if (status == kErrorIoPending || status == 0) {
    return;  // queued (or done): the overlapped completes normally
  }
  auto* overlapped = rex::system::kernel_memory()->TranslateVirtual<uint8_t*>(r30.u32);
  rex::memory::store_and_swap<uint32_t>(overlapped + 0, status);  // InternalLow = result
  REXLOG_DEBUG("XUserFindUsers failed synchronously ({}); completed overlapped {:08X}", status,
               r30.u32);
}

// D3D GPU spin-wait step (sub_82203CB0, hook at its cctpl, 0x82203CC4). Every
// D3D wait on the GPU (fence/ring-space waits: sub_8220AC00 and ten other
// callers) loops on this helper until the GPU's progress writeback passes the
// target. On a 360 it drops hardware-thread priority (cctpl) and runs 32
// db16cyc delay hints, yielding the core to its sibling thread; recompiled,
// those are no-ops, so the main thread spun at ~1.1 cores in menus (83% of
// all samples) and ~0.9 in races. A short sleep per step stands in for them.
void CrashGpuWaitBackoff() {
  std::this_thread::sleep_for(std::chrono::microseconds(20));
}

// D3D swap scheduling (sub_8221EE10, hook at 0x8221EE20 once the swap token is
// in r30). Same XDK D3D code as London's sub_825BDD40. The GPU's swap
// interrupt hands D3D a token
//   frontbuffer address (bits 12-31) | swap interval << 8 | present-immediate
//   threshold (bits 0-7),
// and the flip is queued for (previous flip's vblank + interval). Crash
// presents with interval 2, i.e. every second vblank: 30 fps at 60 Hz.
// Rewriting it to 1 presents every vblank. Whether the game then plays at the
// right speed depends on its update using measured frame time (as London's
// does); a fixed step per frame would run everything at double speed, hence
// off by default (crash_60fps).
void CrashSwapInterval(PPCRegister& r30) {
  if (!REXCVAR_GET(crash_60fps)) {
    return;
  }
  uint32_t token = r30.u32;
  if (((token >> 8) & 0xF) == 2) {
    r30.u64 = (token & ~0xF00u) | (1u << 8);
  }
}
