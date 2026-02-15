#include "WiiUProc.hpp"

#if RETRO_PLATFORM == RETRO_WIIU

#if defined(RETRO_WIIU_CF_AROMA)
// Aroma CFW: provide compatible stubs (adjust if Aroma exposes a different API)
void WiiU_ProcInit() { }
void WiiU_ProcShutdown() { }
bool WiiU_ProcIsRunning() { return true; }


#else
// Prefer WUT/ProcUI when available; otherwise fall back to WHB helpers.
#if defined(__WUT__)
#include <proc_ui/procui.h>
#include <coreinit/foreground.h>

static void WiiU_ProcSaveCallback(void) { OSSavesDone_ReadyToRelease(); }

void WiiU_ProcInit() { ProcUIInit(WiiU_ProcSaveCallback); }
void WiiU_ProcShutdown() { ProcUIShutdown(); }
bool WiiU_ProcIsRunning() { return ProcUIIsRunning() && !ProcUIInShutdown(); }

#else
// WHB fallback (older toolchains)
#include <whb/proc.h>

void WiiU_ProcInit() { WHBProcInit(); }
// For graceful exit under WHB/Tiramisu, signal the proc loop to stop
// rather than forcibly shutting down low-level subsystems.
void WiiU_ProcShutdown() { WHBProcStopRunning(); }
bool WiiU_ProcIsRunning() { return WHBProcIsRunning(); }

#endif

// Default weak hooks for foreground release/acquire. Platforms that need
// to free MEM1 or other foreground-only resources can override these
// symbols. For safety, the engine provides an opt-in implementation that
// will call the render-device helpers only when `ENABLE_WIIU_FOREGROUND_HANDLERS`
// is defined at compile time.
#if RETRO_PLATFORM == RETRO_WIIU
// Prototypes are weak so platform code can still override them.
extern "C" void WiiU_OnReleaseForeground() __attribute__((weak));
extern "C" void WiiU_OnAcquireForeground() __attribute__((weak));

#if defined(ENABLE_WIIU_FOREGROUND_HANDLERS)
// Opt-in: use minimal, best-effort render-device calls to release/recreate
// foreground graphics memory. Keep this guarded to avoid unexpected side
// effects on platforms that don't expect these calls.
extern int InitRenderDevice();
extern void ReleaseRenderDevice();

extern "C" void WiiU_OnReleaseForeground()
{
	// Best-effort: release the render device so the system can reclaim
	// graphics memory while backgrounded. Avoid other global state changes.
	ReleaseRenderDevice();
}

extern "C" void WiiU_OnAcquireForeground()
{
	// Best-effort: recreate the render device when returning to foreground.
	InitRenderDevice();
}
#else
// Default safe no-op implementations.
extern "C" void WiiU_OnReleaseForeground() { }
extern "C" void WiiU_OnAcquireForeground() { }
#endif

#endif

#endif // RETRO_WIIU_CF_AROMA

#else
// Non-WiiU platforms: no-op
void WiiU_ProcInit() { }
void WiiU_ProcShutdown() { }
bool WiiU_ProcIsRunning() { return true; }
#endif
