# ANGLE static XCFramework builds

This repository builds a small, Metal-only ANGLE static library for iOS and
macOS. The pinned default is the maintained ANGLE M143 branch
`chromium/7499` (December 2025).

The GitHub Actions workflow produces:

- `ANGLE.xcframework.zip`: arm64 iPhone/iPad device, arm64 simulator, and
  arm64 Apple Silicon macOS

The unified XCFramework contains one complete `libANGLE.a` per platform slice
with EGL and GLES entry points. Build arguments are kept in the three
`args.*.gn` files at the repository root. Tag pushes matching `v*` create a
draft release, build the platform slices in parallel on GitHub's macOS runners,
merge them into one XCFramework, upload the ZIP, and publish the release. The
workflow can also be run manually with an ANGLE ref and optional release tag.

See `PACKAGE_README.md` for Xcode linker requirements.
