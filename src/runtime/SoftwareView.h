#pragma once
#include "runtime/TerraformField.h"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <utility>
#include <vector>

namespace Spore::Terrain::View {

// A deterministic CPU rasterizer. The renderer is new, portable BlackSpore
// infrastructure, not recovered retail rendering code. No windowing, GPU,
// graphical dependency, proprietary asset or user interaction is required.
struct Camera {
  float yaw = 0.75f;      // radians around vertical axis
  float pitch = 0.85f;    // elevation, 0.25..1.4 radians
  float zoom = 1.0f;     // orthographic scale, 0.25..8
  float panX = 0.0f;     // view-space horizontal offset
  float panY = 0.0f;     // view-space vertical offset
};
enum class Action : uint8_t {
  Left, Right, Up, Down, RotateLeft, RotateRight,
  TiltUp, TiltDown, ZoomIn, ZoomOut, Reset
};
void ApplyCameraAction(Camera& camera, Action action, float strength = 1.0f);

struct Marker {
  float x = 0.5f;       // normalized horizontal world position
  float z = 0.5f;       // normalized world depth
  uint32_t colour = 0x00FFF1A0U;
  float radius = 4.0f;  // output pixel radius
};
struct Frame {
  uint32_t width = 0;
  uint32_t height = 0;
  // 0x00RRGGBB; little-endian BGRA-compatible for native Win32 DIB/X11.
  std::vector<uint32_t> rgb;
  uint64_t Checksum() const noexcept;
};
Frame RenderTerrain(const TerraformField& terrain, const Camera& camera,
                    uint32_t imageWidth, uint32_t imageHeight,
                    const std::vector<Marker>& markers = {});
// Approximate screen click intersection with a plane at given normalized height.
// Returns normalized x,z (not clamped, enabling callers to reject outside clicks).
std::pair<float,float> ScreenToTerrain(const Camera& camera,
                                      uint32_t imageWidth, uint32_t imageHeight,
                                      float screenX, float screenY,
                                      float guessTerrainHeight = 0.5f);
void WritePPM(const Frame& image, std::ostream& output);

} // namespace Spore::Terrain::View
