# ANGLE Metal XCFramework

This package contains a complete static `libANGLE.a` wrapped in
`ANGLE.xcframework`. The archive exports both EGL and OpenGL ES entry points;
only the Metal renderer backend is compiled in. One XCFramework contains iOS
arm64 device, iOS arm64 simulator, and macOS arm64 platform slices.

## Xcode integration

1. Drag `ANGLE.xcframework` into the application target and select **Do Not
   Embed** (it is a static library).
2. Add `-ObjC` to **Other Linker Flags**. If the application has no C++ or
   Objective-C++ source, also link `libc++`.
3. Link these Apple frameworks:
   - iOS: `Metal`, `QuartzCore`, `IOSurface`, `Foundation`, `CoreFoundation`,
     `CoreGraphics`
   - macOS: all of the above plus `Cocoa` and `IOKit`
4. Include ANGLE APIs with headers such as `<EGL/egl.h>` and `<GLES3/gl3.h>`.

The `build-config` directory records the exact GN arguments, expanded GN
arguments, ANGLE ref, source commit and packaging repository commit used for
the package. It also includes `patches/` and `PATCHES.sha256` so the source
modifications can be reproduced.

## Metal shader compatibility

The build includes the Metal helper address-space fix for applications
targeting iOS/macOS 27. It adds the trailing `thread` qualifier to ANGLE's
out/inout, vector-element and swizzle conversion operators, preventing MSL
shader compilation failures followed by `GL_INVALID_OPERATION (0x502)`.
