# ANGLE for Windows x64

Release build of the pinned ANGLE ref, with the D3D11 renderer and HLSL
translator enabled. D3D9, desktop GL, Vulkan, Metal and software Vulkan
backends are disabled. Desktop Win32 HWND integration is supported; this is
not a WinUI-specific build.

## Contents and integration

- `include/`: EGL, GLES2, GLES3 and KHR headers, including ANGLE extensions.
- `lib/libEGL.lib`, `lib/libGLESv2.lib`: x64 import libraries, renamed from
  Chromium's `libEGL.dll.lib` / `libGLESv2.dll.lib` outputs.
- `bin/libEGL.dll`, `bin/libGLESv2.dll`, `bin/d3dcompiler_47.dll`: deploy all
  three beside the application executable, or load them from an explicit
  application-controlled library directory.
- `bin/angle-windows-smoke.exe`, `tests/WindowsEGLSmoke.cpp`: package-level
  smoke test and its source.
- `build-config/`: checked-in and expanded GN arguments, source/toolchain
  revisions, exported symbols, DLL dependencies and smoke-test output.
- `LICENSE` and `SHA256SUMS`: ANGLE license and package file checksums.

Add `include/` to the include search path and link the two import libraries.
Request GLES 3.0 and explicitly choose the D3D11 backend using
`eglGetPlatformDisplayEXT` and `EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE`.
The package uses the static CRT internally (`/MT`); applications still link
against DLL import libraries, not a monolithic ANGLE static library.
Keep objects allocated by ANGLE under ANGLE's API ownership rules.

When integrating into an application that already uses GLAD, use one coherent
GLES function loader and resolve entry points from this ANGLE package. Avoid
mixing GLAD and GLES headers or loading another vendor's `libGLESv2.dll`.

## Verification

CI compiles the smoke test against the staged headers/import libraries and
runs it with the staged DLLs. It explicitly selects D3D11 WARP so GitHub's
hosted runner does not need a physical GPU. The test requires an ES3 context,
compiles and links ESSL 300 shaders, draws and checks pixels on a pbuffer,
then creates a hidden HWND surface and swaps buffers.

Run `bin/angle-windows-smoke.exe` to repeat that software-device check.
Passing it does not validate physical GPU drivers, game performance, ARDA's
MTR teardown, DPI/Preview scaling or device-loss recovery. Those need separate
integration and hardware tests.

Apple's complete-static and Metal compatibility patches are not applied to
this D3D11 DLL build. Its exact upstream ref/commit and packaging commit are
recorded in `build-config/BUILD_INFO.windows.txt`.
