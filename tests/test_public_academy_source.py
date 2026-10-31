"""Public academy homepage regression: content-only layout and unchanged triangle."""
import hashlib
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PublicAcademyTests(unittest.TestCase):
    def test_only_the_requested_main_content_is_present(self):
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        self.assertIn('<main class="academy-content"', html)
        self.assertIn('class="site-header"', html)
        self.assertIn('class="site-footer"', html)
        self.assertNotIn('data-profile-banner', html)
        self.assertIn('id="ai-template"', html)
        self.assertIn('id="docs-template"', html)
        self.assertIn('class="light-theme"', html)
        self.assertIn('name="color-scheme" content="light dark"', html)

    def test_original_triangle_sweep_and_sway_without_flashes(self):
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        css = (ROOT / "academy.css").read_text(encoding="utf-8")
        hero = re.search(r'<svg[^>]*class="hero-brand-mark"[^>]*>(.*?)</svg>', html, re.S)
        self.assertIsNotNone(hero)
        self.assertEqual(hero.group(1).count("<path "), 16)
        self.assertIn('d="M485 650.4h230L600 451.21z"', html)
        for selector in (
            "brand-fade-scan",
            "academy-hero-coriolis-precession",
            "--triangle-scan-duration: 4.8s",
            "--triangle-precession-duration: 6.4s",
            "translate3d(-1px, 0, 0) rotate(-0.8deg) scale(0.995)",
            "translate3d(1px, 0, 0) rotate(0.8deg) scale(1.01)",
            ".hero-brand-mark", "animation:", "mask-position:",
            "transform-origin: center", "prefers-reduced-motion: reduce",
        ):
            with self.subTest(selector=selector):
                self.assertIn(selector, css)
        for removed in ("academy-hero-coriolis-flash", "steps(1, end)", "drop-shadow(", "brightness(0)"):
            with self.subTest(removed=removed):
                self.assertNotIn(removed, css)
        self.assertIn('data-triangle-effect="on"', html)
        self.assertIn('data-triangle-flashes="off"', html)

    def test_central_copy_without_courses_and_original_site_title(self):
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        css = (ROOT / "academy.css").read_text(encoding="utf-8")
        self.assertIn("<h1 id=\"academy-title\">A Elite Estrangeira</h1>", html)
        self.assertIn("Nós sabemos o que somos, onde podemos chegar e o que podemos realizar.", html)
        self.assertIn("Trazendo a verdadeira luz do conhecimento àqueles capazes de questionar o suficiente para buscá-la.", html)
        self.assertIn("<title>Hostinger Simulator - Windows Native Core + Elixir/OTP | hostinger-simulator-mcp</title>", html)
        self.assertNotIn("Cookie Editor", html)
        self.assertNotIn("courses-title", html)
        self.assertNotIn("academy-courses", html)
        self.assertNotIn("course-card", html)
        self.assertNotIn(".academy-courses", css)
        self.assertNotIn(".course-card", css)

    def test_original_icon_reused_without_new_private_assets(self):
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        self.assertIn('href="ui/favicon.ico"', html)
        self.assertEqual(
            hashlib.sha256((ROOT / "ui" / "favicon.ico").read_bytes()).hexdigest(),
            "1a2fcbd95a4fcc84259a43e04c23478020927104981014dd30607277b8fafe9c",
        )
        self.assertNotRegex(html, r'(?:href|src)="http://')
        self.assertEqual(set(re.findall(r'https://([^/"\\s]+)', html)), {"github.com"})
        css = (ROOT / "academy.css").read_text(encoding="utf-8")
        self.assertNotIn("@import", css)
        self.assertNotIn("url(", css)


if __name__ == "__main__":
    unittest.main()
