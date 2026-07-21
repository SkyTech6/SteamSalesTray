#pragma once

#include <string>

namespace platform {

// UTF-8 <-> UTF-16 conversions. Return empty on failure.
std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);

// Formats a (possibly negative) integer with thousands separators, e.g.
// -1,842. Negatives are valid (refunds/corrections) and never clamped.
std::wstring FormatThousands(long long value);

}  // namespace platform
