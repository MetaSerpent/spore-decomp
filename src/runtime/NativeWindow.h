#pragma once
#include "runtime/SoftwareView.h"

#include <cstdint>
#include <functional>
#include <string>

namespace Spore::Terrain::View {

// Zero-SDL native desktop loop. Win32/GDI on Windows and X11 on Linux.
// The graphics and input adapters belong to this reconstructed-engine tooling
// and do not claim to reproduce the original Spore input/rendering stack.
// Keep the original values stable for existing viewers and recorded inputs.
enum class InputKind : uint8_t { KeyDown, MouseDown, Close, KeyUp, MouseMove, MouseUp, FocusLost };
enum KeyCode : int {
  LeftArrow = 1001, RightArrow = 1002, UpArrow = 1003,
  DownArrow = 1004, F5 = 2005, F9 = 2009,
};
struct InputEvent {
  InputKind kind = InputKind::KeyDown;
  int key = 0;            // uppercase ASCII or KeyCode for special keys
  int x = 0, y = 0;      // mouse coordinates in pixels
  int mouseButton = 0;   // 1 left, 2 middle, 3 right; held button for MouseMove
};
struct DesktopCallbacks {
  // Required: a frame with the exact width and height supplied by the window.
  std::function<Frame(uint32_t,uint32_t)> onFrame;
  std::function<void(const InputEvent&)> onInput;
  // Called at most once per loop iteration (nominal ~30Hz).
  std::function<void()> onTick;
  // Optional deterministic termination hook for virtual-display CI.
  std::function<bool()> shouldClose;
};

// Blocks until the user closes the window or presses Escape.
// Throws if a native display is unavailable, frames are malformed, or the
// callback fails. No audio or background thread is created.
int RunDesktop(const std::string& title, uint32_t width, uint32_t height,
               const DesktopCallbacks& callbacks);

} // namespace Spore::Terrain::View
