# Windows WebView2 — offline desktop boundary

The existing `simulator.exe` command-line engine and its CTest contracts remain the source of truth. The optional Windows desktop client is a **read-only local fixture viewer**, not a Hostinger or Zimbra integration and not an MCP transport.

## Launch and safety contract

- A distinct `simulator_desktop.exe` Windows GUI target embeds Microsoft Edge WebView2 where the WebView2 SDK is available at build time. It must not replace `simulator.exe` or change its CLI behavior.
- The local HTML/CSS/JavaScript assets are shipped as repository-owned source; the WebView2 host must only navigate to its virtual local hostname. No external URL, remote stylesheet, telemetry, popup, download, account session, mail delivery, DNS write, restore or billing operation.
- Only explicit, enumerated **read-only** native service fixture requests can cross the JavaScript-to-native bridge. Unknown messages are rejected and never interpreted as arbitrary shell, URL, or tool calls.
- The supplied `favicon.ico` is embedded in the Windows executable resource and used by the taskbar/window. Startup is maximized, with normal user ability to restore/minimize/close.
- The native WebView2 Runtime is a prerequisite. SDK files, proprietary archives, private customer references, credentials and other non-redistributable assets must never be committed.
- The hosted-vs-managed feature matrix remains synthetic. Provider-specific contractual terms, financial prices and uptime assertions are **not** represented as promises or operational SLAs.

## Verification

Use the existing CMake presets, `ctest`, Python safety/publication gates and smoke-test the GUI on Windows with a WebView2 Runtime installed. The desktop UI is not a live provider and must not claim backend side effects. Commit messages must be exactly `Initial commit`, using normal fast-forward pushes.
