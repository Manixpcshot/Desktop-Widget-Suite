# Desktop Widget Suite — v0.1.1

Native C++20 desktop utility for Windows 10 (1703+) / Windows 11. **No WebView, Electron or browser UI.**

## What changed since v0.1.0

- Five separate-looking **black floating orbs**, white vector icons, soft individual shadows and genuinely transparent gaps. No rectangular panel background. Per-pixel premultiplied alpha is presented with `UpdateLayeredWindow`; native GDI+ draws this surface (the Direct2D/DirectWrite renderer remains available for future tools).
- Independent spring slide/scale per orb, staggered entrance and spring hover enlargement. Clicks hit actual circular shapes, not full rectangular rows.
- The orbs open on launch; launching again shows the existing instance. Tray → Show pins them, Escape closes them, hovering a chosen screen edge reveals them again. Right-click an orb for Settings.
- Functional **local calendar/live clock**, **SQLite-backed autosaving Unicode quick note**, and **fit-to-window native JPG/PNG/BMP image viewer**.
- **Run on Windows sign-in** and **automatic stable GitHub updates**, both enabled by default and switchable in Settings → Apply. Tray/Settings also provide a manual update check.

**Not a complete suite:** AI Chat is explicitly unavailable; no API key is collected. The photo tool is an image viewer, not yet an editor. Calendar is currently Gregorian, not the requested Persian/Islamic conversion. Notes are a single quick-note window, not a complete widgets manager. Automatic OS theme detection and full UI Automation remain future work.

## Install and use

Download the x64 EXE or NSIS installer from GitHub Releases. Run once. The executable creates/launches its per-user managed copy:

```
%LOCALAPPDATA%\DesktopSuite\bin\DesktopSuite.exe
```

This lets updates replace the app **without administrator privileges**, including when the launcher was installed under Program Files. Later launches from the original EXE redirect to the managed copy; a newer manually downloaded EXE can upgrade it when the application is closed. A second instance brings the existing panel into view.

Settings and notes: `%LOCALAPPDATA%\DesktopSuite\suite.db`. Update/startup preferences: `HKCU\Software\DesktopSuite`. Sign-in entry: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\DesktopSuite` (quoted managed EXE path).

Five icons from top to bottom: AI (unavailable), image viewer, calendar/clock, quick notes, settings. Up/Down/Tab selects, Enter/Space activates, Escape closes. Use tray → Quit to fully exit; closing a utility window only hides it. Notes autosave changed text every 1.5 seconds and when closing/quitting.

## Update behavior and trust boundary

- Checks `https://api.github.com/repos/Manixpcshot/Desktop-Widget-Suite/releases/latest` on launch and hourly, using native WinHTTP on a worker thread. No telemetry, API keys or GitHub tokens. Public API rate limits/network failures are logged, never treated as an update.
- Accepts only strictly newer stable `vMAJOR.MINOR.PATCH` releases; ignores drafts/prereleases, missing assets and downgrades.
- Downloads **only `DesktopSuite.exe`** from this repository's exact release URL over HTTPS. Limits metadata to 1 MiB and the EXE to 64 MiB. Requires GitHub's `sha256:` asset digest, checks length, PE magic, file-resource version and SHA256 before staging.
- **Trusts the repository owner and GitHub HTTPS/digest metadata**, not an independent signing key. Binaries are not yet Authenticode-signed. This is integrity checking, not an independent publisher-signature guarantee.
- Verified background updates automatically restart the application. A manual check asks before restarting. Notes flush on graceful exit. Turn auto-update off in Settings if automatic restarts are unwanted.
- The staged new EXE runs as a native helper, waits for the current PID to exit, preserves `bin\DesktopSuite.exe.bak`, copies itself to the fixed managed path, and relaunches it. Copy/launch errors restore the old executable where possible. There is **no shell command or PowerShell updater**.
- A launch that immediately crashes after successful `CreateProcess` is **not automatically detected/rolled back**. Keep the backup for manual recovery. Network shutdown can wait for a bounded in-flight request; it never blocks the interactive message loop while checking.
- **v0.1.0 did not contain an updater**: install v0.1.1 manually once. Future stable releases with valid assets can then install automatically. Publishing commits alone is not an update; a newer GitHub Release is required.

To remove: Quit from tray, then uninstall the launcher. The NSIS uninstaller refuses to continue while the app window exists, removes the current user’s sign-in entry and managed `bin`/`updates` copies, and preserves the SQLite database and preferences. For a standalone EXE, disable Run on sign-in and Auto-update, Apply, Quit, then delete `%LOCALAPPDATA%\DesktopSuite`. To erase all saved data after uninstall, delete that directory and optionally `HKCU\Software\DesktopSuite`. Other Windows users’ per-user copies are not removed.

## Build

Visual Studio 2022 C++ workload + Windows SDK, CMake 3.21+, Git, vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg C:/dev/vcpkg
C:/dev/vcpkg/bootstrap-vcpkg.bat
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/Release/DesktopSuite.exe
```

vcpkg installs static SQLite and nlohmann/json. WinHTTP, BCrypt, GDI+, common controls and Direct2D/DirectWrite are Windows system libraries. No external HTTP DLLs. Startup failures use a message box; later update/render/persistence errors go to SQLite `errors`.

Portable tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests cover springs/bounce, stagger, negative-coordinate monitor edges, strict release version comparisons and digest syntax. Windows additionally runs SQLite settings/Unicode note round-trips and `scripts/windows_smoke.ps1`: real window creation, calendar/note click dispatch, per-user bootstrap, graceful exit, native staged update replacement/relaunch, backup/hash verification and per-pixel renderer error-log checks. This is **not** an end-to-end test of downloading a future GitHub release or a visual smoothness/accessibility certification.

## Release and remaining manual checks

Tag a tested commit `vX.Y.Z`; GitHub Actions builds/tests and uploads `DesktopSuite.exe`, NSIS installer and Persian RTL Excel notes. Keep CMake/resource/manifest versions synchronized. The release's exact standalone EXE filename and GitHub digest must be retained for auto-updates. Runner-provided vcpkg is not revision-pinned yet.

Before a production release, manually verify actual appearance/smoothness, mixed-DPI/negative-coordinate monitors, high-contrast/accessibility, desktop click-through gaps, suspend/resume, proxy/offline behavior, real newer-release download/restart, and uninstall cleanup. Automated CI cannot establish all of these properties.
