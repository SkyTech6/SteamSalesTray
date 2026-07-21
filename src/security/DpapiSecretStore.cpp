#include "security/DpapiSecretStore.h"

#include <cstdint>
#include <cstring>
#include <vector>
#include <windows.h>
#include <wincrypt.h>

#include "platform/Paths.h"

#pragma comment(lib, "crypt32.lib")

namespace security {

namespace {
constexpr uint8_t kMagic[4] = {'S', 'S', 'T', 'K'};
constexpr uint16_t kFormatVersion = 1;
constexpr size_t kHeaderSize = 12;
// Sanity ceiling so a corrupt length field can't trigger a huge allocation.
constexpr uint32_t kMaxBlobBytes = 1u << 20;  // 1 MB

// Application-specific optional entropy (see plan §11). Not a security boundary
// on its own; must be identical for protect and unprotect.
constexpr char kEntropy[] = "SteamSalesTray.ApiKey.v1";

DATA_BLOB MakeEntropyBlob() {
    DATA_BLOB blob;
    blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy));
    blob.cbData = sizeof(kEntropy) - 1;  // exclude NUL terminator
    return blob;
}

// Writes `bytes` to `path` atomically: temp file, flush, replace, cleanup.
bool WriteFileAtomic(const std::wstring& path, const std::vector<uint8_t>& bytes) {
    const std::wstring tmp = path + L".tmp";

    HANDLE file = ::CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool ok = true;
    DWORD written = 0;
    if (!bytes.empty()) {
        ok = ::WriteFile(file, bytes.data(),
                         static_cast<DWORD>(bytes.size()), &written, nullptr) &&
             written == bytes.size();
    }
    if (ok) {
        ok = ::FlushFileBuffers(file) != 0;
    }
    ::CloseHandle(file);

    if (ok) {
        ok = ::MoveFileExW(tmp.c_str(), path.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }
    if (!ok) {
        ::DeleteFileW(tmp.c_str());
    }
    return ok;
}

bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        size.QuadPart > static_cast<LONGLONG>(kHeaderSize + kMaxBlobBytes)) {
        ::CloseHandle(file);
        return false;
    }
    out.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const bool ok = ::ReadFile(file, out.data(),
                               static_cast<DWORD>(out.size()), &read, nullptr) &&
                    read == out.size();
    ::CloseHandle(file);
    return ok;
}

void StoreU16(std::vector<uint8_t>& v, uint16_t value) {
    v.push_back(static_cast<uint8_t>(value & 0xFF));
    v.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void StoreU32(std::vector<uint8_t>& v, uint32_t value) {
    v.push_back(static_cast<uint8_t>(value & 0xFF));
    v.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

uint16_t LoadU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t LoadU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

DpapiSecretStore::DpapiSecretStore(std::wstring filePath)
    : filePath_(filePath.empty() ? platform::GetApiKeyPath()
                                 : std::move(filePath)) {}

bool DpapiSecretStore::Store(const SecureString& plaintextUtf8) const {
    if (filePath_.empty() || plaintextUtf8.Empty()) {
        return false;
    }

    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintextUtf8.Data()));
    in.cbData = static_cast<DWORD>(plaintextUtf8.Size());
    DATA_BLOB entropy = MakeEntropyBlob();
    DATA_BLOB out{};

    if (!::CryptProtectData(&in, L"SteamSalesTray API key", &entropy, nullptr,
                            nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return false;
    }
    if (out.cbData == 0 || out.cbData > kMaxBlobBytes) {
        if (out.pbData) ::LocalFree(out.pbData);
        return false;
    }

    std::vector<uint8_t> file;
    file.reserve(kHeaderSize + out.cbData);
    file.insert(file.end(), kMagic, kMagic + sizeof(kMagic));
    StoreU16(file, kFormatVersion);
    StoreU16(file, 0);  // reserved
    StoreU32(file, out.cbData);
    file.insert(file.end(), out.pbData, out.pbData + out.cbData);

    // DPAPI output must be released with LocalFree.
    ::SecureZeroMemory(out.pbData, out.cbData);
    ::LocalFree(out.pbData);

    return WriteFileAtomic(filePath_, file);
}

bool DpapiSecretStore::Load(SecureString& out) const {
    out.Wipe();
    if (filePath_.empty()) {
        return false;
    }

    std::vector<uint8_t> file;
    if (!ReadWholeFile(filePath_, file) || file.size() < kHeaderSize) {
        return false;
    }
    if (std::memcmp(file.data(), kMagic, sizeof(kMagic)) != 0) {
        return false;
    }
    if (LoadU16(file.data() + 4) != kFormatVersion) {
        return false;
    }
    const uint32_t blobLen = LoadU32(file.data() + 8);
    if (blobLen == 0 || blobLen > kMaxBlobBytes ||
        file.size() != kHeaderSize + blobLen) {
        return false;
    }

    DATA_BLOB in;
    in.pbData = file.data() + kHeaderSize;
    in.cbData = blobLen;
    DATA_BLOB entropy = MakeEntropyBlob();
    DATA_BLOB plain{};

    if (!::CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr,
                             CRYPTPROTECT_UI_FORBIDDEN, &plain)) {
        return false;
    }

    out.Assign(reinterpret_cast<const char*>(plain.pbData), plain.cbData);
    ::SecureZeroMemory(plain.pbData, plain.cbData);
    ::LocalFree(plain.pbData);
    return true;
}

bool DpapiSecretStore::HasStoredKey() const {
    if (filePath_.empty()) {
        return false;
    }
    std::vector<uint8_t> file;
    if (!ReadWholeFile(filePath_, file) || file.size() < kHeaderSize) {
        return false;
    }
    return std::memcmp(file.data(), kMagic, sizeof(kMagic)) == 0 &&
           LoadU16(file.data() + 4) == kFormatVersion;
}

bool DpapiSecretStore::Remove() const {
    if (filePath_.empty()) {
        return false;
    }
    if (::DeleteFileW(filePath_.c_str())) {
        return true;
    }
    return ::GetLastError() == ERROR_FILE_NOT_FOUND;
}

}  // namespace security
