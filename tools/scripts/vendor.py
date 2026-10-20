#!/usr/bin/env python3
"""
Vendor third-party code into the tree from two manifests:

- `3rdparty/vendor.toml`: sources built in-graph, one directory `3rdparty/<name>` per table
- `toolchains/vendor.toml`: prebuilt host tools tracked with Git LFS,
  one directory `toolchains/<name>/<platform>` per platform of a table

Every directory is derived from an upstream archive as a pure function of the manifest
(and of `3rdparty/patches/<name>/` for sources). A source table looks like this:

    [name]
    version = "1.2.3"           # substituted for `{version}` in `url`
    url     = "https://..."     # .tar.gz, .tar.xz or .zip
    sha256  = "..."             # of the archive; leave empty to get it printed
    license = "MIT"             # informational
    keep    = ["include/**", "src/*.c", "LICENSE"]  # globs, relative to the archive root
    exclude = ["src/*test*.c"]  # optional, removed from `keep` matches
    strip   = 1                 # optional, leading path components to strip, default 1
    patches = ["config.patch"]  # optional, `git apply`-ed in order after `replace`

    [[name.replace]]            # optional regex substitutions (Python `re` syntax)
    files = ["include/*.h"]
    regex = '\\);'
    with  = ') NOEXCEPT;'

A prebuilt table has per-platform archives instead (no `patches` and `replace`):

    [name]
    version = "1.2.3"
    license = "MIT"

    [name.platforms.linux-x86_64]
    url     = "https://..."
    sha256  = "..."
    keep    = ["bin/tool", "lib/libtool.so.*", "LICENSE"]
    exclude = [...]             # optional
    strip   = 0                 # optional, default 1

The leading `strip` path components must be the same for all archive members.
Every `keep`, `exclude`, `files` pattern and every `replace` must match something,
and every patch must apply cleanly, otherwise the entry is rejected. This catches
renames when bumping versions. Archive members that are not regular files (symlinks,
directories) are never kept. Files are compared and written byte for byte; prebuilt
files also keep the executable bit of the archive member (compared on POSIX hosts).
`check` tells LFS pointers (`git lfs pull` was not run) apart from real mismatches.

Commands:
    vendor.py update [name...]  re-derive the directories and replace them in the tree
    vendor.py check [name...]   derive into a temporary directory and diff against the tree

Needs only the Python standard library (3.14+) and `git` (for `git apply`).
"""

import argparse
import difflib
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import tomllib
import urllib.request
import zipfile
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
THIRD_PARTY = ROOT / '3rdparty'
TOOLCHAINS = ROOT / 'toolchains'
PATCHES = THIRD_PARTY / 'patches'
DEFAULT_CACHE = ROOT / 'build' / 'vendor-cache'

ARCHIVE_KEYS = {'url', 'sha256', 'keep', 'exclude', 'strip'}
SOURCE_KEYS = ARCHIVE_KEYS | {'version', 'license', 'patches', 'replace'}
SOURCE_REQUIRED_KEYS = {'version', 'url', 'sha256', 'license', 'keep'}
PREBUILT_KEYS = {'version', 'license', 'platforms'}
PLATFORM_REQUIRED_KEYS = {'url', 'sha256', 'keep'}
REPLACE_KEYS = {'files', 'regex', 'with'}

LFS_POINTER_PREFIX = b'version https://git-lfs.github.com/spec/v1'


class VendorError(Exception):
	pass


@dataclass
class Unit:
	"""One vendored directory derived from one archive"""
	name: str  # manifest table name
	label: str  # `name` or `name/platform`
	version: str
	url: str
	sha256: str
	keep: list[str]
	target: Path
	exclude: list[str] = field(default_factory=list)
	strip: int = 1
	patches: list[str] = field(default_factory=list)
	replace: list[dict] = field(default_factory=list)
	prebuilt: bool = False


def check_keys(what: str, table: dict, allowed: set[str], required: set[str]) -> None:
	unknown = set(table) - allowed
	missing = required - set(table)
	if unknown:
		raise VendorError(f'{what}: unknown keys {sorted(unknown)}')
	if missing:
		raise VendorError(f'{what}: missing keys {sorted(missing)}')
	if 'keep' in table and not table['keep']:
		raise VendorError(f'{what}: `keep` is empty')
	if not isinstance(table.get('strip', 0), int) or table.get('strip', 0) < 0:
		raise VendorError(f'{what}: `strip` must be a non-negative integer')


def source_units(manifest: dict) -> list[Unit]:
	units = []
	for name, entry in manifest.items():
		if not isinstance(entry, dict):
			raise VendorError(f'{name}: not a table')
		check_keys(name, entry, SOURCE_KEYS, SOURCE_REQUIRED_KEYS)
		for i, rep in enumerate(entry.get('replace', [])):
			if set(rep) != REPLACE_KEYS:
				raise VendorError(f'{name}: replace[{i}] must have exactly the keys {sorted(REPLACE_KEYS)}')
			if not rep['files']:
				raise VendorError(f'{name}: replace[{i}] has no `files`')

		units.append(Unit(name=name, label=name, version=entry['version'],
			url=entry['url'].format(version=entry['version']), sha256=entry['sha256'], keep=entry['keep'],
			target=THIRD_PARTY / name, exclude=entry.get('exclude', []), strip=entry.get('strip', 1),
			patches=entry.get('patches', []), replace=entry.get('replace', [])))
	return units


def prebuilt_units(manifest: dict) -> list[Unit]:
	units = []
	for name, entry in manifest.items():
		if not isinstance(entry, dict):
			raise VendorError(f'{name}: not a table')
		check_keys(name, entry, PREBUILT_KEYS, PREBUILT_KEYS)
		if not entry['platforms']:
			raise VendorError(f'{name}: `platforms` is empty')

		for platform, archive in entry['platforms'].items():
			label = f'{name}/{platform}'
			check_keys(label, archive, ARCHIVE_KEYS, PLATFORM_REQUIRED_KEYS)
			units.append(Unit(name=name, label=label, version=entry['version'],
				url=archive['url'].format(version=entry['version']), sha256=archive['sha256'], keep=archive['keep'],
				target=TOOLCHAINS / name / platform, exclude=archive.get('exclude', []),
				strip=archive.get('strip', 1), prebuilt=True))
	return units


def load_units() -> list[Unit]:
	units = []
	for manifest_path, parse in ((THIRD_PARTY / 'vendor.toml', source_units),
			(TOOLCHAINS / 'vendor.toml', prebuilt_units)):
		with open(manifest_path, 'rb') as f:
			units += parse(tomllib.load(f))

	names = [u.name for u in units if not u.prebuilt]
	duplicates = sorted(set(names) & {u.name for u in units if u.prebuilt})
	if duplicates:
		raise VendorError(f'names used in both manifests: {", ".join(duplicates)}')
	return units


def select(units: list[Unit], names: list[str]) -> list[Unit]:
	if not names:
		return units
	unknown = [n for n in names if not any(u.name == n for u in units)]
	if unknown:
		raise VendorError(f'not in the manifests: {", ".join(unknown)}')
	return [u for u in units if u.name in names]


def download(unit: Unit, cache: Path) -> Path:
	path = cache / f'{unit.name}-{PurePosixPath(unit.url).name}'

	if not path.exists():
		print(f'  downloading {unit.url}')
		cache.mkdir(parents=True, exist_ok=True)
		request = urllib.request.Request(unit.url, headers={'User-Agent': 'voxen-vendor.py'})
		with urllib.request.urlopen(request) as response, tempfile.NamedTemporaryFile(dir=cache, delete=False) as tmp:
			shutil.copyfileobj(response, tmp)
		Path(tmp.name).replace(path)

	digest = hashlib.sha256(path.read_bytes()).hexdigest()
	if digest != unit.sha256:
		path.unlink()
		if not unit.sha256:
			raise VendorError(f'{unit.label}: sha256 is not set, downloaded archive has {digest}')
		raise VendorError(f'{unit.label}: sha256 mismatch, expected {unit.sha256}, downloaded archive has {digest}')
	return path


def read_archive(path: Path, strip: int) -> dict[str, tuple[bytes, bool]]:
	"""Regular files of the archive as (content, executable), keyed by path with `strip` components removed"""
	files = {}
	if path.name.endswith('.zip'):
		with zipfile.ZipFile(path) as zf:
			for info in zf.infolist():
				if not info.is_dir():
					files[info.filename] = (zf.read(info), bool((info.external_attr >> 16) & 0o111))
	else:
		with tarfile.open(path) as tf:
			for member in tf:
				if member.isfile():
					files[member.name] = (tf.extractfile(member).read(), bool(member.mode & 0o111))

	for p in files:
		pp = PurePosixPath(p)
		if pp.is_absolute() or '..' in pp.parts:
			raise VendorError(f'{path.name}: unsafe member path {p}')

	prefixes = {PurePosixPath(p).parts[:strip] for p in files}
	if len(prefixes) != 1 or any(len(PurePosixPath(p).parts) <= strip for p in files):
		raise VendorError(f'{path.name}: expected one common {strip}-component prefix, got {sorted(prefixes)}')
	return {str(PurePosixPath(*PurePosixPath(p).parts[strip:])): v for p, v in files.items()}


def match_patterns(paths: list[str], patterns: list[str], what: str) -> set[str]:
	matched = set()
	for pattern in patterns:
		hits = {p for p in paths if PurePosixPath(p).full_match(pattern)}
		if not hits:
			raise VendorError(f'{what} pattern "{pattern}" matches nothing')
		matched |= hits
	return matched


def derive(unit: Unit, archive: Path, out: Path) -> None:
	"""Writes the vendored content of `unit` into the empty directory `out`"""
	files = read_archive(archive, unit.strip)
	contents = {p: data for p, (data, _) in files.items()}
	paths = sorted(files)
	kept = match_patterns(paths, unit.keep, f'{unit.label}: keep')
	kept -= match_patterns(sorted(kept), unit.exclude, f'{unit.label}: exclude')

	for i, rep in enumerate(unit.replace):
		regex = re.compile(rep['regex'], re.MULTILINE)
		total = 0
		for p in match_patterns(sorted(kept), rep['files'], f'{unit.label}: replace[{i}] files'):
			text, count = regex.subn(rep['with'], contents[p].decode())
			contents[p] = text.encode()
			total += count
		if total == 0:
			raise VendorError(f'{unit.label}: replace[{i}] regex "{rep["regex"]}" matches nothing')

	for p in sorted(kept):
		dst = out / p
		dst.parent.mkdir(parents=True, exist_ok=True)
		dst.write_bytes(contents[p])
		if unit.prebuilt:
			dst.chmod(0o755 if files[p][1] else 0o644)

	for patch in unit.patches:
		patch_path = PATCHES / unit.name / patch
		if not patch_path.is_file():
			raise VendorError(f'{unit.label}: patch {patch_path} does not exist')
		result = subprocess.run(['git', '-C', str(out), 'apply', '--check', str(patch_path)],
			capture_output=True, text=True, check=False)
		if result.returncode != 0:
			raise VendorError(f'{unit.label}: patch {patch} does not apply:\n{result.stderr.strip()}')
		subprocess.run(['git', '-C', str(out), 'apply', str(patch_path)], check=True)

	count = sum(1 for _ in out.rglob('*') if _.is_file())
	size = sum(f.stat().st_size for f in out.rglob('*') if f.is_file())
	print(f'  {count} files, {size / 1024 / 1024:.1f} MiB')


def list_files(directory: Path) -> set[str]:
	return {f.relative_to(directory).as_posix() for f in directory.rglob('*') if f.is_file()}


def is_executable(path: Path) -> bool:
	return bool(path.stat().st_mode & 0o111)


def compare(unit: Unit, derived: Path, show_diff: bool) -> bool:
	target = unit.target
	if not target.is_dir():
		print(f'  MISMATCH: {target.relative_to(ROOT)} does not exist')
		return False

	expected = list_files(derived)
	actual = list_files(target)
	problems = []
	for p in sorted(expected - actual):
		problems.append(f'missing: {p}')
	for p in sorted(actual - expected):
		problems.append(f'extra:   {p}')
	for p in sorted(expected & actual):
		a = (derived / p).read_bytes()
		b = (target / p).read_bytes()
		if a != b:
			if b.startswith(LFS_POINTER_PREFIX):
				problems.append(f'not fetched from LFS: {p}')
				continue
			problems.append(f'differs: {p}')
			if show_diff:
				try:
					diff = difflib.unified_diff(a.decode().splitlines(keepends=True),
						b.decode().splitlines(keepends=True), f'derived/{p}', f'{target.relative_to(ROOT)}/{p}')
					problems.append(''.join(diff))
				except UnicodeDecodeError:
					problems.append('(binary)')
		elif unit.prebuilt and os.name == 'posix' and is_executable(derived / p) != is_executable(target / p):
			problems.append(f'exec bit: {p}')

	if problems:
		print('  MISMATCH:')
		for problem in problems:
			print('    ' + problem.replace('\n', '\n    '))
		return False
	print('  ok')
	return True


def main() -> int:
	parser = argparse.ArgumentParser(description='Vendor third-party code from 3rdparty/ and toolchains/ manifests')
	parser.add_argument('command', choices=['update', 'check'])
	parser.add_argument('names', nargs='*', help='manifest tables to process, default all')
	parser.add_argument('--cache', type=Path, default=DEFAULT_CACHE, help='archive download cache directory')
	parser.add_argument('--diff', action='store_true', help='check: print unified diffs of differing text files')
	args = parser.parse_args()

	try:
		units = select(load_units(), args.names)
	except VendorError as e:
		print(f'error: {e}', file=sys.stderr)
		return 2

	failed = []
	with tempfile.TemporaryDirectory(prefix='voxen-vendor-') as tmp:
		for i, unit in enumerate(units):
			print(f'{unit.label} {unit.version}')
			derived = Path(tmp) / str(i)
			derived.mkdir()
			try:
				archive = download(unit, args.cache)
				derive(unit, archive, derived)
			except VendorError as e:
				print(f'  error: {e}')
				failed.append(unit.label)
				continue

			if args.command == 'check':
				if not compare(unit, derived, args.diff):
					failed.append(unit.label)
			else:
				if unit.target.exists():
					shutil.rmtree(unit.target)
				shutil.copytree(derived, unit.target)
				print(f'  written to {unit.target.relative_to(ROOT)}')

	if failed:
		print(f'failed: {", ".join(failed)}', file=sys.stderr)
		return 1
	return 0


if __name__ == '__main__':
	sys.exit(main())
