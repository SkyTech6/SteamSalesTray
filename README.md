# Steam Sales Tray

A lightweight, native Windows 10/11 system-tray application that retrieves sales
data from the **Steamworks Partner Financial Web API** and displays **net units
sold per Steam product** from the notification area.

- Native Win32, C++20, x64 only. No frameworks, runtimes, browsers, or webviews.
- No persistent window or taskbar entry while idle; lives in the tray.
- Incremental sync via Steam high-water marks (no repeated full-history downloads).
- API key protected at rest with Windows **DPAPI** (bound to your Windows user profile on this machine).
- Near-zero CPU usage while idle.

## Who this is for

Steamworks partners who want a lightweight desktop check on product sales
without keeping browser dashboards open.

## Building

Requires **Visual Studio 2022** Build Tools (Desktop development with C++) and
**CMake 3.20+**.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The executable is produced at `build\Release\SteamSalesTray.exe`.

### Tests

Unit tests (offline, hermetic) run via CTest:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Covers JSON parsing, aggregation + per-date replacement, DPAPI round-trip /
tamper detection, and log redaction/rotation.

## Releasing (maintainers)

Releases are published through the GitHub Actions workflow at
`.github/workflows/release.yml`.

Workflow behavior:

- Triggered manually from the Actions tab (workflow_dispatch only).
- Requires a semantic version input in `X.Y.Z` format.
- Creates and pushes an annotated tag named `vX.Y.Z`.
- Builds the full Release configuration with CMake.
- Runs the full CTest suite and fails the release on any failing test.
- Publishes a full GitHub Release and uploads one asset named
  `SteamSalesTray.exe`.

How to run a release:

1. Open **Actions** in GitHub and choose **Release Windows EXE**.
2. Click **Run workflow**.
3. Enter `version` as `X.Y.Z` (for example `0.2.0`).
4. Optionally set `target_ref` to a branch, tag, or commit SHA.
5. Run the workflow and wait for completion.

If the workflow fails:

- Version rejected: ensure `version` is exactly `X.Y.Z` with no `v` prefix.
- Tag already exists: choose a new version.
- Test failure: inspect the failed test output in the workflow logs.
- Missing executable: verify CMake build succeeded for Release.

### Running

```text
SteamSalesTray.exe             # normal tray mode
SteamSalesTray.exe --settings  # open settings on the running instance
SteamSalesTray.exe --refresh   # trigger a manual refresh
SteamSalesTray.exe --background # startup mode (used by the Run registry entry)
```

## Data location

All mutable data lives under `%LOCALAPPDATA%\SteamSalesTray\`:

```text
sales.db      SQLite cache of daily product sales + sync state
api-key.dat   DPAPI-protected Financial API key
app.log       Bounded diagnostic log (secrets redacted)
```

## Security note

This tool deliberately omits Steam's recommended IP whitelisting. DPAPI protects
the Financial API key at rest and generally binds decryption to the same Windows
user and machine. It does **not** protect the key from malware or another process
running with the same user's authority. Treat the Financial API key as a
high-value password.

## Known limitations

- **Steamworks is the source of truth.** This app is a convenience view over
  downloaded financial data. Spot-check totals in Steamworks reporting if you
  are validating payout-critical numbers.
- **Product names populate incrementally.** Names come from the `app_info`
  lookup block. Any product with sales on a subsequently-synced date gets its
  real name; dormant titles synced before this was wired show `App <id>` until
  they next sell or you run **Settings → Clear Local Sales Cache** (full
  resync).
- **First sync can take longer.** Initial backfill may process many historical
  dates; later syncs are incremental.

## Third-party dependencies

Vendored as source under `third_party/` — no package manager required:

- **SQLite** (public domain amalgamation) — local sync state & sales storage
- **yyjson** (MIT) — fast C JSON parser for Steam API responses

TLS/HTTPS is provided by the OS via WinHTTP; no bundled TLS library is used.
