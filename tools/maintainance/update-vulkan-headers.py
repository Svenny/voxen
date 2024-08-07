#!/usr/bin/env python3

import pathlib
import subprocess
import sys

headers_dir = (pathlib.Path(sys.argv[0]).parent / '../../3rdparty/vulkan-headers/vulkan').resolve()
print('Updating Vulkan headers in', headers_dir)

def get_vulkan_header(header: str):
	url = f'https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/main/include/vulkan/{header}'
	path = headers_dir / header

	print(f'Downloading {url} to {path}')
	subprocess.run(['curl', url, '-o', path], check=True)

get_vulkan_header('vk_platform.h')

vk_platform_path = headers_dir / 'vk_platform.h'
print(f'Patching {vk_platform_path}')
subprocess.run(['patch', '--no-backup-if-mismatch', vk_platform_path,
	headers_dir / 'vk_platform_patch.diff'], check=True)

for header in [ 'vulkan.h', 'vulkan_core.h', 'vulkan_wayland.h', 'vulkan_win32.h',
	'vulkan_xcb.h', 'vulkan_xlib_xrandr.h', 'vulkan_xlib.h' ]:
	get_vulkan_header(header)

	path = headers_dir / header
	print(f'Patching in VKAPI_NOEXCEPT in {path}')
	subprocess.run(['sed', '-i', 's/);/) VKAPI_NOEXCEPT;/', path], check=True)

video_dir = (headers_dir / '../vk_video').resolve()

for header in [ 'vulkan_video_codecs_common.h', 'vulkan_video_codec_h264std.h', 'vulkan_video_codec_h265std.h', 'vulkan_video_codec_av1std.h',
   'vulkan_video_codec_h264std_decode.h', 'vulkan_video_codec_h264std_encode.h', 'vulkan_video_codec_h265std_decode.h', 'vulkan_video_codec_h265std_encode.h',
   'vulkan_video_codec_av1std_decode.h' ]:
   url = f'https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/main/include/vk_video/{header}'
   path = video_dir / header

   print(f'Downloading {url} to {path}')
   subprocess.run(['curl', url, '-o', path], check=True)

print('Done')
