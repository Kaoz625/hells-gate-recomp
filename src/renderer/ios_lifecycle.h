#pragma once

namespace renderer {
#if defined(__APPLE__)
// Must run on the main (UI) thread once; AppIsActive / WaitWhileInactive from any thread.
void InstallAppLifecycleObservers();
bool AppIsActive();
// Blocks while the app is inactive / in the background; true if it waited.
bool WaitWhileInactive();
#endif
}  // namespace renderer
