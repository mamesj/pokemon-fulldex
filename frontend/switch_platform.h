// Nintendo Switch (homebrew, libnx) parts of the frontend. Kept apart from main.cpp so that
// libnx's <switch.h>, with its short type names and macros, is only seen by one file.
#pragma once
#ifdef __SWITCH__
#include <cstdint>
#include <string>

namespace SwitchPlatform
{
// Called first in main(). Started from the Homebrew Menu (no arguments), it sets up the
// arguments for the ROM, save and settings in the app's own folder on the SD card, sends the
// log there, and checks that the game was started with the whole console's memory. Returns
// false, after saying why on the screen, when the game cannot start.
bool Start(int& argc, char**& argv);

// Whether the game was chosen from the game list (several games found), so that QUIT in the pause
// menu goes back to it; ChangeGame then makes the program start again with the list once it ends.
bool CanChangeGame();
void ChangeGame();

// Called once the game has run a few seconds: a start with the JIT worked, so the next one
// uses it again (otherwise the next start falls back to the interpreter by itself).
void JitStarted();

// The console's own date and time, in the time zone set on it. False if it cannot be read.
bool LocalTime(int& year, int& month, int& day, int& hour, int& minute, int& second);

// The system language set on the console ("en", "es", "ja", "fr", "de", "it", "ko", etc.).
const char* SystemLanguage();

// Whether the console is in handheld or tabletop mode (its own screen, 720p) rather than docked.
bool IsHandheld();

// For the timing lines in the log, in test runs only (empty otherwise): the processor,
// graphics and memory clocks and the chip's and the board's temperature right now, as text.
std::string Status();

// The thread for the emulation when frames are shown alongside it (main.cpp, emuthread.h):
// core 0, the main thread's priority and a stack as big as the main thread's. Then the main
// thread moves to core 2 (MoveMainThread). JoinThread waits for the thread's end.
bool StartThread(void (*entry)(void*), void* arg);
void JoinThread();
void MoveMainThread();

// Called last: the next start (the game list again, or the next test run).
void Finish();

// Ends the program at once (after Finish), for an exit that has got stuck.
[[noreturn]] void ForceExit();
}
#endif
