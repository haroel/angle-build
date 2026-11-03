# ANGLE Apple and Windows builds

This repository builds a small, Metal-only ANGLE static library for iOS and
macOS, and D3D11-only EGL/GLES DLLs for Windows x64. The pinned default is the
ANGLE M143 branch
`chromium/7499` (December 2025).

The GitHub Actions workflow produces:

- `ANGLE.xcframework.zip`: arm64 iPhone/iPad device, arm64 simulator, and
  arm64 Apple Silicon macOS
- `ANGLE-windows-x64.zip`: x64 Release EGL/GLES DLLs, import libraries,
  headers, D3D compiler and a D3D11 WARP smoke test

The unified XCFramework contains one complete `libANGLE.a` per platform slice
with EGL and GLES entry points. Build arguments are kept in the
`args.*.gn` files at the repository root. Pushes to `main` and tags matching `v*` create a
draft release, build Apple slices on macOS runners and Windows DLLs on
`windows-2022` in parallel, merge the Apple slices into one XCFramework,
upload both ZIPs, and publish only after both packages pass their checks. The
workflow can also be run manually with an ANGLE ref and optional release tag.
Branch pushes use the default ANGLE ref and publish a release named
`angle-m143-build-<run id>` after all builds and checks succeed.

See `PACKAGE_README.md` for Xcode linker requirements.
See [Windows integration notes](PACKAGE_README.windows.md) for the DLL layout,
linker requirements and validation limits. Windows uses VS2022, the Windows
SDK and Chromium's downloaded clang-cl; `DEPOT_TOOLS_WIN_TOOLCHAIN=0` selects
the runner's installed Microsoft toolchain. The non-component Release build
uses `/MT` internally. Its GN arguments are in [args.windows.x64.gn](args.windows.x64.gn).
The Windows job initializes depot_tools' CIPD/Python environment before
disabling automatic updates, then invokes that checkout's tools by explicit path.

To build, open **Actions → Build and publish ANGLE libraries → Run workflow**,
keep the default `angle_ref` or select a compatible upstream ref, and optionally
set `release_tag`. Pushing to `main` or a `v*` tag starts the same complete build. Download
`ANGLE-windows-x64.zip` from the resulting Release or the `angle-windows-release`
workflow artifact. No personal access token is needed; publishing uses the
workflow's existing `GITHUB_TOKEN` permissions.

The Windows job compiles and executes [WindowsEGLSmoke.cpp](tests/WindowsEGLSmoke.cpp)
against the staged package. It checks an ES3 pbuffer shader draw/readback and
a hidden HWND surface/swap using D3D11 WARP. WARP is a software device and does
not replace hardware GPU or ARDA integration testing.

## Source patches

Both Apple platform jobs run `scripts/apply-patches.sh` before GN generation. It
applies every patch in `patches/`, skips patches already present upstream,
and fails if a patch cannot be applied or confirmed as already present.

- `angle-complete-static-library.patch` folds EGL, GLES and the Metal backend
  into a self-contained archive.
- `angle-metal-thread-address-space.patch` backports
  [WebKit's Metal address-space fix](https://commits.webkit.org/317176@main).
  The `ANGLE_Out`, `ANGLE_InOut`, `ANGLE_VectorElemRef` and `ANGLE_SwizzleRef`
  conversion operators need a trailing `thread` qualifier. Without it,
  applications targeting iOS/macOS 27 can fail shader linking with
  `reference to type 'thread float3' could not bind to an lvalue of type 'float3'`,
  followed by repeated `GL_INVALID_OPERATION (0x502)` errors.

To apply the same patches to a local ANGLE source checkout:

```sh
bash scripts/apply-patches.sh /path/to/angle
```

Apple release packages include the patch files and SHA-256 checksums under
`build-config/`, plus the packaging repository commit in each platform's
`BUILD_INFO` file.

Windows builds use the unpatched upstream ref and record that explicitly in
`build-config/BUILD_INFO.windows.txt`; the DLL package has its own `SHA256SUMS`.

## Metal shader regression check

`tests/MetalShaderSmoke.cpp` links shaders using all four helper types,
draws into a headless EGL pbuffer and checks the resulting pixels. Build the
test executable with deployment target **27.0** to exercise the failing
compiler behavior; older deployment targets may hide the issue.

For a package unpacked at `/path/to/ANGLE`, run on macOS 27 with Xcode 27:

```sh
angle_slice=/path/to/ANGLE/ANGLE.xcframework/macos-arm64
xcrun --sdk macosx clang++ -std=c++17 -target arm64-apple-macos27.0 \
  -I "$angle_slice/Headers" tests/MetalShaderSmoke.cpp "$angle_slice/libANGLE.a" \
  -Wl,-ObjC -framework Metal -framework QuartzCore -framework IOSurface \
  -framework Foundation -framework CoreFoundation -framework CoreGraphics -framework IOKit \
  -o /tmp/angle-metal-smoke
/tmp/angle-metal-smoke
```

For an iOS 27 simulator, use the `ios-arm64-simulator` slice,
`--sdk iphonesimulator -target arm64-apple-ios27.0-simulator`, omit `IOKit`,
and run the executable with `xcrun simctl spawn <device-uuid> /tmp/angle-metal-smoke`.
Both cases must print `PASS` with `GL error=0x0`. The workflow's macOS 15
runners retain the existing link checks; this OS 27 GPU check runs separately
on a compatible system.
