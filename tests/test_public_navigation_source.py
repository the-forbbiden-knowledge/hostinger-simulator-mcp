"""Original header menus, public-only mode and footer shape."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
HTML = (ROOT / "index.html").read_text(encoding="utf-8")
JS = (ROOT / "public-original.js").read_text(encoding="utf-8")


class PublicOriginalNavigationTests(unittest.TestCase):
    def test_actual_original_navigation_not_a_replacement(self):
        self.assertIn('class="navigation-menu_root__HT1ea"', HTML)
        for label in ("Produtos", "Recursos", "Soluções", "Empresas", "Preços"):
            self.assertIn(label, HTML)
        for identity in ('data-geist-menu-button', 'aria-label="Pergunte à IA"',
                         'cmdklaunch_launcher__f9k1l'):
            self.assertIn(identity, HTML)
        self.assertIn("07-desktop-navigation", JS)
        self.assertIn("06-academy-command", JS)
        self.assertIn("10-profile-menu", JS)
        self.assertIn("11-ask-ai", JS)

    def test_original_footer_five_categories_plus_triangle(self):
        start = HTML.index('<footer class=')
        end = HTML.index('</footer>', start)
        footer = HTML[start:end]
        categories = re.findall(r'<h2 class="font-mono uppercase [^"]+">([^<]+)</h2>', footer)
        self.assertEqual(categories, ["Primeiros passos", "Investigar", "Monitorar",
                                      "Plataforma", "Recursos"])
        self.assertIn('aria-label="Chefes logo"', footer)
        self.assertIn('aria-label="sistema"', footer)
        self.assertIn('aria-label="claro"', footer)
        self.assertIn('aria-label="escuro"', footer)

    def test_original_modals_have_no_live_backend(self):
        self.assertIn('data-ask-ai-panel', HTML)
        self.assertIn('connect-src &#39;none&#39;', HTML.replace("'", "&#39;"))
        self.assertNotIn('fetch(', JS)
        self.assertNotIn('XMLHttpRequest', JS)
        self.assertNotIn('http://127.0.0.1', JS)
        self.assertIn('Sem IA conectada', JS)
        self.assertNotIn('data-profile-banner\x3d\x22hidden', HTML)
        self.assertFalse((ROOT / "login.html").exists())
        self.assertFalse((ROOT / "signup.html").exists())


if __name__ == "__main__":
    unittest.main()
