/*
 * MeshSwarm Library - Callbacks Module
 * 
 * Custom callback hooks for loop, serial, and display customization.
 * Only compiled when MESHSWARM_ENABLE_CALLBACKS is enabled.
 */

#include "../MeshSwarm.h"

#if MESHSWARM_ENABLE_CALLBACKS

// ============== CALLBACK REGISTRATION ==============
void MeshSwarm::onLoop(LoopCallback callback) {
  loopCallbacks.push_back(callback);
}

#if MESHSWARM_ENABLE_SERIAL
void MeshSwarm::onSerialCommand(SerialHandler handler) {
  serialHandlers.push_back(handler);
}
#endif

#if MESHSWARM_ENABLE_DISPLAY
void MeshSwarm::onDisplayUpdate(DisplayHandler handler) {
  displayHandlers.push_back(handler);
}
#endif

#endif // MESHSWARM_ENABLE_CALLBACKS
