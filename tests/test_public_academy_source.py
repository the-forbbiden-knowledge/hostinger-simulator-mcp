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
        self.assertNotRegex(html, r"<(?:header|footer|nav|aside)\\b")
        self.assertNotIn('data-profile-banner', html)
        self.assertNotRegex(html, r"<script\\b")
        self.assertIn('class="light-theme"', html)
        self.assertIn('name="color-scheme" content="light"', html)

    def test_original_triangle_and_animation_survive(self):
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        css = (ROOT / "academy.css").read_text(encoding="utf-8")
        self.assertEqual(html.count("<path "), 16)
        self.assertIn('d="M485 650.4h230L600 451.21z"', html)
        for selector in (
            "brand-fade-scan", "academy-hero-coriolis-flash",
            "academy-hero-coriolis-precession", ".hero-brand-mark",
            "animation:", "mask-position:", "steps(1, end)"
        ):
            with self.subTest(selector=selector):
                self.assertIn(selector, css)
        self.assertIn('data-triangle-effect="on"', html)
        self.assertIn('data-triangle-flashes="on"', html)

    def test_central_copy_and_course_are_preserved(self):
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        self.assertIn("<h1 id=\"academy-title\">A Elite Estrangeira</h1>", html)
        self.assertIn("Nós sabemos o que somos, onde podemos chegar e o que podemos realizar.", html)
        self.assertIn("Trazendo a verdadeira luz do conhecimento àqueles capazes de questionar o suficiente para buscá-la.", html)
        self.assertIn("<h2 id=\"courses-title\">Cursos</h2>", html)
        self.assertIn("Como importar e exportar cookies com o Cookie Editor", html)

    def test_original_icon_reused_without_new_private_assets(self):
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        self.assertIn('href="ui/favicon.ico"', html)
        self.assertEqual(
            hashlib.sha256((ROOT / "ui" / "favicon.ico").read_bytes()).hexdigest(),
            "1a2fcbd95a4fcc84259a43e04c23478020927104981014dd30607277b8fafe9c",
        )
        self.assertNotIn("http://", html)
        self.assertNotIn("https://", html)
        css = (ROOT / "academy.css").read_text(encoding="utf-8")
        self.assertNotIn("@import", css)
        self.assertNotIn("url(", css)


if __name__ == "__main__":
    unittest.main()
