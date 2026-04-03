#include "flagd_ops.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "flagd/evaluator/murmur_hash/MurmurHash3.h"
#include "qcbor/qcbor.h"

namespace flagd {

namespace {

// Checks if a string has a leading zero (and is not just "0").
bool HasLeadingZero(std::string_view str) {
  return str.size() > 1 && str[0] == '0';
}

// Parses number according to SemVer 2.0.0 specification.
absl::Status ParseSemVerNum(std::string_view num_str, std::string_view name,
                            uint64_t* out) {
  if (HasLeadingZero(num_str)) {
    return absl::InvalidArgumentError(absl::StrCat(
        name, " version MUST NOT contain leading zeros: ", num_str));
  }
  if (!absl::SimpleAtoi(num_str, out)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid SemVer ", name, " digits: ", num_str));
  }
  return absl::OkStatus();
};

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

    uint64_t major = 0;
    uint64_t minor = 0;
    uint64_t patch = 0;
    if (absl::Status status = ParseSemVerNum(core_parts[0], "Major", &major);
        !status.ok()) {
      return status;
    }

    if (core_parts.size() >= 2) {
      if (absl::Status status = ParseSemVerNum(core_parts[1], "Minor", &minor);
          !status.ok()) {
        return status;
      }
    }

    if (core_parts.size() == 3) {
      if (absl::Status status = ParseSemVerNum(core_parts[2], "Patch", &patch);
          !status.ok()) {
        return status;
      }
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

        bool is_numeric = std::all_of(ident.begin(), ident.end(), ::isdigit);
        if (is_numeric && HasLeadingZero(ident)) {
          return absl::InvalidArgumentError(
              "Numeric pre-release identifiers MUST NOT contain leading zeros");
        }

        pre_release.emplace_back(ident);
      }
    }

    return SemanticVersion(major, minor, patch, std::move(pre_release));
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
          std::all_of(lhs_part.begin(), lhs_part.end(), ::isdigit);
      bool rhs_is_num =
          std::all_of(rhs_part.begin(), rhs_part.end(), ::isdigit);

      if (lhs_is_num && rhs_is_num) {
        uint64_t lhs_num;
        uint64_t rhs_num;
        // Numeric identifiers are compared numerically.
        if (absl::SimpleAtoi(lhs_part, &lhs_num) &&
            absl::SimpleAtoi(rhs_part, &rhs_num)) {
          if (lhs_num != rhs_num) return lhs_num < rhs_num ? -1 : 1;
        } else {
          // Fallback to lexicographical comparison if parsing fails (e.g.,
          // overflow)
          if (lhs_part != rhs_part) return lhs_part < rhs_part ? -1 : 1;
        }
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
  std::string variant;
  int32_t weight;
};

// Helper function to verify the CBOR encoding and hash for a test case.
// Expects 'id' and 'path' to be present in the data (evaluation context)
// to perform verification against files.
bool VerifyTest(const nlohmann::json& data, uint32_t calculated_hash,
                const std::vector<uint8_t>& calculated_cbor) {
  if (!data.is_object() || !data.contains("id") || !data.contains("path")) {
    return true;
  }

  std::string test_id;
  if (data["id"].is_string()) {
    test_id = data["id"].get<std::string>();
  } else if (data["id"].is_number()) {
    test_id = std::to_string(data["id"].get<int>());
  } else {
    return true;
  }

  if (!data["path"].is_string()) return true;
  std::string test_path = data["path"].get<std::string>();

  std::string hash_file = test_path + "/" + test_id + ".hash";
  std::string cbor_file = test_path + "/" + test_id + ".cbor";

  if (!std::filesystem::exists(hash_file) ||
      !std::filesystem::exists(cbor_file)) {
    return true;
  }

  try {
    std::ifstream c_file(cbor_file, std::ios::binary);
    std::vector<uint8_t> expected_cbor((std::istreambuf_iterator<char>(c_file)),
                                       std::istreambuf_iterator<char>());

    if (calculated_cbor != expected_cbor) {
      std::stringstream ss_calc;
      ss_calc << std::hex << std::setfill('0');
      for (uint8_t byte : calculated_cbor) {
        ss_calc << std::setw(2) << static_cast<int>(byte);
      }

      std::stringstream ss_exp;
      ss_exp << std::hex << std::setfill('0');
      for (uint8_t byte : expected_cbor) {
        ss_exp << std::setw(2) << static_cast<int>(byte);
      }

      std::cout << "VERIFY_TEST FAIL: test " << test_id
                << ", cbor mismatch: expected " << ss_exp.str() << ", got "
                << ss_calc.str() << "\n";
      return false;
    }

    std::ifstream h_file(hash_file);
    std::string expected_hash;
    h_file >> expected_hash;
    // Remove quotes if present
    if (!expected_hash.empty() && expected_hash.front() == '"') {
      expected_hash.erase(0, 1);
    }
    if (!expected_hash.empty() && expected_hash.back() == '"') {
      expected_hash.pop_back();
    }

    std::stringstream ss_hash;
    ss_hash << std::hex << std::setw(sizeof(calculated_hash) * 2)
            << std::setfill('0') << calculated_hash;
    std::string actual_hash = ss_hash.str();

    if (actual_hash != expected_hash) {
      std::cout << "VERIFY_TEST FAIL: test " << test_id
                << ", hash mismatch: expected " << expected_hash << ", got "
                << actual_hash << "\n";
      return false;
    }

    return true;
  } catch (const std::exception& e) {
    std::cout << "VERIFY_TEST ERROR: test " << test_id << ", " << e.what()
              << "\n";
    return false;
  }
}

// CBOR Canonical sorting: length first, then lexicographical
bool CompareCborKeys(const std::string& lhs, const std::string& rhs) {
  if (lhs.length() != rhs.length()) {
    return lhs.length() < rhs.length();
  }
  return lhs < rhs;
}

void EncodeJson(QCBOREncodeContext* enc_ctx, const nlohmann::json& data) {
  if (data.is_object()) {
    QCBOREncode_OpenMap(enc_ctx);
    std::vector<std::string> keys;
    for (auto it = data.begin(); it != data.end(); ++it) {
      keys.push_back(it.key());
    }
    std::sort(keys.begin(), keys.end(), CompareCborKeys);
    for (const auto& key : keys) {
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
  } else if (data.is_number_integer()) {
    QCBOREncode_AddInt64(enc_ctx, data.get<int64_t>());
  } else if (data.is_number_unsigned()) {
    QCBOREncode_AddUInt64(enc_ctx, data.get<uint64_t>());
  } else if (data.is_number_float()) {
    double val = data.get<double>();
    if (std::trunc(val) == val && val <= static_cast<double>(INT64_MAX) &&
        val >= static_cast<double>(INT64_MIN)) {
      QCBOREncode_AddInt64(enc_ctx, static_cast<int64_t>(val));
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
  if (operation == "^") return ver1.GetMajor() == ver2.GetMajor();
  if (operation == "~") {
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

  if (values.size() < 2) {
    return absl::InvalidArgumentError(
        "fractional evaluation data has length under 2");
  }

  // 1. Get the target property value used for bucketing the values
  absl::StatusOr<nlohmann::json> bucketing_property_eval =
      eval.Apply(values[0], data);
  if (!bucketing_property_eval.ok()) return bucketing_property_eval.status();

  nlohmann::json bucketing_property_value;
  bool first_value_used = false;
  if (bucketing_property_eval.value().is_array()) {
    // Fallback logic from spec: Concatenate flagKey and targetingKey if
    // property is missing
    std::string flag_key;
    if (data.contains("$flagd") && data["$flagd"].is_object() &&
        data["$flagd"].contains("flagKey") &&
        data["$flagd"]["flagKey"].is_string()) {
      flag_key = data["$flagd"]["flagKey"].get<std::string>();
    }

    std::string targeting_key;
    if (data.contains("targetingKey") && data["targetingKey"].is_string()) {
      targeting_key = data["targetingKey"].get<std::string>();
    }
    bucketing_property_value = absl::StrCat(flag_key, targeting_key);
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

    if (!item.value()[0].is_string()) {
      return absl::InvalidArgumentError("Variant name must be a string");
    }

    int32_t weight = 1;
    if (item.value().size() >= 2 && item.value()[1].is_number()) {
      weight = item.value()[1].get<int32_t>();
      if (weight < 0) {
        return absl::InvalidArgumentError("Weight must be non-negative.");
      }
    }

    distributions.push_back({item.value()[0].get<std::string>(), weight});
    sum_of_weights += weight;
  }

  if (distributions.empty()) {
    return absl::InvalidArgumentError("No distributions found");
  }

  if (sum_of_weights == 0) {
    return absl::InvalidArgumentError("Sum of weights must be positive");
  }

  if (sum_of_weights >=
      static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
    return absl::InvalidArgumentError("Sum of weights exceeds maximum limit");
  }

  // 3. Serialize hashing value to cbor representation using qcbor.
  QCBOREncodeContext encode_ctx;
  UsefulBufC size_info;
  QCBORError err;

  QCBOREncode_Init(&encode_ctx, SizeCalculateUsefulBuf);

  EncodeJson(&encode_ctx, bucketing_property_value);

  err = QCBOREncode_Finish(&encode_ctx, &size_info);
  if (err != QCBOR_SUCCESS) {
    std::cerr << "Size calculation failed: " << err << "\n";
    return 1;
  }

  size_t required_size = size_info.len;
  std::cout << "Calculated exact size: " << required_size << " bytes.\n";

  // Create a vector perfectly sized to the required bytes
  std::vector<uint8_t> buffer(required_size);

  // Map the vector to a QCBOR UsefulBuf
  UsefulBuf cbor_buffer = {buffer.data(), buffer.size()};

  // Initialize the encoder again, this time with the real memory
  QCBOREncode_Init(&encode_ctx, cbor_buffer);

  EncodeJson(&encode_ctx, bucketing_property_value);

  UsefulBufC encoded;
  err = QCBOREncode_Finish(&encode_ctx, &encoded);
  if (err != QCBOR_SUCCESS) {
    return absl::InternalError(absl::StrCat("QCBOR encoding failed: ", err));
  }

  std::vector<std::uint8_t> hashing_object(
      static_cast<const uint8_t*>(encoded.ptr),
      static_cast<const uint8_t*>(encoded.ptr) + encoded.len);

  // 3. Calculate hash and determine bucket
  uint32_t hash_value;
  MurmurHash3_x86_32(hashing_object.data(),
                     static_cast<int>(hashing_object.size()), 0, &hash_value);

  if (!VerifyTest(data, hash_value, hashing_object)) {
    return absl::InternalError("Fractional verification failed");
  }

  // High-precision bucketing using 64-bit math to distribute hash over
  // sum_of_weights
  uint64_t bucket = (static_cast<uint64_t>(hash_value) * sum_of_weights) >>
                    std::numeric_limits<uint32_t>::digits;

  // 4. Return the variant corresponding to the bucket
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
