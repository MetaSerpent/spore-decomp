#include "runtime/TerraformField.h"
#include "runtime/TerraformMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Spore::Terrain {
namespace {
constexpr std::uint64_t kHashOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kHashPrime = UINT64_C(1099511628211);
constexpr double kInvSqrt2 = 0.7071067811865475244008443621;

void AddWord(std::uint64_t& h, std::uint32_t v) noexcept {
  for (unsigned i = 0; i < 4; ++i) {
    h ^= static_cast<std::uint8_t>(v >> (8 * i));
    h *= kHashPrime;
  }
}
void ValidValue(float value) {
  if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
    throw std::invalid_argument("terraform sample must be finite in [0,1]");
}
} // namespace

TerraformField::TerraformField(std::uint32_t width, std::uint32_t height,
                               float initialValue)
    : width_(width), height_(height) {
  if (width < 2 || height < 2 ||
      std::uint64_t(width) * std::uint64_t(height) > 1000000)
    throw std::invalid_argument("terraform field dimensions outside 2x2..1000000 samples");
  ValidValue(initialValue);
  samples_.assign(std::size_t(width) * height, initialValue);
}

float TerraformField::At(std::uint32_t x, std::uint32_t y) const {
  if (x >= width_ || y >= height_) throw std::out_of_range("terrain coordinate outside field");
  return samples_[std::size_t(y) * width_ + x];
}

double TerraformField::Mean() const noexcept {
  double sum = 0;
  for (float value : samples_) sum += static_cast<double>(value);
  return sum / static_cast<double>(samples_.size());
}

std::uint32_t TerraformField::PlanetType(float lavaThreshold) const {
  if (!std::isfinite(lavaThreshold))
    throw std::invalid_argument("lava threshold must be finite");
  return PlanetTypeFromTerraformValue(static_cast<float>(Mean()), lavaThreshold);
}

BrushResult TerraformField::Apply(float centreX, float centreY,
                                  float radius, float delta) {
  if (!std::isfinite(centreX) || !std::isfinite(centreY) ||
      centreX < 0.0f || centreX > 1.0f ||
      centreY < 0.0f || centreY > 1.0f ||
      !std::isfinite(radius) || radius <= 0.0f || radius > 1.0f ||
      !std::isfinite(delta) || delta < -1.0f || delta > 1.0f)
    throw std::invalid_argument("invalid brush centre, radius, or delta");
  if (delta == 0.0f) return {};

  const auto clampX = [this](double x) {
    return std::clamp(static_cast<int>(x * double(width_ - 1)), 0,
                      static_cast<int>(width_) - 1);
  };
  const auto clampY = [this](double y) {
    return std::clamp(static_cast<int>(y * double(height_ - 1)), 0,
                      static_cast<int>(height_) - 1);
  };
  const int left = clampX(std::max(0.0, double(centreX) - double(radius)));
  const int right = clampX(std::min(1.0, double(centreX) + double(radius))) + 1;
  const int top = clampY(std::max(0.0, double(centreY) - double(radius)));
  const int bottom = clampY(std::min(1.0, double(centreY) + double(radius))) + 1;
  BrushResult result;
  const double r2 = double(radius) * double(radius);
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      const double dx = double(x) / double(width_ - 1) - double(centreX);
      const double dy = double(y) / double(height_ - 1) - double(centreY);
      if (dx * dx + dy * dy >= r2) continue;
      const float fx = static_cast<float>(0.5 + dx / double(radius) * kInvSqrt2);
      const float fy = static_cast<float>(0.5 + dy / double(radius) * kInvSqrt2);
      const float influence = TerraformBrushFalloff(fx, fy);
      if (influence <= 0.0f) continue;
      float& value = samples_[std::size_t(y) * width_ + std::size_t(x)];
      const float next = static_cast<float>(std::clamp(
          double(value) + double(delta) * double(influence), 0.0, 1.0));
      if (next == value) continue;
      result.appliedDelta += double(next) - double(value);
      value = next;
      ++result.affected;
    }
  }
  return result;
}

std::uint64_t TerraformField::Checksum() const noexcept {
  std::uint64_t h = kHashOffset;
  AddWord(h, width_);
  AddWord(h, height_);
  for (float value : samples_) {
    std::uint32_t bits = 0;
    static_assert(sizeof bits == sizeof value, "expected binary32");
    std::memcpy(&bits, &value, sizeof bits);
    AddWord(h, bits);
  }
  return h;
}

std::string TerraformField::Snapshot() const {
  std::ostringstream out;
  out << "SPORE_TERRAFORM_FIELD 1\n" << width_ << ' ' << height_ << '\n';
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  for (std::size_t i = 0; i < samples_.size(); ++i) {
    out << samples_[i] << (i + 1 == samples_.size() ? '\n' : ' ');
  }
  return out.str();
}

TerraformField TerraformField::Restore(std::string_view snapshot) {
  std::istringstream in{std::string(snapshot)};
  std::string signature;
  unsigned version = 0;
  std::uint32_t width = 0, height = 0;
  if (!(in >> signature >> version) || signature != "SPORE_TERRAFORM_FIELD" ||
      version != 1 || !(in >> width >> height))
    throw std::invalid_argument("invalid terrain snapshot header or version");
  TerraformField field(width, height, 0.0f);
  for (auto& value : field.samples_) {
    if (!(in >> value) || !std::isfinite(value) || value < 0.0f || value > 1.0f)
      throw std::invalid_argument("invalid, missing or nonfinite terrain sample");
  }
  std::string trailing;
  if (in >> trailing) throw std::invalid_argument("unexpected trailing terrain snapshot data");
  return field;
}

} // namespace Spore::Terrain
