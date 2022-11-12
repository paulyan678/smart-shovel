from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIRECTORY = REPOSITORY_ROOT / "tools"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import check_docs  # noqa: E402


class DocumentationCheckTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        (self.root / "docs" / "assets").mkdir(parents=True)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def write_text(self, relative_path: str, content: str) -> Path:
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def create_valid_fixture(self) -> None:
        self.write_text(
            "README.md",
            "[Guide](docs/guide%20one.md)\n"
            "[External](https://example.com/reference)\n"
            "![Embedded](data:image/svg+xml;base64,PHN2Zy8+)\n",
        )
        self.write_text(
            "docs/guide one.md",
            "![Prototype](<assets/photo one.webp>)\n"
            '<img src="assets/photo%20one.webp" alt="Prototype">\n'
            "[Section](#details)\n",
        )
        self.write_text(
            "docs/site.css",
            '.hero { background-image: url("assets/photo%20one.webp"); }\n'
            ".marker { filter: url(#shadow); }\n"
            '[tabindex="0"]:focus-visible { outline: 2px solid; }\n',
        )
        self.write_text("docs/site.js", 'preview.src = "assets/photo%20one.webp";\n')
        self.write_text(
            "docs/assets/diagram.svg",
            '<svg xmlns="http://www.w3.org/2000/svg"><defs><filter id="shadow"/></defs></svg>\n',
        )
        (self.root / "docs" / "assets" / "photo one.webp").write_bytes(b"RIFF fixture")

    def messages(self, result: check_docs.CheckResult) -> str:
        return "\n".join(issue.render() for issue in result.issues)

    def test_accepts_valid_links_spaces_encoding_and_skipped_urls(self) -> None:
        self.create_valid_fixture()

        result = check_docs.check_repository(self.root)

        self.assertTrue(result.ok, self.messages(result))
        self.assertEqual(result.source_count, 5)
        self.assertEqual(result.local_reference_count, 5)
        self.assertEqual(result.xml_count, 1)
        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            exit_code = check_docs.main([str(self.root)])
        self.assertEqual(exit_code, 0)
        self.assertIn("Documentation check passed", stdout.getvalue())

    def test_rejects_broken_local_link(self) -> None:
        self.write_text("README.md", "[Missing](docs/not-there.md)\n")

        result = check_docs.check_repository(self.root)

        self.assertFalse(result.ok)
        self.assertIn("broken local target: docs/not-there.md", self.messages(result))

    def test_rejects_absolute_local_paths_and_file_urls(self) -> None:
        self.write_text(
            "README.md",
            "![Private](/Users/example/private.png)\n"
            '<a href="file:///tmp/local.html">local</a>\n',
        )

        result = check_docs.check_repository(self.root)

        self.assertFalse(result.ok)
        messages = self.messages(result)
        self.assertIn("absolute local path is not allowed", messages)
        self.assertEqual(messages.count("absolute local path is not allowed"), 2)

    def test_rejects_malformed_svg(self) -> None:
        self.write_text("README.md", "![Diagram](docs/assets/broken.svg)\n")
        self.write_text("docs/assets/broken.svg", "<svg><g></svg>\n")

        result = check_docs.check_repository(self.root)

        self.assertFalse(result.ok)
        self.assertIn("malformed SVG/XML", self.messages(result))

    def test_rejects_oversized_asset(self) -> None:
        self.write_text("README.md", "![Large](docs/assets/large.png)\n")
        (self.root / "docs" / "assets" / "large.png").write_bytes(
            b"x" * (check_docs.MAX_SINGLE_ASSET_BYTES + 1)
        )

        result = check_docs.check_repository(self.root)

        self.assertFalse(result.ok)
        self.assertIn("asset is", self.messages(result))
        self.assertIn(f"limit is {check_docs.MAX_SINGLE_ASSET_BYTES}", self.messages(result))

    def test_rejects_embedded_webp_metadata(self) -> None:
        self.write_text("README.md", "![Private](docs/assets/private.webp)\n")
        payload = b"camera and location fields"
        chunk = b"EXIF" + len(payload).to_bytes(4, "little") + payload
        if len(payload) % 2:
            chunk += b"\x00"
        body = b"WEBP" + chunk
        (self.root / "docs" / "assets" / "private.webp").write_bytes(
            b"RIFF" + len(body).to_bytes(4, "little") + body
        )

        result = check_docs.check_repository(self.root)

        self.assertFalse(result.ok)
        self.assertIn("embedded EXIF raster metadata is not allowed", self.messages(result))

    def test_rejects_zip_in_exported_tree(self) -> None:
        self.write_text("README.md", "Documentation\n")
        (self.root / "docs" / "pitch.zip").write_bytes(b"PK fixture")

        result = check_docs.check_repository(self.root)

        self.assertFalse(result.ok)
        self.assertIn("tracked ZIP archives are not allowed", self.messages(result))


if __name__ == "__main__":
    unittest.main()
