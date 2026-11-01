"""Static contracts for the GitHub Releases updater and portable release workflow."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UpdateContractTests(unittest.TestCase):
    def test_version_is_semver_and_release_is_preview(self):
        version = (ROOT / "VERSION.txt").read_text(encoding="ascii").strip()
        self.assertRegex(version, r"^\d+\.\d+\.\d+$")
        workflow = (ROOT / ".github/workflows/release-windows.yml").read_text(encoding="utf-8")
        self.assertIn('prerelease = $true', workflow)
        self.assertIn('contents: write', workflow)
        self.assertIn('"VERSION.txt"', workflow)
        self.assertIn('group: windows-release-${{ github.ref }}', workflow)
        self.assertIn('GITHUB_TOKEN: ${{ github.token }}', workflow)
        self.assertNotRegex(workflow, r'\$\{\s+github\.')

    def test_updater_uses_fixed_github_release_origin_and_digest(self):
        updater = (ROOT / "tools/update.ps1").read_text(encoding="utf-8")
        self.assertIn("https://api.github.com/repos/", updater)
        self.assertIn("hostinger-simulator-mcp/releases?per_page=20", updater)
        self.assertIn("https://github.com/", updater)
        self.assertIn("/hostinger-simulator-mcp/releases/download/", updater)
        self.assertIn("hostinger-simulator-windows-x64.zip", updater)
        self.assertIn("sha256:[a-f0-9]{64}", updater)
        self.assertIn("Get-FileHash", updater)
        self.assertIn("browser_download_url", updater)
        self.assertNotIn("http://", updater)
        self.assertNotIn("Invoke-Expression", updater)
        self.assertNotIn("Start-Process powershell", updater)

    def test_archive_validation_and_transactional_rollback_exist(self):
        updater = (ROOT / "tools/update.ps1").read_text(encoding="utf-8")
        for token in (
            "Assert-SafeArchive", "HashSet[string]", "Caminho inseguro",
            "tools/update.ps1", "Install-PackageTransaction",
            "Restore-PackageTransaction", "HostingerSimulator\\rollback",
            "CloseMainWindow()", "WaitForExit(30000)",
        ):
            with self.subTest(token=token):
                self.assertIn(token, updater)

    def test_native_bridge_exposes_only_fixed_update_command(self):
        native = (ROOT / "src/native/desktop_main.cpp").read_text(encoding="utf-8")
        js = (ROOT / "ui/app.js").read_text(encoding="utf-8")
        html = (ROOT / "ui/index.html").read_text(encoding="utf-8")
        self.assertIn('command == L"app.update"', native)
        self.assertIn("ShellExecuteW", native)
        self.assertIn("powershell.exe", native)
        self.assertIn('data-command="app.update"', html)
        self.assertIn('"app.update"', js)
        self.assertIn("connect-src 'none'", html)
        self.assertIn("simulator.invalid", native)
        self.assertNotIn("fetch(", js)
        self.assertNotIn("XMLHttpRequest", js)

    def test_release_workflow_builds_desktop_and_verifies_sdk_and_asset_digest(self):
        workflow = (ROOT / ".github/workflows/release-windows.yml").read_text(encoding="utf-8")
        for token in (
            "1.0.4314.35",
            "90778deb1b77e36292828b4b5833c9255c27533f40e0ddd55891ae718944edc4",
            "cmake --build --preset windows-msvc --config Release",
            "ctest --preset windows-msvc",
            "hostinger-simulator-windows-x64.zip",
            "PACKAGE_SHA256",
            "zipAsset.digest",
            "prerelease = $true",
        ):
            with self.subTest(token=token):
                self.assertIn(token, workflow)

    def test_cmake_copies_updater_and_version_beside_desktop_binary(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("tools/update.ps1", cmake)
        self.assertIn("VERSION.txt", cmake)
        self.assertIn("shell32", cmake)


if __name__ == "__main__":
    unittest.main()
