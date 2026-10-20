#!/usr/bin/env python3
"""
Vendor third-party sources into `3rdparty/` from the manifest `3rdparty/vendor.toml`.

Each manifest table describes one directory `3rdparty/<name>`, derived from an
upstream archive as a pure function of the manifest and `3rdparty/patches/<name>/`:

    [name]
    version = "1.2.3"           # substituted for `{version}` in `url`
    url     = "https://..."     # .tar.gz, .tar.xz or .zip with one top-level directory
    sha256  = "..."             # of the archive; leave empty to get it printed
    license = "MIT"             # informational
    keep    = ["include/**", "src/*.c", "LICENSE"]  # globs, relative to the archive root
    exclude = ["src/*test*.c"]  # optional, removed from `keep` matches
    patches = ["config.patch"]  # optional, `git apply`-ed in order after `replace`

    [[name.replace]]            # optional regex substitutions (Python `re` syntax)
    files = ["include/*.h"]
    regex = '\\);'
    with  = ') NOEXCEPT;'

Every `keep`, `exclude`, `files` pattern and every `replace` must match something,
and every patch must apply cleanly, otherwise the entry is rejected. This catches
renames when bumping versions. Archive members that are not regular files (symlinks,
directories) are never kept. Files are compared and written byte for byte.

Commands:
    vendor.py update [name...]  re-derive the directories and replace them in the tree
    vendor.py check [name...]   derive into a temporary directory and diff against the tree

Needs only the Python standard library (3.14+) and `git` (for `git apply`).
"""

import argparse
import difflib
import hashlib
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import tomllib
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
THIRD_PARTY = ROOT / '3rdparty'
MANIFEST = THIRD_PARTY / 'vendor.toml'
PATCHES = THIRD_PARTY / 'patches'
DEFAULT_CACHE = ROOT / 'build' / 'vendor-cache'

ENTRY_KEYS = {'version', 'url', 'sha256', 'license', 'keep', 'exclude', 'patches', 'replace'}
REQUIRED_KEYS = {'version', 'url', 'sha256', 'license', 'keep'}
REPLACE_KEYS = {'files', 'regex', 'with'}


class VendorError(Exception):
	pass


def load_manifest() -> dict[str, dict]:
	with open(MANIFEST, 'rb') as f:
		manifest = tomllib.load(f)

	for name, entry in manifest.items():
		if not isinstance(entry, dict):
			raise VendorError(f'{name}: not a table')
		unknown = set(entry) - ENTRY_KEYS
		missing = REQUIRED_KEYS - set(entry)
		if unknown:
			raise VendorError(f'{name}: unknown keys {sorted(unknown)}')
		if missing:
			raise VendorError(f'{name}: missing keys {sorted(missing)}')
		if not entry['keep']:
			raise VendorError(f'{name}: `keep` is empty')
		for i, rep in enumerate(entry.get('replace', [])):
			if set(rep) != REPLACE_KEYS:
				raise VendorError(f'{name}: replace[{i}] must have exactly the keys {sorted(REPLACE_KEYS)}')
			if not rep['files']:
				raise VendorError(f'{name}: replace[{i}] has no `files`')

	return manifest


def select(manifest: dict[str, dict], names: list[str]) -> dict[str, dict]:
	if not names:
		return manifest
	unknown = [n for n in names if n not in manifest]
	if unknown:
		raise VendorError(f'not in the manifest: {", ".join(unknown)}')
	return {n: manifest[n] for n in names}


def download(name: str, entry: dict, cache: Path) -> Path:
	url = entry['url'].format(version=entry['version'])
	path = cache / f'{name}-{PurePosixPath(url).name}'

	if not path.exists():
		print(f'  downloading {url}')
		cache.mkdir(parents=True, exist_ok=True)
		request = urllib.request.Request(url, headers={'User-Agent': 'voxen-vendor.py'})
		with urllib.request.urlopen(request) as response, tempfile.NamedTemporaryFile(dir=cache, delete=False) as tmp:
			shutil.copyfileobj(response, tmp)
		Path(tmp.name).replace(path)

	digest = hashlib.sha256(path.read_bytes()).hexdigest()
	if digest != entry['sha256']:
		path.unlink()
		if not entry['sha256']:
			raise VendorError(f'{name}: sha256 is not set, downloaded archive has {digest}')
		raise VendorError(f'{name}: sha256 mismatch, expected {entry["sha256"]}, downloaded archive has {digest}')
	return path


def read_archive(path: Path) -> dict[str, bytes]:
	"""Regular files of the archive, keyed by path with the top-level directory stripped"""
	files = {}
	if path.name.endswith('.zip'):
		with zipfile.ZipFile(path) as zf:
			for info in zf.infolist():
				if not info.is_dir():
					files[info.filename] = zf.read(info)
	else:
		with tarfile.open(path) as tf:
			for member in tf:
				if member.isfile():
					files[member.name] = tf.extractfile(member).read()

	top_levels = {PurePosixPath(p).parts[0] for p in files}
	if len(top_levels) != 1:
		raise VendorError(f'{path.name}: expected one top-level directory, got {sorted(top_levels)}')
	return {str(PurePosixPath(*PurePosixPath(p).parts[1:])): data for p, data in files.items()}


def match_patterns(paths: list[str], patterns: list[str], what: str) -> set[str]:
	matched = set()
	for pattern in patterns:
		hits = {p for p in paths if PurePosixPath(p).full_match(pattern)}
		if not hits:
			raise VendorError(f'{what} pattern "{pattern}" matches nothing')
		matched |= hits
	return matched


def derive(name: str, entry: dict, archive: Path, out: Path) -> None:
	"""Writes the vendored content of `entry` into the empty directory `out`"""
	files = read_archive(archive)
	paths = sorted(files)
	kept = match_patterns(paths, entry['keep'], f'{name}: keep')
	kept -= match_patterns(sorted(kept), entry.get('exclude', []), f'{name}: exclude')

	for i, rep in enumerate(entry.get('replace', [])):
		regex = re.compile(rep['regex'], re.MULTILINE)
		total = 0
		for p in match_patterns(sorted(kept), rep['files'], f'{name}: replace[{i}] files'):
			text, count = regex.subn(rep['with'], files[p].decode())
			files[p] = text.encode()
			total += count
		if total == 0:
			raise VendorError(f'{name}: replace[{i}] regex "{rep["regex"]}" matches nothing')

	for p in sorted(kept):
		dst = out / p
		dst.parent.mkdir(parents=True, exist_ok=True)
		dst.write_bytes(files[p])

	for patch in entry.get('patches', []):
		patch_path = PATCHES / name / patch
		if not patch_path.is_file():
			raise VendorError(f'{name}: patch {patch_path} does not exist')
		result = subprocess.run(['git', '-C', str(out), 'apply', '--check', str(patch_path)],
			capture_output=True, text=True, check=False)
		if result.returncode != 0:
			raise VendorError(f'{name}: patch {patch} does not apply:\n{result.stderr.strip()}')
		subprocess.run(['git', '-C', str(out), 'apply', str(patch_path)], check=True)

	count = sum(1 for _ in out.rglob('*') if _.is_file())
	size = sum(f.stat().st_size for f in out.rglob('*') if f.is_file())
	print(f'  {count} files, {size / 1024 / 1024:.1f} MiB')


def list_files(directory: Path) -> set[str]:
	return {f.relative_to(directory).as_posix() for f in directory.rglob('*') if f.is_file()}


def compare(name: str, derived: Path, show_diff: bool) -> bool:
	target = THIRD_PARTY / name
	if not target.is_dir():
		print(f'  MISMATCH: {target} does not exist')
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
			problems.append(f'differs: {p}')
			if show_diff:
				try:
					diff = difflib.unified_diff(a.decode().splitlines(keepends=True),
						b.decode().splitlines(keepends=True), f'derived/{p}', f'3rdparty/{name}/{p}')
					problems.append(''.join(diff))
				except UnicodeDecodeError:
					problems.append('(binary)')

	if problems:
		print('  MISMATCH:')
		for problem in problems:
			print('    ' + problem.replace('\n', '\n    '))
		return False
	print('  ok')
	return True


def main() -> int:
	parser = argparse.ArgumentParser(description='Vendor third-party sources from 3rdparty/vendor.toml')
	parser.add_argument('command', choices=['update', 'check'])
	parser.add_argument('names', nargs='*', help='dependencies to process, default all')
	parser.add_argument('--cache', type=Path, default=DEFAULT_CACHE, help='archive download cache directory')
	parser.add_argument('--diff', action='store_true', help='check: print unified diffs of differing text files')
	args = parser.parse_args()

	try:
		entries = select(load_manifest(), args.names)
	except VendorError as e:
		print(f'error: {e}', file=sys.stderr)
		return 2

	failed = []
	with tempfile.TemporaryDirectory(prefix='voxen-vendor-') as tmp:
		for name, entry in entries.items():
			print(f'{name} {entry["version"]}')
			derived = Path(tmp) / name
			derived.mkdir()
			try:
				archive = download(name, entry, args.cache)
				derive(name, entry, archive, derived)
			except VendorError as e:
				print(f'  error: {e}')
				failed.append(name)
				continue

			if args.command == 'check':
				if not compare(name, derived, args.diff):
					failed.append(name)
			else:
				target = THIRD_PARTY / name
				if target.exists():
					shutil.rmtree(target)
				shutil.copytree(derived, target)
				print(f'  written to {target.relative_to(ROOT)}')

	if failed:
		print(f'failed: {", ".join(failed)}', file=sys.stderr)
		return 1
	return 0


if __name__ == '__main__':
	sys.exit(main())
