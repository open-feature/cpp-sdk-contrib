#include "flagd_ops.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "flagd/evaluator/json_logic/json_logic.h"
#include "flagd/evaluator/murmur_hash/MurmurHash3.h"
#include "qcbor/qcbor.h"

namespace flagd {

namespace {

// Checks if a string has a leading zero (and is not just "0").
bool HasLeadingZero(std::string_view str) {
  return str.size() > 1 && str[0] == '0';
}

// Parses number according to SemVer 2.0.0 specification.
absl::StatusOr<uint64_t> ParseSemVerNum(std::string_view num_str,
                                        std::string_view name) {
  if (HasLeadingZero(num_str)) {
    return absl::InvalidArgumentError(absl::StrCat(
        name, " version MUST NOT contain leading zeros: ", num_str));
  }
  uint64_t out = 0;
  if (!absl::SimpleAtoi(num_str, &out)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid SemVer ", name, " digits: ", num_str));
  }
  return out;
}

// Evaluates and retrieves a fixed number of string arguments from JsonLogic.
absl::StatusOr<std::vector<std::string>> GetStrings(
    const json_logic::JsonLogic& eval, const nlohmann::json& values,
    const nlohmann::json& data, size_t expected_size) {
  if (!values.is_array()) {
    return absl::InvalidArgumentError("Arguments must be an array");
  }

  if (values.size() != expected_size) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Operator requires exactly ", expected_size, " arguments"));
  }

  std::vector<std::string> result;
  result.reserve(expected_size);
  for (const nlohmann::json& item : values) {
    absl::StatusOr<nlohmann::json> applied = eval.Apply(item, data);
    if (!applied.ok()) return applied.status();
    if (!applied.value().is_string()) {
      return absl::InvalidArgumentError(
          "All arguments must evaluate to strings");
    }
    result.push_back(applied.value().get<std::string>());
  }

  return result;
}

// Represents a Semantic Version (SemVer 2.0.0) for comparison.
class SemanticVersion {
 private:
  uint64_t major_;
  uint64_t minor_;
  uint64_t patch_;
  std::vector<std::string> pre_release_;

 public:
  explicit SemanticVersion(uint64_t major = 0, uint64_t minor = 0,
                           uint64_t patch = 0,
                           std::vector<std::string> pre_release = {})
      : major_(major),
        minor_(minor),
        patch_(patch),
        pre_release_(std::move(pre_release)) {}

  uint64_t GetMajor() const { return major_; }
  uint64_t GetMinor() const { return minor_; }
  uint64_t GetPatch() const { return patch_; }
  const std::vector<std::string>& GetPreRelease() const { return pre_release_; }

  // Parses a string into a SemanticVersion object.
  // Supports partial versions (e.g., "1.2") by defaulting missing parts to 0.
  static absl::StatusOr<SemanticVersion> Parse(std::string_view text) {
    if (!text.empty() && (text[0] == 'v' || text[0] == 'V')) {
      text.remove_prefix(1);
    }

    // 1. Remove build metadata (ignored for precedence comparison)
    std::vector<std::string_view> build_parts = absl::StrSplit(text, '+');
    std::string_view core_and_pre = build_parts[0];

    // 2. Separate core and pre-release
    std::vector<std::string_view> pre_parts =
        absl::StrSplit(core_and_pre, absl::MaxSplits('-', 1));
    std::string_view core = pre_parts[0];

    // 3. Parse core components (major.minor.patch)
    std::vector<std::string_view> core_parts = absl::StrSplit(core, '.');
    if (core_parts.empty() || core_parts.size() > 3) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid SemVer core: ", core));
    }

    absl::StatusOr<uint64_t> major = ParseSemVerNum(core_parts[0], "Major");
    if (!major.ok()) {
      return major.status();
    }

    uint64_t minor = 0;
    if (core_parts.size() >= 2) {
      absl::StatusOr<uint64_t> minor_res =
          ParseSemVerNum(core_parts[1], "Minor");
      if (!minor_res.ok()) {
        return minor_res.status();
      }
      minor = *minor_res;
    }

    uint64_t patch = 0;
    if (core_parts.size() == 3) {
      absl::StatusOr<uint64_t> patch_res =
          ParseSemVerNum(core_parts[2], "Patch");
      if (!patch_res.ok()) {
        return patch_res.status();
      }
      patch = *patch_res;
    }

    // 4. Parse pre-release identifiers
    std::vector<std::string> pre_release;
    if (pre_parts.size() > 1) {
      std::vector<std::string_view> identifiers =
          absl::StrSplit(pre_parts[1], '.');
      for (std::string_view ident : identifiers) {
        if (ident.empty()) {
          return absl::InvalidArgumentError("Empty pre-release identifier");
        }

        bool is_numeric =
            std::all_of(ident.begin(), ident.end(),
                        [](unsigned char chr) { return std::isdigit(chr); });
        if (is_numeric && HasLeadingZero(ident)) {
          return absl::InvalidArgumentError(
              "Numeric pre-release identifiers MUST NOT contain leading zeros");
        }

        pre_release.emplace_back(ident);
      }
    }

    return SemanticVersion(*major, minor, patch, std::move(pre_release));
  }

  // Compares two SemanticVersion objects based on SemVer 2.0.0 precedence
  // rules. Returns: -1 if this < other, 0 if equal, 1 if this > other
  int Compare(const SemanticVersion& other) const {
    if (major_ != other.major_) return major_ < other.major_ ? -1 : 1;
    if (minor_ != other.minor_) return minor_ < other.minor_ ? -1 : 1;
    if (patch_ != other.patch_) return patch_ < other.patch_ ? -1 : 1;

    // A normal version has higher precedence than a pre-release version
    if (pre_release_.empty() && !other.pre_release_.empty()) return 1;
    if (!pre_release_.empty() && other.pre_release_.empty()) return -1;
    if (pre_release_.empty() && other.pre_release_.empty()) return 0;

    size_t len = std::min(pre_release_.size(), other.pre_release_.size());
    for (size_t i = 0; i < len; ++i) {
      const std::string& lhs_part = pre_release_[i];
      const std::string& rhs_part = other.pre_release_[i];

      // Numeric identifiers have lower precedence than non-numeric identifiers.
      bool lhs_is_num =
          std::all_of(lhs_part.begin(), lhs_part.end(),
                      [](unsigned char chr) { return std::isdigit(chr); });
      bool rhs_is_num =
          std::all_of(rhs_part.begin(), rhs_part.end(),
                      [](unsigned char chr) { return std::isdigit(chr); });

      if (lhs_is_num && rhs_is_num) {
        // Compare numerically by length first, then lexicographically.
        // This supports arbitrary precision integers without overflow.
        if (lhs_part.length() != rhs_part.length()) {
          return lhs_part.length() < rhs_part.length() ? -1 : 1;
        }
        if (lhs_part != rhs_part) return lhs_part < rhs_part ? -1 : 1;
      } else if (lhs_is_num && !rhs_is_num) {
        return -1;
      } else if (!lhs_is_num && rhs_is_num) {
        return 1;
      } else {
        // Non-numeric identifiers are compared lexicographically in ASCII sort
        // order.
        if (lhs_part != rhs_part) return lhs_part < rhs_part ? -1 : 1;
      }
    }

    // A larger set of pre-release fields has a higher precedence than a smaller
    // set.
    if (pre_release_.size() != other.pre_release_.size()) {
      return pre_release_.size() < other.pre_release_.size() ? -1 : 1;
    }

    return 0;
  }
};

struct Distribution {
  nlohmann::json variant;
  int32_t weight;
};

constexpr size_t kStackKeyBufferSize = 256;

// Encodes a single string key to deterministic CBOR bytes for map key sorting.
std::vector<uint8_t> EncodeCborKey(const std::string& key) {
  QCBOREncodeContext ctx;
  UsefulBuf_MAKE_STACK_UB(buf, kStackKeyBufferSize);
  QCBOREncode_Init(&ctx, buf);
  QCBOREncode_AddText(&ctx, {key.data(), key.size()});
  UsefulBufC out;
  if (QCBOREncode_Finish(&ctx, &out) == QCBOR_SUCCESS) {
    return std::vector<uint8_t>(static_cast<const uint8_t*>(out.ptr),
                                static_cast<const uint8_t*>(out.ptr) + out.len);
  }
  UsefulBufC size_info;
  QCBOREncode_Init(&ctx, SizeCalculateUsefulBuf);
  QCBOREncode_AddText(&ctx, {key.data(), key.size()});
  QCBOREncode_Finish(&ctx, &size_info);
  std::vector<uint8_t> dyn_buf(size_info.len);
  UsefulBuf dyn_ub = {dyn_buf.data(), dyn_buf.size()};
  QCBOREncode_Init(&ctx, dyn_ub);
  QCBOREncode_AddText(&ctx, {key.data(), key.size()});
  QCBOREncode_Finish(&ctx, &out);
  return std::vector<uint8_t>(static_cast<const uint8_t*>(out.ptr),
                              static_cast<const uint8_t*>(out.ptr) + out.len);
}

// Encodes nlohmann::json to deterministic CBOR per RFC 8949.
void EncodeJson(QCBOREncodeContext* enc_ctx, const nlohmann::json& data) {
  if (data.is_object()) {
    QCBOREncode_OpenMap(enc_ctx);
    std::vector<std::pair<std::vector<uint8_t>, std::string>> keys;
    keys.reserve(data.size());
    for (const auto& item : data.items()) {
      keys.emplace_back(EncodeCborKey(item.key()), item.key());
    }
    std::sort(keys.begin(), keys.end(), [](const auto& lhs, const auto& rhs) {
      return lhs.first < rhs.first;
    });
    for (const auto& key_pair : keys) {
      const std::string& key = key_pair.second;
      QCBOREncode_AddText(enc_ctx, {key.data(), key.size()});
      EncodeJson(enc_ctx, data[key]);
    }
    QCBOREncode_CloseMap(enc_ctx);
  } else if (data.is_array()) {
    QCBOREncode_OpenArray(enc_ctx);
    for (const auto& element : data) {
      EncodeJson(enc_ctx, element);
    }
    QCBOREncode_CloseArray(enc_ctx);
  } else if (data.is_string()) {
    const std::string& str = data.get<std::string>();
    QCBOREncode_AddText(enc_ctx, {str.data(), str.size()});
  } else if (data.is_boolean()) {
    QCBOREncode_AddBool(enc_ctx, data.get<bool>());
  } else if (data.is_number_unsigned()) {
    QCBOREncode_AddUInt64(enc_ctx, data.get<uint64_t>());
  } else if (data.is_number_integer()) {
    int64_t val = data.get<int64_t>();
    if (val >= 0) {
      QCBOREncode_AddUInt64(enc_ctx, static_cast<uint64_t>(val));
    } else {
      QCBOREncode_AddInt64(enc_ctx, val);
    }
  } else if (data.is_number_float()) {
    double val = data.get<double>();
    if (std::trunc(val) == val && val <= static_cast<double>(INT64_MAX) &&
        val >= static_cast<double>(INT64_MIN)) {
      if (val < 0.0) {
        QCBOREncode_AddInt64(enc_ctx, static_cast<int64_t>(val));
      } else {
        QCBOREncode_AddUInt64(enc_ctx, static_cast<uint64_t>(val));
      }
    } else {
      QCBOREncode_AddDouble(enc_ctx, val);
    }
  } else if (data.is_null()) {
    QCBOREncode_AddNULL(enc_ctx);
  }
}

}  // namespace

absl::StatusOr<nlohmann::json> StartsWith(const json_logic::JsonLogic& eval,
                                          const nlohmann::json& values,
                                          const nlohmann::json& data) {
  absl::StatusOr<std::vector<std::string>> strings_res =
      GetStrings(eval, values, data, 2);
  if (!strings_res.ok()) return strings_res.status();

  const std::string& source_str = strings_res.value()[0];
  const std::string& prefix = strings_res.value()[1];

  return absl::StartsWith(source_str, prefix);
}

absl::StatusOr<nlohmann::json> EndsWith(const json_logic::JsonLogic& eval,
                                        const nlohmann::json& values,
                                        const nlohmann::json& data) {
  absl::StatusOr<std::vector<std::string>> strings_res =
      GetStrings(eval, values, data, 2);
  if (!strings_res.ok()) return strings_res.status();

  const std::string& source_str = strings_res.value()[0];
  const std::string& suffix = strings_res.value()[1];

  return absl::EndsWith(source_str, suffix);
}

absl::StatusOr<nlohmann::json> SemVer(const json_logic::JsonLogic& eval,
                                      const nlohmann::json& values,
                                      const nlohmann::json& data) {
  absl::StatusOr<std::vector<std::string>> strings_res =
      GetStrings(eval, values, data, 3);
  if (!strings_res.ok()) return strings_res.status();

  absl::StatusOr<SemanticVersion> v1_res =
      SemanticVersion::Parse(strings_res.value()[0]);
  if (!v1_res.ok()) return v1_res.status();
  const SemanticVersion& ver1 = v1_res.value();

  const std::string& operation = strings_res.value()[1];

  absl::StatusOr<SemanticVersion> v2_res =
      SemanticVersion::Parse(strings_res.value()[2]);
  if (!v2_res.ok()) return v2_res.status();
  const SemanticVersion& ver2 = v2_res.value();

  const int cmp = ver1.Compare(ver2);

  if (operation == "=" || operation == "==") return cmp == 0;
  if (operation == "!=") return cmp != 0;
  if (operation == ">") return cmp > 0;
  if (operation == "<") return cmp < 0;
  if (operation == ">=") return cmp >= 0;
  if (operation == "<=") return cmp <= 0;
  if (operation == "^") {
    if (ver1.Compare(ver2) < 0) return false;
    if (ver2.GetMajor() > 0) {
      return ver1.GetMajor() == ver2.GetMajor();
    }
    if (ver2.GetMinor() > 0) {
      return ver1.GetMajor() == 0 && ver1.GetMinor() == ver2.GetMinor();
    }
    return ver1.GetMajor() == 0 && ver1.GetMinor() == 0 &&
           ver1.GetPatch() == ver2.GetPatch();
  }
  if (operation == "~") {
    if (ver1.Compare(ver2) < 0) return false;
    return ver1.GetMajor() == ver2.GetMajor() &&
           ver1.GetMinor() == ver2.GetMinor();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Unknown SemVer operator: ", operation));
}

absl::StatusOr<nlohmann::json> Fractional(const json_logic::JsonLogic& eval,
                                          const nlohmann::json& values,
                                          const nlohmann::json& data) {
  if (!values.is_array()) {
    return absl::InvalidArgumentError(
        "fractional evaluation data is not an array");
  }

  if (values.empty()) {
    return absl::InvalidArgumentError("fractional evaluation data is empty");
  }

  // 1. Get the target property value used for bucketing
  absl::StatusOr<nlohmann::json> bucketing_property_eval =
      eval.Apply(values[0], data);
  if (!bucketing_property_eval.ok()) return bucketing_property_eval.status();

  if (bucketing_property_eval.value().is_null()) {
    return absl::InvalidArgumentError(
        "Fractional evaluation data cannot be null");
  }

  nlohmann::json bucketing_property_value;
  bool first_value_used = false;

  if (bucketing_property_eval.value().is_array()) {
    std::string flag_key;
    if (data.contains("$flagd") && data["$flagd"].is_object() &&
        data["$flagd"].contains("flagKey") &&
        data["$flagd"]["flagKey"].is_string()) {
      flag_key = data["$flagd"]["flagKey"].get<std::string>();
    }

    if (!data.is_object() || !data.contains("targetingKey") ||
        data["targetingKey"].is_null()) {
      return absl::InvalidArgumentError(
          "Missing targetingKey for fractional evaluation");
    }
    if (!data["targetingKey"].is_string()) {
      return absl::InvalidArgumentError("targetingKey must be a string");
    }

    std::string targeting_key = data["targetingKey"].get<std::string>();
    bucketing_property_value = nlohmann::json::array({flag_key, targeting_key});
    first_value_used = false;
  } else {
    bucketing_property_value = bucketing_property_eval.value();
    first_value_used = true;
  }

  // 2. Parse the fractional distribution
  std::vector<Distribution> distributions;
  uint64_t sum_of_weights = 0;

  for (size_t i = first_value_used ? 1 : 0; i < values.size(); i++) {
    absl::StatusOr<nlohmann::json> item = eval.Apply(values[i], data);
    if (!item.ok()) return item.status();
    if (!item.value().is_array() || item.value().empty()) {
      return absl::InvalidArgumentError("Invalid distribution element");
    }

    int32_t weight = 1;
    if (item.value().size() >= 2 && item.value()[1].is_number()) {
      weight = std::max(item.value()[1].get<int32_t>(), 0);
    }

    distributions.push_back({item.value()[0], weight});
    sum_of_weights += weight;
  }

  if (distributions.empty()) {
    return absl::InvalidArgumentError("No distributions found");
  }

  if (sum_of_weights == 0) {
    return absl::InvalidArgumentError("Sum of weights must be positive");
  }

  if (sum_of_weights >
      static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
    return absl::InvalidArgumentError("Sum of weights exceeds maximum limit");
  }

  // 3. Serialize hashing value to deterministic CBOR representation using qcbor
  QCBOREncodeContext encode_ctx;
  UsefulBufC size_info;
  QCBORError err;

  QCBOREncode_Init(&encode_ctx, SizeCalculateUsefulBuf);
  EncodeJson(&encode_ctx, bucketing_property_value);
  err = QCBOREncode_Finish(&encode_ctx, &size_info);
  if (err != QCBOR_SUCCESS) {
    return absl::InternalError(
        absl::StrCat("QCBOR size calculation failed: ", err));
  }

  std::vector<uint8_t> buffer(size_info.len);
  UsefulBuf cbor_buffer = {buffer.data(), buffer.size()};

  QCBOREncode_Init(&encode_ctx, cbor_buffer);
  EncodeJson(&encode_ctx, bucketing_property_value);
  UsefulBufC encoded;
  err = QCBOREncode_Finish(&encode_ctx, &encoded);
  if (err != QCBOR_SUCCESS) {
    return absl::InternalError(absl::StrCat("QCBOR encoding failed: ", err));
  }

  // 4. Calculate MurmurHash3_x86_32 on the CBOR bytes
  uint32_t hash_value = 0;
  MurmurHash3_x86_32(encoded.ptr, static_cast<int>(encoded.len), 0,
                     &hash_value);

  // 5. Calculate bucket using high-precision 64-bit integer arithmetic
  uint64_t bucket = (static_cast<uint64_t>(hash_value) * sum_of_weights) >>
                    std::numeric_limits<uint32_t>::digits;

  // 6. Select variant
  uint64_t range_end = 0;
  for (const Distribution& dist : distributions) {
    range_end += dist.weight;
    if (bucket < range_end) {
      return dist.variant;
    }
  }

  return absl::InternalError("Fractional bucketing failed to find a variant");
}

}  // namespace flagd
