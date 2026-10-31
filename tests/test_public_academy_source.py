"""Original academy source integrity and sanctioned public homepage substitutions."""
import hashlib
import re
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
HTML = (ROOT / "index.html").read_bytes()


def span(start, end):
    a = HTML.index(start)
    return HTML[a:HTML.index(end, a) + len(end)]


class OriginalAcademySourceTests(unittest.TestCase):
    def test_banner_byte_for_byte_identical_to_user_archive(self):
        banner = span(b'<aside class="flex z-30', b'</aside>')
        self.assertEqual(len(banner), 2048)
        self.assertEqual(hashlib.sha256(banner).hexdigest(),
                         "a86d41a0c56b561f63bea5b3643e7c381a7e2d212def298f20dcfea919697825")

    def test_original_header_unmodified(self):
        header = span(b'<header class=', b'</header>')
        self.assertEqual(hashlib.sha256(header).hexdigest(),
                         "d3087318b9f3b37e407ff588af09260abd8beec93dd47deb99f1eb4f9f187468")
        self.assertIn(b'navigation-menu_root__HT1ea', header)
        self.assertIn(b'aria-label="Op', header)

    def test_original_grid_and_project_only_copy_change(self):
        html = HTML.decode("utf-8")
        self.assertIn('>Projeto</h2>', html)
        self.assertIn('>Hostinger Simulator MCP</h3>', html)
        self.assertIn('src="public-original.js"', html)
        self.assertIn('href="triangle-motion.css"', html)
        self.assertIn('href="ui/favicon.ico"', html)
        self.assertNotIn('Como importar e exportar cookies com o Cookie Editor', html)
        self.assertNotIn('src="/src/js/site.js', html)
        self.assertIn('light-theme', html)

    def test_original_triangle_svg_preserved(self):
        html = HTML.decode("utf-8")
        hero = re.search(r'<svg[^>]+class="triangle-module__d-3XEG__triangle hero-brand-mark"[^>]*>(.*?)</svg>', html, re.S)
        self.assertIsNotNone(hero)
        self.assertEqual(hero.group(1).count("<path "), 16)
        css = (ROOT / "triangle-motion.css").read_text("utf-8")
        self.assertIn("brand-fade-scan", css)
        self.assertIn("academy-hero-coriolis-precession", css)
        self.assertNotIn("academy-hero-coriolis-flash", css)
        self.assertIn("prefers-reduced-motion", css)

    def test_original_assets_declared_without_checkout_newline_conversion(self):
        # A Windows runner may use core.autocrlf=true; these files have byte-level
        # SHA-256 contracts that require identical checkout bytes on all runners.
        attrs = (ROOT / ".gitattributes").read_text(encoding="utf-8")
        for rule in (
            "/index.html -text", "/src/css/*.css -text",
            "/public-original.js -text", "/triangle-motion.css -text",
        ):
            with self.subTest(rule=rule):
                self.assertIn(rule, attrs.splitlines())

    def test_original_core_styles_and_icon_not_modified(self):
        self.assertEqual(hashlib.sha256((ROOT / "src/css/site.css").read_bytes()).hexdigest(),
                         "445a589e7b28abc4ab6d004bacb06e1cad869bade4fdd5bde41f5348029ba3c5")
        self.assertEqual(hashlib.sha256((ROOT / "src/css/742a1c022f0bfbbe.css").read_bytes()).hexdigest(),
                         "d7862c5b6ba53289872c61909574c4fafffeff7a73bcc1e5ed77a6050550222d")
        self.assertEqual(hashlib.sha256((ROOT / "ui/favicon.ico").read_bytes()).hexdigest(),
                         "1a2fcbd95a4fcc84259a43e04c23478020927104981014dd30607277b8fafe9c")


if __name__ == "__main__":
    unittest.main()
