#include "runtime/TerrainMesh.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace Spore::Terrain {
namespace {
constexpr std::size_t kMaxVertices = 262144;

struct Vec3 { double x,y,z; };
Vec3 Normal(const TerraformField& field, std::uint32_t x, std::uint32_t z,
            float horizontalScale, float verticalScale) {
  const auto width = field.Width();
  const auto height = field.Height();
  const auto left = x > 0 ? x - 1 : x;
  const auto right = x + 1 < width ? x + 1 : x;
  const auto near = z > 0 ? z - 1 : z;
  const auto far = z + 1 < height ? z + 1 : z;
  const double dx = double(horizontalScale) * double(right - left) /
                    double(width - 1);
  const double dz = double(horizontalScale) * double(far - near) /
                    double(height - 1);
  const double sx = (double(field.At(right,z)) - double(field.At(left,z))) *
                    double(verticalScale) / dx;
  const double sz = (double(field.At(x,far)) - double(field.At(x,near))) *
                    double(verticalScale) / dz;
  const double norm = std::sqrt(sx*sx + 1.0 + sz*sz);
  return {sx == 0.0 ? 0.0 : -sx/norm, 1.0/norm,
          sz == 0.0 ? 0.0 : -sz/norm};
}
std::size_t Index(std::uint32_t x,std::uint32_t z,std::uint32_t width) {
  return std::size_t(z) * width + x + 1;
}
} // namespace

TerrainMeshStats WriteTerrainOBJ(const TerraformField& field,
                                 std::ostream& out,
                                 float horizontalScale, float verticalScale) {
  const auto width = field.Width(), height = field.Height();
  if (!std::isfinite(horizontalScale) || horizontalScale <= 0.0f ||
      !std::isfinite(verticalScale) || verticalScale < 0.0f)
    throw std::invalid_argument("mesh scales must be finite; horizontal > 0, vertical >= 0");
  const auto sampleCount = std::size_t(width) * height;
  if (width < 2 || height < 2 || sampleCount > kMaxVertices)
    throw std::invalid_argument("OBJ mesh requires 2x2 through 262144 samples");

  TerrainMeshStats stats;
  stats.vertices = sampleCount;
  stats.triangles = std::size_t(width - 1) * (height - 1) * 2;
  stats.minimumHeight = 1.0f;
  stats.maximumHeight = 0.0f;
  out << "# BlackSpore reconstructed-terrain heightfield export (Y-up)\n";
  out << "o SporeTerrainField\n";
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  for (std::uint32_t z=0;z<height;++z) {
    for (std::uint32_t x=0;x<width;++x) {
      const float sample = field.At(x,z);
      stats.minimumHeight = std::min(stats.minimumHeight,sample);
      stats.maximumHeight = std::max(stats.maximumHeight,sample);
      const double vx = (double(x) / double(width-1) - 0.5) * horizontalScale;
      const double vz = (double(z) / double(height-1) - 0.5) * horizontalScale;
      const double vy = double(sample) * verticalScale;
      out << "v " << vx << ' ' << vy << ' ' << vz << '\n';
    }
  }
  for (std::uint32_t z=0;z<height;++z) {
    for (std::uint32_t x=0;x<width;++x) {
      out << "vt " << double(x)/double(width-1) << ' '
          << double(z)/double(height-1) << '\n';
    }
  }
  for (std::uint32_t z=0;z<height;++z) {
    for (std::uint32_t x=0;x<width;++x) {
      const auto n=Normal(field,x,z,horizontalScale,verticalScale);
      out << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    }
  }
  for (std::uint32_t z=0;z+1<height;++z) {
    for (std::uint32_t x=0;x+1<width;++x) {
      const auto a=Index(x,z,width), b=Index(x+1,z,width),
                 c=Index(x,z+1,width), d=Index(x+1,z+1,width);
      // Y-up winding: left/bottom/right and right/bottom/top.
      out << "f " << a << '/' << a << '/' << a << ' '
          << c << '/' << c << '/' << c << ' '
          << b << '/' << b << '/' << b << '\n';
      out << "f " << b << '/' << b << '/' << b << ' '
          << c << '/' << c << '/' << c << ' '
          << d << '/' << d << '/' << d << '\n';
    }
  }
  if (!out) throw std::runtime_error("OBJ mesh stream write failed");
  return stats;
}

} // namespace Spore::Terrain
