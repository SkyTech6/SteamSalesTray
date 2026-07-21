#pragma once

namespace platform {

// Manages the per-user "start at sign-in" preference via
// HKCU\Software\Microsoft\Windows\CurrentVersion\Run (no admin required).
namespace StartupManager {

// True if the Run value exists and points at the current executable.
bool IsEnabled();

// Writes the Run value: "<exe>" --background. Returns false on failure.
bool Enable();

// Removes the Run value. Returns true if it no longer exists afterward.
bool Disable();

}  // namespace StartupManager
}  // namespace platform
