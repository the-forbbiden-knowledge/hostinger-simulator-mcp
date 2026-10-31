"""Static regression checks for the optional, offline WebView2 source layout.

These checks inspect local source; they do not establish actual runtime isolation.
"""
import hashlib
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OfflineDesktopSourceTests(unittest.TestCase):
    def test_exact_supplied_icon(self):
        data = (ROOT / "ui" / "favicon.ico").read_bytes()
        self.assertEqual(
            hashlib.sha256(data).hexdigest(),
            "1a2fcbd95a4fcc84259a43e04c23478020927104981014dd30607277b8fafe9c",
        )

    def test_ui_bundled_and_csp_blocks_remote_connections(self):
        html = (ROOT / "ui" / "index.html").read_text(encoding="utf-8")
        self.assertIn("connect-src 'none'", html)
        self.assertIn("default-src 'none'", html)
        self.assertIn("frame-src 'none'", html)
        self.assertIn('src="app.js"', html)
        self.assertIn('href="styles.css"', html)

    def test_native_bridge_has_explicit_denial_and_external_resource_guard(self):
        src = (ROOT / "src" / "native" / "desktop_main.cpp").read_text(encoding="utf-8")
        for required in (
            "unsupported_read_only_fixture",
            "add_WebMessageReceived",
            "add_NavigationStarting",
            "add_NewWindowRequested",
            "AddWebResourceRequestedFilter",
            'L"Forbidden"',
            "is_local(source)",
            "is_local(address)",
            "SW_SHOWMAXIMIZED",
            "simulator::service_policy::report()",
        ):
            with self.subTest(required=required):
                self.assertIn(required, src)
        self.assertNotIn("ShellExecute", src)
        self.assertNotIn("CreateProcess", src)

    def test_webview_is_separate_from_cli(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("add_executable(simulator src/native/main.cpp)", cmake)
        self.assertIn("add_executable(simulator_desktop WIN32", cmake)
        self.assertIn("WEBVIEW2_SDK_DIR", cmake)
        self.assertIn("copy_directory", cmake)


if __name__ == "__main__":
    unittest.main()
