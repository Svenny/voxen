"""
Generates contents of `version.hpp` build-config file
"""

import pathlib
import subprocess
import sys

if len(sys.argv) < 5:
	print('Usage: python3 gen-version-header.py <output-file> <major> <minor> <patch> [suffix]')
	sys.exit(1)

try:
	git_dir = pathlib.Path(sys.argv[0]).parent / '../../.git'
	git_hash = subprocess.check_output([
		'git', '--git-dir', git_dir, 'rev-parse', 'HEAD'
	], stderr=subprocess.DEVNULL).decode('ascii')[:16]
except Exception:
	# This can fail on source copies without git history or no git in system
	git_hash = 'unknown'

suffix = sys.argv[5] if len(sys.argv) == 6 else ''

data = """// GENERATED FILE, DO NOT EDIT
#pragma once

namespace voxen::Version
{{

// Voxen version, follows SemVer 2.0.0
constexpr inline int MAJOR = {0};
constexpr inline int MINOR = {1};
constexpr inline int PATCH = {2};

// Optional prerelease suffix
constexpr inline char SUFFIX[] = "{3}";
// Partial hash of Git commit, can be `unknown`
constexpr inline char GIT_HASH[] = "{4}";

// All components of version combined, usable for logging/display
constexpr inline char STRING[] = "{0}.{1}.{2}{5}{3} (git-{4})";

}}
""".format(*sys.argv[2:5], suffix, git_hash, '-' if suffix else '')

out_file = pathlib.Path(sys.argv[1])

# Don't touch output file if not needed to avoid excessive rebuilds
if out_file.is_file():
	with open(out_file, 'r') as f:
		if f.read() == data:
			sys.exit(0)

out_file.parent.mkdir(parents=True, exist_ok=True)

with open(out_file, 'w') as f:
	f.write(data)
