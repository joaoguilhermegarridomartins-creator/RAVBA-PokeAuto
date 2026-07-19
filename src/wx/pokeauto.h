#ifndef VBAM_WX_POKEAUTO_H_
#define VBAM_WX_POKEAUTO_H_

#include <cstdint>
#include <string>

namespace pokeauto {

// Starts the embedded Pokemon FireRed (BPRE revision 1) starter hunt.
// The recorded sequence is replayed internally through the emulator joypad,
// so no Windows virtual controller is created.
bool StartFireRedStarterHunt();

// Stops the current automation and restores the user's speedup settings.
void Stop();

// Called once per emulated frame.
void Tick();

// Returns the internal joypad bits that should be mixed with physical input.
uint32_t GetInputMask();

bool IsRunning();
std::string GetStatusText();

}  // namespace pokeauto

#endif  // VBAM_WX_POKEAUTO_H_
