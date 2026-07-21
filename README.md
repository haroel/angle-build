# ANGLE static XCFramework builds

This repository builds a small, Metal-only ANGLE static library for iOS and
macOS. The pinned default is the maintained ANGLE M143 branch
`chromium/7499` (December 2025).

The GitHub Actions workflow produces:

- `ANGLE-iOS.xcframework.zip`: arm64 iPhone/iPad device and arm64 simulator
- `ANGLE-macOS.xcframework.zip`: arm64 Apple Silicon macOS

Each XCFramework contains one complete `libANGLE.a` with EGL and GLES entry
points. Build arguments are kept in the three `args.*.gn` files at the
repository root. Tag pushes matching `v*` create a draft release, build both
packages on GitHub's macOS runner, upload the ZIP files, and publish the
release. The workflow can also be run manually with an ANGLE ref and optional
release tag.

See `PACKAGE_README.md` for Xcode linker requirements.
