// Read-only bulk audit of Spore property assets.
//
// This tool intentionally does not require retail data to compile or run its
// synthetic regression tests. Real game archives are supplied only at runtime.
#include "resource/PackageStack.h"
#include "resource/PropertyList.h"
#include "resource/PropertyDatabase.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace Spore::Resource;
namespace {

constexpr uint32_t kPropType = 0x00B1B104;
constexpr uint32_t kMaxResourceBytes = 16U * 1024U * 1024U;

struct ErrorEntry {
  ResourceKey key;
  std::string reason;
};
struct Report {
  size_t mounted = 0;
  size_t effective = 0;
  size_t properties = 0;
  size_t decoded = 0;
  size_t failed = 0;
  size_t tooLarge = 0;
  size_t parentPassed = 0;
  size_t parentFailed = 0;
  size_t propertyRecords = 0;
  std::map<uint16_t, size_t> types;
  std::vector<ErrorEntry> errors;
};

std::string Hex(uint32_t number) {
  std::ostringstream output;
  output << "0x" << std::hex << std::uppercase
         << std::setw(8) << std::setfill('0') << number;
  return output.str();
}
void JsonString(std::ostream& out, const std::string& value) {
  out << '"';
  for (unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          out << "\\u00" << std::hex << std::uppercase
              << std::setw(2) << std::setfill('0') << unsigned(c)
              << std::dec;
        } else {
          out << static_cast<char>(c);
        }
        break;
    }
  }
  out << '"';
}
void Output(const Report& r, bool inherit) {
  std::cout << "{\"packages\":" << r.mounted
            << ",\"effective_resources\":" << r.effective
            << ",\"property_resources\":" << r.properties
            << ",\"decoded\":" << r.decoded
            << ",\"failed\":" << r.failed
            << ",\"oversized\":" << r.tooLarge
            << ",\"property_records\":" << r.propertyRecords;
  if (inherit) {
    std::cout << ",\"parent_chains_passed\":" << r.parentPassed
              << ",\"parent_chains_failed\":" << r.parentFailed;
  }
  std::cout << ",\"types\":[";
  bool first = true;
  for (const auto& count : r.types) {
    if (!first) std::cout << ',';
    first = false;
    std::cout << "{\"id\":";
    JsonString(std::cout, Hex(count.first));
    std::cout << ",\"count\":" << count.second << '}';
  }
  std::cout << "],\"errors\":[";
  first = true;
  for (const auto& error : r.errors) {
    if (!first) std::cout << ',';
    first = false;
    std::cout << "{\"type\":";
    JsonString(std::cout, Hex(error.key.type));
    std::cout << ",\"group\":";
    JsonString(std::cout, Hex(error.key.group));
    std::cout << ",\"instance\":";
    JsonString(std::cout, Hex(error.key.instance));
    std::cout << ",\"reason\":";
    JsonString(std::cout, error.reason);
    std::cout << '}';
  }
  std::cout << "]}\n";
}
void Usage() {
  std::cerr << "Usage: spore_prop_audit [--strict] [--verify-parents]"
               " <base.package> [patch.package ...]\n"
               "Outputs machine-readable JSON metadata and diagnostics.\n"
               "Input files are never modified. Later packages override earlier packages.\n"
               "--strict returns status 3 if any resource fails inspection.\n"
               "Per-property maximum unpacked size: 16 MiB.\n";
}

} // namespace

int main(int argc, char** argv) {
  bool strict = false;
  bool inherit = false;
  std::vector<std::string> files;
  for (int i = 1; i < argc; ++i) {
    const std::string token(argv[i]);
    if (token == "--strict") strict = true;
    else if (token == "--verify-parents") inherit = true;
    else if (!token.empty() && token[0] == '-') {
      Usage();
      return 2;
    } else files.push_back(token);
  }
  if (files.empty()) {
    Usage();
    return 2;
  }

  PackageStack stack;
  PropertyDatabase db;
  Report stats;
  for (const auto& file : files) {
    if (!stack.Mount(file)) {
      std::cerr << "cannot mount " << file << ": " << stack.Error() << '\n';
      return 1;
    }
    if (inherit && !db.Mount(file)) {
      std::cerr << "cannot mount " << file << ": " << db.Error() << '\n';
      return 1;
    }
    ++stats.mounted;
  }

  const auto entries = stack.EffectiveEntries();
  stats.effective = entries.size();
  for (const auto& item : entries) {
    if (item.key.type != kPropType) continue;
    ++stats.properties;

    if (item.entry.memSize > kMaxResourceBytes) {
      ++stats.failed;
      ++stats.tooLarge;
      stats.errors.push_back({item.key, "unpacked property exceeds 16 MiB safety limit"});
      continue;
    }
    const auto raw = stack.Read(item.key);
    if (!raw) {
      ++stats.failed;
      stats.errors.push_back({item.key, "resource read failed: " + stack.Error()});
      continue;
    }
    PropertyList props;
    if (!props.Parse(*raw)) {
      ++stats.failed;
      stats.errors.push_back({item.key, props.Error()});
      continue;
    }

    ++stats.decoded;
    stats.propertyRecords += props.Properties().size();
    for (const auto& property : props.Properties())
      ++stats.types[property.type];

    if (inherit) {
      const auto* effective = db.Resolve(item.key);
      if (!effective) {
        ++stats.parentFailed;
        stats.errors.push_back({item.key, "inheritance: " + db.Error()});
      } else {
        ++stats.parentPassed;
      }
    }
  }

  Output(stats, inherit);
  return strict && (!stats.errors.empty()) ? 3 : 0;
}
