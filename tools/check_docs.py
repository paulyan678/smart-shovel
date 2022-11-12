#!/usr/bin/env python3
"""Validate local documentation links, XML, privacy, and asset weight."""

from __future__ import annotations

import argparse
import html
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from urllib.parse import unquote, urlsplit


SOURCE_SUFFIXES = {".css", ".htm", ".html", ".js", ".markdown", ".md", ".svg", ".xml"}
XML_SUFFIXES = {".svg", ".xml"}
ASSET_SUFFIXES = {
    ".avif",
    ".gif",
    ".ico",
    ".jpeg",
    ".jpg",
    ".mp4",
    ".pdf",
    ".png",
    ".svg",
    ".webm",
    ".webp",
    ".woff",
    ".woff2",
}

# Documentation should remain practical to clone and quick to render on a phone.
MAX_SINGLE_ASSET_BYTES = 2 * 1024 * 1024
MAX_TOTAL_ASSET_BYTES = 8 * 1024 * 1024

ABSOLUTE_LOCAL_PATH = re.compile(
    r"(?:/Users/|file\s*://|[A-Za-z]:[\\/](?:Users|Documents)[\\/])",
    re.IGNORECASE,
)
HTML_REFERENCE = re.compile(
    r"\b(?:href|src)\s*=\s*(?:\"([^\"]*)\"|'([^']*)'|([^\s\"'=<>`]+))",
    re.IGNORECASE,
)
CSS_REFERENCE = re.compile(
    r"url\(\s*(?:\"([^\"]*)\"|'([^']*)'|([^)]*?))\s*\)",
    re.IGNORECASE,
)
MARKDOWN_DEFINITION = re.compile(
    r"^[ \t]*\[[^\]\n]+\]:[ \t]*(?:<([^>]+)>|([^\s]+))",
    re.MULTILINE,
)
FENCED_MARKDOWN = re.compile(
    r"^(?P<fence>`{3,}|~{3,})[^\n]*\n.*?^(?P=fence)[ \t]*$",
    re.MULTILINE | re.DOTALL,
)


@dataclass(frozen=True)
class Reference:
    target: str
    offset: int


@dataclass(frozen=True)
class Issue:
    path: str
    message: str
    line: int | None = None

    def render(self) -> str:
        location = self.path if self.line is None else f"{self.path}:{self.line}"
        return f"{location}: {self.message}"


@dataclass
class CheckResult:
    issues: list[Issue] = field(default_factory=list)
    source_count: int = 0
    reference_count: int = 0
    local_reference_count: int = 0
    asset_count: int = 0
    asset_bytes: int = 0
    xml_count: int = 0

    @property
    def ok(self) -> bool:
        return not self.issues

    def summary(self) -> str:
        return (
            f"sources={self.source_count}, references={self.reference_count}, "
            f"local={self.local_reference_count}, assets={self.asset_count}, "
            f"asset_bytes={self.asset_bytes}, xml={self.xml_count}"
        )


def _relative(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def _line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _mask_fenced_markdown(text: str) -> str:
    """Replace fenced blocks with spaces while preserving offsets and newlines."""

    def mask(match: re.Match[str]) -> str:
        return "".join("\n" if character == "\n" else " " for character in match.group(0))

    return FENCED_MARKDOWN.sub(mask, text)


def _markdown_inline_references(text: str) -> list[Reference]:
    """Extract inline Markdown destinations, including balanced parentheses."""

    references: list[Reference] = []
    cursor = 0
    while True:
        marker = text.find("](", cursor)
        if marker < 0:
            break
        cursor = marker + 2
        if marker > 0 and text[marker - 1] == "\\":
            continue

        position = cursor
        while position < len(text) and text[position].isspace():
            position += 1
        if position >= len(text):
            continue

        start = position
        if text[position] == "<":
            start += 1
            end = start
            while end < len(text) and text[end] != ">":
                end += 1
            if end < len(text):
                references.append(Reference(text[start:end], start))
            continue

        nested = 0
        escaped = False
        end = start
        while end < len(text):
            character = text[end]
            if escaped:
                escaped = False
                end += 1
                continue
            if character == "\\":
                escaped = True
                end += 1
                continue
            if character == "(":
                nested += 1
            elif character == ")":
                if nested == 0:
                    break
                nested -= 1
            elif character.isspace() and nested == 0:
                break
            end += 1
        if end > start:
            references.append(Reference(text[start:end], start))

    return references


def extract_references(path: Path, text: str) -> list[Reference]:
    is_markdown = path.suffix.lower() in {".md", ".markdown"}
    scan_text = _mask_fenced_markdown(text) if is_markdown else text
    references: list[Reference] = []

    if is_markdown:
        references.extend(_markdown_inline_references(scan_text))
        for match in MARKDOWN_DEFINITION.finditer(scan_text):
            target = next(group for group in match.groups() if group is not None)
            references.append(Reference(target, match.start()))

    for pattern in (HTML_REFERENCE, CSS_REFERENCE):
        for match in pattern.finditer(scan_text):
            target = next(group for group in match.groups() if group is not None)
            references.append(Reference(target, match.start()))

    # A target can be found by two syntaxes only in contrived input; keep the
    # diagnostics deterministic and avoid double-counting it.
    unique = {(reference.offset, reference.target): reference for reference in references}
    return [unique[key] for key in sorted(unique)]


def _source_files(root: Path) -> list[Path]:
    paths: list[Path] = []
    readme = root / "README.md"
    if readme.is_file():
        paths.append(readme)
    docs = root / "docs"
    if docs.is_dir():
        paths.extend(
            path
            for path in docs.rglob("*")
            if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
        )
    return sorted(set(paths))


def _tracked_zip_files(root: Path) -> list[Path]:
    if (root / ".git").exists():
        completed = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            check=False,
            capture_output=True,
        )
        if completed.returncode == 0:
            return sorted(
                root / os.fsdecode(entry)
                for entry in completed.stdout.split(b"\0")
                if entry and Path(os.fsdecode(entry)).suffix.lower() == ".zip"
            )

    # Non-Git fixture or exported source tree: conservatively reject ZIPs in it.
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() == ".zip" and ".git" not in path.parts
    )


def _embedded_raster_metadata(path: Path) -> str | None:
    """Return a sensitive metadata kind for supported raster containers."""

    try:
        data = path.read_bytes()
    except OSError:
        return None

    suffix = path.suffix.lower()
    if suffix == ".webp" and len(data) >= 12 and data[:4] == b"RIFF" and data[8:12] == b"WEBP":
        offset = 12
        while offset + 8 <= len(data):
            chunk_type = data[offset : offset + 4]
            chunk_size = int.from_bytes(data[offset + 4 : offset + 8], "little")
            if chunk_type in {b"EXIF", b"XMP "}:
                return chunk_type.decode("ascii").strip()
            offset += 8 + chunk_size + (chunk_size % 2)
    elif suffix in {".jpeg", ".jpg"}:
        if b"Exif\x00\x00" in data:
            return "EXIF"
        if b"http://ns.adobe.com/xap/1.0/" in data or b"<x:xmpmeta" in data:
            return "XMP"
    elif suffix == ".png" and data.startswith(b"\x89PNG\r\n\x1a\n"):
        offset = 8
        while offset + 12 <= len(data):
            chunk_size = int.from_bytes(data[offset : offset + 4], "big")
            chunk_type = data[offset + 4 : offset + 8]
            chunk_data = data[offset + 8 : offset + 8 + chunk_size]
            if chunk_type == b"eXIf":
                return "EXIF"
            if chunk_type in {b"iTXt", b"tEXt", b"zTXt"} and (
                b"XML:com.adobe.xmp" in chunk_data or b"<x:xmpmeta" in chunk_data
            ):
                return "XMP"
            offset += 12 + chunk_size
    return None


def _validate_reference(
    root: Path,
    source: Path,
    reference: Reference,
    text: str,
    result: CheckResult,
) -> None:
    raw_target = html.unescape(reference.target.strip())
    decoded_target = unquote(raw_target)
    line = _line_number(text, reference.offset)
    source_name = _relative(source, root)

    if not raw_target or raw_target.startswith("#"):
        return
    if ABSOLUTE_LOCAL_PATH.search(decoded_target):
        result.issues.append(Issue(source_name, "absolute local path is not allowed", line))
        return
    if raw_target.startswith("//"):
        return

    parsed = urlsplit(raw_target)
    if parsed.scheme:
        # file:// was rejected above; all other schemes are external or embedded.
        return
    if not parsed.path:
        return

    decoded_path = unquote(parsed.path)
    if decoded_path.startswith("/"):
        candidate = root / decoded_path.lstrip("/")
    else:
        candidate = source.parent / decoded_path

    root_resolved = root.resolve()
    candidate_resolved = candidate.resolve()
    try:
        candidate_resolved.relative_to(root_resolved)
    except ValueError:
        result.issues.append(Issue(source_name, f"local target escapes repository: {raw_target}", line))
        return

    result.local_reference_count += 1
    if not candidate_resolved.exists():
        result.issues.append(Issue(source_name, f"broken local target: {raw_target}", line))


def check_repository(root: Path) -> CheckResult:
    root = root.resolve()
    result = CheckResult()

    if not root.is_dir():
        result.issues.append(Issue(str(root), "repository root is not a directory"))
        return result
    if not (root / "README.md").is_file():
        result.issues.append(Issue("README.md", "required documentation entry point is missing"))
    if not (root / "docs").is_dir():
        result.issues.append(Issue("docs", "documentation directory is missing"))

    sources = _source_files(root)
    result.source_count = len(sources)
    for source in sources:
        source_name = _relative(source, root)
        try:
            text = source.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            result.issues.append(Issue(source_name, f"cannot read as UTF-8: {error}"))
            continue

        decoded_text = unquote(text)
        for match in ABSOLUTE_LOCAL_PATH.finditer(decoded_text):
            result.issues.append(
                Issue(
                    source_name,
                    "absolute local path is not allowed",
                    _line_number(decoded_text, match.start()),
                )
            )

        references = extract_references(source, text)
        result.reference_count += len(references)
        for reference in references:
            _validate_reference(root, source, reference, text, result)

        if source.suffix.lower() in XML_SUFFIXES:
            result.xml_count += 1
            try:
                document = ET.parse(source)
                if source.suffix.lower() == ".svg" and document.getroot().tag.rsplit("}", 1)[-1] != "svg":
                    result.issues.append(Issue(source_name, "SVG root element must be <svg>"))
            except (ET.ParseError, OSError) as error:
                result.issues.append(Issue(source_name, f"malformed SVG/XML: {error}"))

    docs = root / "docs"
    if docs.is_dir():
        assets = sorted(
            path
            for path in docs.rglob("*")
            if path.is_file() and path.suffix.lower() in ASSET_SUFFIXES
        )
        result.asset_count = len(assets)
        for asset in assets:
            size = asset.stat().st_size
            result.asset_bytes += size
            if size > MAX_SINGLE_ASSET_BYTES:
                result.issues.append(
                    Issue(
                        _relative(asset, root),
                        f"asset is {size} bytes; limit is {MAX_SINGLE_ASSET_BYTES}",
                    )
                )
            metadata_kind = _embedded_raster_metadata(asset)
            if metadata_kind is not None:
                result.issues.append(
                    Issue(
                        _relative(asset, root),
                        f"embedded {metadata_kind} raster metadata is not allowed",
                    )
                )
        if result.asset_bytes > MAX_TOTAL_ASSET_BYTES:
            result.issues.append(
                Issue(
                    "docs",
                    f"asset budget is {result.asset_bytes} bytes; limit is {MAX_TOTAL_ASSET_BYTES}",
                )
            )

    for archive in _tracked_zip_files(root):
        result.issues.append(Issue(_relative(archive, root), "tracked ZIP archives are not allowed"))

    # Raw-text and parsed-reference checks can identify the same absolute path.
    result.issues = sorted(
        set(result.issues),
        key=lambda issue: (issue.path, issue.line or 0, issue.message),
    )
    return result


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "repository_root",
        nargs="?",
        type=Path,
        default=Path.cwd(),
        help="repository root to check (default: current directory)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_argument_parser().parse_args(argv)
    result = check_repository(args.repository_root)
    if result.ok:
        print(f"Documentation check passed: {result.summary()}")
        return 0

    for issue in result.issues:
        print(f"ERROR: {issue.render()}", file=sys.stderr)
    print(
        f"Documentation check failed with {len(result.issues)} issue(s): {result.summary()}",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
