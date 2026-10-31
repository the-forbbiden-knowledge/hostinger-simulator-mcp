"""Checks for navigation, modal affordances, and public GitHub Pages boundaries."""
import base64
import hashlib
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PublicNavigationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.html = (ROOT / "index.html").read_text(encoding="utf-8")

    def test_three_navigation_panels_and_fixed_empty_slots(self):
        html = self.html
        for name in ("Produtos", "Recursos", "Soluções"):
            self.assertIn('id="menu-' + name + '"', html)
        self.assertEqual(html.count('class="nav-column"'), 9)
        self.assertEqual(html.count('class="nav-item"'), 27)
        self.assertEqual(html.count('class="nav-slot-empty"'), 18)
        self.assertIn('grid-template-rows:repeat(5,62px)', html)
        self.assertIn('aria-expanded="false"', html)
        self.assertIn('class="mobile-menu"', html)

    def test_footer_and_light_theme_selector(self):
        html = self.html
        self.assertEqual(html.count('class="foot-col"'), 5)
        self.assertIn('class="foot-col foot-brand"', html)
        self.assertIn('class="theme-switcher"', html)
        for value in ("system", "light", "dark"):
            self.assertIn('name="site-theme" value="' + value + '"', html)
        self.assertIn('Ambiente de simulação', html)

    def test_modal_triggers_do_not_link_to_private_pages(self):
        html = self.html
        for token in ('id="ai-template"', 'id="docs-template"', 'id="account-template"',
                      'data-account-action="entrar"', 'data-account-action="cadastro"',
                      'aria-modal="true"', 'id="docs-query"', 'id="ai-question"'):
            self.assertIn(token, html)
        for denied in ('href="/login', 'href="/signup', 'fetch(', 'XMLHttpRequest',
                       'api.groq.com', 'config/groq-api.key', 'electron/'):
            self.assertNotIn(denied, html)

    def test_inline_script_and_style_are_csp_hash_restricted(self):
        html = self.html
        policy = re.search(r'<meta http-equiv="Content-Security-Policy" content="([^"]+)"', html).group(1)
        for tag in ("style", "script"):
            contents = re.findall("<" + tag + r">(.*?)</" + tag + ">", html, re.S)
            self.assertEqual(len(contents), 1)
            digest = base64.b64encode(hashlib.sha256(contents[0].encode("utf-8")).digest()).decode()
            self.assertIn("'sha256-" + digest + "'", policy)
        self.assertIn("connect-src 'none'", policy)
        self.assertIn("form-action 'none'", policy)
        self.assertNotIn("unsafe-inline", policy)
        self.assertNotIn("unsafe-eval", policy)


if __name__ == "__main__":
    unittest.main()
