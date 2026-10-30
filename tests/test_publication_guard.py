"""Synthetic privacy-policy regression tests for source snapshot checks."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from publication_guard import check_entry, inspect_entries


class PublicationGuardTests(unittest.TestCase):
    def test_known_public_paths_pass(self):
        for name in ('README.md', 'src/native/simulator.cpp',
                     'tools/publication_guard.py', '.github/workflows/windows-native.yml',
                     '.env.example'):
            self.assertEqual(check_entry(name, b'synthetic public text'), set())

    def test_private_source_file_types_fail(self):
        for suffix in ('.pdf', '.zip', '.docx', '.7z', '.pfx', '.pem', '.sqlite'):
            with self.subTest(suffix=suffix):
                self.assertIn('proprietary_or_sensitive_file_type',
                              check_entry('any-name' + suffix, b'fixture'))

    def test_private_folders_fail(self):
        for prefix in ('private/', 'library/', 'third-party/', 'credentials/'):
            self.assertIn('private_path', check_entry(prefix + 'file.txt', b'fixture'))

    def test_environment_file_not_allowlisted(self):
        self.assertIn('environment_file', check_entry('.env', b'A=1'))
        self.assertIn('environment_file', check_entry('.env.production', b'A=1'))
        self.assertEqual(check_entry('.env.example', b'VALUE=placeholder'), set())

    def test_private_key_header_is_detected_without_echoing_it(self):
        marker = b'-----BEGIN ' + b'PRIVATE KEY-----'
        self.assertIn('private_key', check_entry('config.txt', marker))

    def test_credential_like_provider_tokens_are_denied(self):
        fake = b'gh' + b'p_' + b'Z' * 40
        self.assertIn('provider_token', check_entry('synthetic.txt', fake))

    def test_credential_like_cloud_access_keys_are_denied(self):
        fake = b'AK' + b'IA' + b'X' * 16
        self.assertIn('cloud_key', check_entry('synthetic.txt', fake))

    def test_oversized_files_fail_closed(self):
        self.assertIn('oversized_unreviewed_file',
                      check_entry('ordinary.txt', b'x' * 2_000_001))

    def test_symlinks_require_review(self):
        self.assertIn('symlink_requires_manual_review',
                      check_entry('link.txt', b'', symlink=True))

    def test_reporting_is_redacted_and_deterministic(self):
        entries = [('private/source.txt', b'synthetic', False),
                   ('public.txt', b'plain', False)]
        result = inspect_entries(entries)
        self.assertEqual(result['decision'], 'no_go')
        self.assertEqual(result['checked_files'], 2)
        self.assertEqual(result['violating_files'], 1)
        self.assertFalse(result['secret_values_logged'])
        self.assertFalse(result['file_names_logged'])
        self.assertNotIn('private/source.txt', str(result))
        self.assertEqual(result, inspect_entries(entries))

    def test_clean_snapshot_approved(self):
        report = inspect_entries([('public.py', b'print("hello")', False)])
        self.assertEqual(report['decision'], 'go')

    def test_rejects_traversal_paths(self):
        self.assertIn('invalid_path', check_entry('../other.txt', b'x'))
        self.assertIn('invalid_path', check_entry('/absolute.txt', b'x'))


if __name__ == '__main__':
    unittest.main()
