#pragma once

#include <string>

#include "security/SecureString.h"

namespace security {

// Stores the Steam Financial API key at rest, protected with Windows DPAPI
// (current user + machine scope; CRYPTPROTECT_UI_FORBIDDEN, no LOCAL_MACHINE).
//
// On-disk envelope (see plan §11):
//   0  4  Magic "SSTK"
//   4  2  Format version (1)
//   6  2  Reserved
//   8  4  DPAPI blob length
//   12 N  DPAPI-protected blob
//
// The file is written atomically (temp file + flush + MoveFileEx replace).
class DpapiSecretStore {
public:
    // Path defaults to platform::GetApiKeyPath(); overridable for tests.
    explicit DpapiSecretStore(std::wstring filePath = {});

    // Protects `plaintextUtf8` and writes it atomically. Returns false on any
    // DPAPI or file error.
    bool Store(const SecureString& plaintextUtf8) const;

    // Decrypts the stored key into `out`. Returns false if absent, tampered, or
    // undecryptable (e.g. different user/machine).
    bool Load(SecureString& out) const;

    // True if a structurally valid key file exists (does not decrypt).
    bool HasStoredKey() const;

    // Deletes the key file. Returns true if it no longer exists afterward.
    bool Remove() const;

    const std::wstring& FilePath() const { return filePath_; }

private:
    std::wstring filePath_;
};

}  // namespace security
