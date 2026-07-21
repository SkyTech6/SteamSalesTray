# Vendored third-party dependencies

These are compiled directly into `SteamSalesTray.exe`. No package manager is used.

| Library | Version   | License       | Files                                    |
|---------|-----------|---------------|------------------------------------------|
| SQLite  | 3.53.3    | Public domain | `sqlite/sqlite3.c`, `sqlite3.h`, `sqlite3ext.h` |
| yyjson  | 0.10.0    | MIT           | `yyjson/yyjson.c`, `yyjson.h` (see `yyjson/LICENSE`) |

## Updating

- **SQLite**: download the amalgamation zip from <https://www.sqlite.org/download.html>
  and replace the three files above.
- **yyjson**: replace `yyjson.c` / `yyjson.h` from the desired tag at
  <https://github.com/ibireme/yyjson>.

## Build notes

SQLite is configured via compile-time defines in the top-level `CMakeLists.txt`
(e.g. `SQLITE_OMIT_LOAD_EXTENSION`, `SQLITE_THREADSAFE`), not by editing the
amalgamation.
