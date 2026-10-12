# DirectX 11 Backend Integration

## Overview

This document describes the DirectX 11 backend for Tenkaichi3Decomp, intended for older Windows PCs that don't support DirectX 12 or Vulkan but have good D3D11 driver support.

## Current Status

**Skeleton implementation** - The backend structure, device initialization, texture management, and shader templates are in place. The renderer is NOT yet functional and cannot render frames.

## Architecture

The D3D11 backend follows the same architecture as the OpenGL backend (`gs_gl.c`):

1. `gs_draw.c` records frame data as `GsDraw` primitives
2. The backend replays the draw list using D3D11 API calls
3. Render targets replicate the PS2's 512x448 framebuffers
4. Shaders implement PS2 GS effects (alpha test, fog, depth tint, outline, glow)

## Files

- `port/src/gs/gs_d3d11.c` - D3D11 backend implementation (skeleton)
- `port/src/gs/shaders/gs_d3d11.hlsl` - HLSL shader templates

## Requirements

- Windows 7 SP1 with Platform Update or later (Windows 8+ recommended)
- DirectX 11 capable GPU (feature level 10.0 or higher)
- DirectX SDK / Windows SDK with D3D11 headers and libraries
- SDL3 for window management

## Implementation Roadmap

### Phase 1: Core Rendering (Required)

- [ ] Verify the actual `GsBackend` interface in `gs_draw.h` and match all function signatures
- [ ] Implement window handle extraction via SDL3 (`SDL_GetPointerProperty` with `SDL_PROP_WINDOW_WIN32_HWND_POINTER`)
- [ ] Compile HLSL shaders and create input layouts
- [ ] Create 512x448 render targets (color RGBA8, aux R8, depth D32_FLOAT)
- [ ] Implement vertex and index buffer uploads
- [ ] Implement draw call submission with proper pipeline state
- [ ] Implement blend state caching (normal, subtractive, additive, custom)
- [ ] Implement depth stencil states (NEVER, ALWAYS, GEQUAL, GREATER)
- [ ] Implement texture uploads with correct BC1/BC2/BC3 row pitches and mip levels
- [ ] Implement framebuffer presentation to swap chain
- [ ] Wire backend into renderer selection in `gs_draw.c`
- [ ] Add D3D11 libraries to Windows build configuration

### Phase 2: PS2 Effects (Required for Visual Parity)

- [ ] Port GLSL shader logic to HLSL (alpha test, DATE, FBA)
- [ ] Port fog, depth tint, outline, glow, and distance blur
- [ ] Handle split-screen rendering
- [ ] Handle movie texture uploads
- [ ] Implement texture packs (DDS/PNG)

### Phase 3: Testing and Optimization

- [ ] Test on Windows 7/8/10 with feature levels 10.0, 10.1, 11.0
- [ ] Compare screenshots against Vulkan and OpenGL backends
- [ ] Profile on older GPUs (Intel HD 4000, GTX 400/500, AMD HD 5000/6000)
- [ ] Test VSync, window resizing, fullscreen switching
- [ ] Test device loss and recovery
- [ ] Test all game modes including online, split-screen, movies

## Known Limitations

- The skeleton does not compile against the real backend interface until signatures are aligned.
- The HLSL shaders are templates and do not implement PS2 GS semantics yet.
- The renderer is not wired into the build or runtime selection.
- No Windows build or runtime test has been performed.
- This work does not include game assets; users must provide their own disc image.

## Build Notes

When integration is complete, the Windows build must link against:
- `d3d11.lib`
- `dxgi.lib`
- `d3dcompiler.lib`

The code uses COM interfaces directly (C-style vtable calls). Ensure `_WIN32` guards prevent compilation on other platforms.
