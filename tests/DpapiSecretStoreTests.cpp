// Unit tests for DPAPI-protected key storage (offline, hermetic).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <windows.h>

#include "security/DpapiSecretStore.h"
#include "security/SecureString.h"

using namespace security;

static int g_fail = 0;
#define CHECK(cond, msg)                                       \
    do {                                                       \
        if (cond) { std::wprintf(L"  PASS: %s\n", msg); }      \
        else { std::wprintf(L"  FAIL: %s\n", msg); ++g_fail; } \
    } while (0)

static std::vector<uint8_t> ReadAll(const std::wstring& p) {
    HANDLE h = ::CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, 0, nullptr);
    std::vector<uint8_t> v;
    if (h == INVALID_HANDLE_VALUE) return v;
    DWORD sz = ::GetFileSize(h, nullptr);
    v.resize(sz);
    DWORD rd = 0;
    ::ReadFile(h, v.data(), sz, &rd, nullptr);
    ::CloseHandle(h);
    return v;
}
static void WriteAll(const std::wstring& p, const std::vector<uint8_t>& v) {
    HANDLE h = ::CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             0, nullptr);
    DWORD wr = 0;
    ::WriteFile(h, v.data(), (DWORD)v.size(), &wr, nullptr);
    ::CloseHandle(h);
}

int main() {
    wchar_t tmp[MAX_PATH];
    ::GetTempPathW(MAX_PATH, tmp);
    std::wstring path = std::wstring(tmp) + L"sst_ut_key.dat";
    ::DeleteFileW(path.c_str());

    DpapiSecretStore store(path);
    const char* secret = "ABCD1234EF567890ABCD1234EF567890";

    SecureString in;
    in.Assign(secret, strlen(secret));
    CHECK(store.Store(in), L"Store() succeeds");
    CHECK(store.HasStoredKey(), L"HasStoredKey() true after store");
    SecureString out;
    CHECK(store.Load(out), L"Load() succeeds");
    CHECK(out.Size() == strlen(secret) &&
              memcmp(out.Data(), secret, out.Size()) == 0,
          L"decrypted bytes match original");

    auto bytes = ReadAll(path);
    CHECK(bytes.size() >= 6 && bytes[0] == 'S' && bytes[1] == 'S' &&
              bytes[2] == 'T' && bytes[3] == 'K',
          L"magic == SSTK");
    bool leak = false;
    for (size_t i = 0; i + strlen(secret) <= bytes.size(); ++i)
        if (memcmp(&bytes[i], secret, strlen(secret)) == 0) { leak = true; break; }
    CHECK(!leak, L"plaintext key not present in file");

    auto tampered = bytes;
    tampered[tampered.size() - 1] ^= 0xFF;
    WriteAll(path, tampered);
    SecureString t;
    CHECK(!store.Load(t), L"Load() fails on tampered blob");

    WriteAll(path, bytes);
    CHECK(store.Remove(), L"Remove() succeeds");
    CHECK(!store.HasStoredKey(), L"HasStoredKey() false after remove");
    CHECK(store.Remove(), L"Remove() idempotent when absent");

    std::wprintf(L"\n%s (failures=%d)\n",
                 g_fail ? L"TESTS FAILED" : L"ALL TESTS PASSED", g_fail);
    return g_fail ? 1 : 0;
}
