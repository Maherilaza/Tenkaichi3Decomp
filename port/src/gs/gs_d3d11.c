/*
 * Graphics Synthesizer, Direct3D 11 back end
 *
 * This is a DirectX 11 renderer for older Windows PCs that don't support DirectX 12 or Vulkan.
 * It provides an alternative to the OpenGL 3.3 fallback renderer with better performance on
 * older hardware that has good D3D11 driver support.
 *
 * Architecture mirrors gs_gl.c: records frames from gs_draw.c and replays them using D3D11 API.
 * Requires Windows 7 SP1 or later with Platform Update.
 */

#ifdef _WIN32

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"
#include "gs_draw.h"
#include "shaders.h"
#include "ui.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

extern int Port_AspectMilli(void);

/* ---- D3D11 State ---- */
static ID3D11Device *sDevice;
static ID3D11DeviceContext *sContext;
static IDXGISwapChain *sSwapChain;
static ID3D11RenderTargetView *sBackBufferRTV;

/* Shaders */
static ID3D11VertexShader *sVS;
static ID3D11PixelShader *sPS;
static ID3D11InputLayout *sInputLayout;

/* Buffers */
static ID3D11Buffer *sVertexBuffer;
static ID3D11Buffer *sIndexBuffer;
static ID3D11Buffer *sConstantBuffer;

/* Render targets (512x448 color + depth) */
#define MAX_TARGETS 8
static ID3D11Texture2D *sColorTextures[MAX_TARGETS];
static ID3D11Texture2D *sAuxTextures[MAX_TARGETS];
static ID3D11Texture2D *sDepthTextures[MAX_TARGETS];
static ID3D11RenderTargetView *sColorRTVs[MAX_TARGETS];
static ID3D11RenderTargetView *sAuxRTVs[MAX_TARGETS];
static ID3D11DepthStencilView *sDSVs[MAX_TARGETS];
static ID3D11ShaderResourceView *sColorSRVs[MAX_TARGETS];
static ID3D11ShaderResourceView *sDepthSRVs[MAX_TARGETS];

/* Samplers (16 combinations: filtered, clamp U, clamp V, mipmap) */
static ID3D11SamplerState *sSamplers[16];

/* Blend states */
static ID3D11BlendState *sBlendStates[256]; /* Cached by blend key */

/* Depth stencil states */
static ID3D11DepthStencilState *sDepthStates[8]; /* NEVER, ALWAYS, GEQUAL, GREATER */

/* Rasterizer state */
static ID3D11RasterizerState *sRasterizerState;

/* Texture cache */
#define MAX_TEXTURES 2048
static struct {
    ID3D11Texture2D *tex;
    ID3D11ShaderResourceView *srv;
    uint32_t w, h;
} sTextures[MAX_TEXTURES];
static int sTextureCount;

/* Vertex buffer size */
#define VERTEX_BUFFER_SIZE (16 * 1024 * 1024)
#define INDEX_BUFFER_SIZE (4 * 1024 * 1024)

/* ---- Initialization ---- */

static int d3d11_init(void *window_handle) {
    HWND hwnd = (HWND)window_handle;
    HRESULT hr;

    /* Create device and swap chain */
    DXGI_SWAP_CHAIN_DESC scd = {0};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = 1024;
    scd.BufferDesc.Height = 896;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL featureLevel;

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        createFlags,
        featureLevels,
        sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION,
        &scd,
        &sSwapChain,
        &sDevice,
        &featureLevel,
        &sContext
    );

    if (FAILED(hr)) {
        fprintf(stderr, "bt3: D3D11CreateDeviceAndSwapChain failed: 0x%08X\n", hr);
        return 0;
    }

    fprintf(stderr, "bt3: D3D11 initialized (feature level %d.%d)\n",
            (featureLevel >> 12) & 0xF, (featureLevel >> 8) & 0xF);

    /* Create back buffer RTV */
    ID3D11Texture2D *backBuffer;
    hr = sSwapChain->lpVtbl->GetBuffer(sSwapChain, 0, &IID_ID3D11Texture2D, (void**)&backBuffer);
    if (FAILED(hr)) {
        fprintf(stderr, "bt3: GetBuffer failed\n");
        return 0;
    }

    hr = sDevice->lpVtbl->CreateRenderTargetView(sDevice, (ID3D11Resource*)backBuffer, NULL, &sBackBufferRTV);
    backBuffer->lpVtbl->Release(backBuffer);
    if (FAILED(hr)) {
        fprintf(stderr, "bt3: CreateRenderTargetView failed\n");
        return 0;
    }

    /* Create vertex buffer */
    D3D11_BUFFER_DESC bd = {0};
    bd.ByteWidth = VERTEX_BUFFER_SIZE;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = sDevice->lpVtbl->CreateBuffer(sDevice, &bd, NULL, &sVertexBuffer);
    if (FAILED(hr)) {
        fprintf(stderr, "bt3: CreateBuffer (vertex) failed\n");
        return 0;
    }

    /* Create index buffer */
    bd.ByteWidth = INDEX_BUFFER_SIZE;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    hr = sDevice->lpVtbl->CreateBuffer(sDevice, &bd, NULL, &sIndexBuffer);
    if (FAILED(hr)) {
        fprintf(stderr, "bt3: CreateBuffer (index) failed\n");
        return 0;
    }

    /* Create constant buffer */
    bd.ByteWidth = 256;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = sDevice->lpVtbl->CreateBuffer(sDevice, &bd, NULL, &sConstantBuffer);
    if (FAILED(hr)) {
        fprintf(stderr, "bt3: CreateBuffer (constant) failed\n");
        return 0;
    }

    /* Create samplers */
    for (int i = 0; i < 16; i++) {
        D3D11_SAMPLER_DESC sd = {0};
        sd.Filter = (i & 1) ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = (i & 2) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV = (i & 4) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = (i & 8) ? D3D11_FLOAT32_MAX : 0;
        hr = sDevice->lpVtbl->CreateSamplerState(sDevice, &sd, &sSamplers[i]);
        if (FAILED(hr)) {
            fprintf(stderr, "bt3: CreateSamplerState %d failed\n", i);
            return 0;
        }
    }

    /* Create rasterizer state (no culling) */
    D3D11_RASTERIZER_DESC rd = {0};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    hr = sDevice->lpVtbl->CreateRasterizerState(sDevice, &rd, &sRasterizerState);
    if (FAILED(hr)) {
        fprintf(stderr, "bt3: CreateRasterizerState failed\n");
        return 0;
    }

    /* TODO: Compile shaders from HLSL */
    /* TODO: Create render targets */

    return 1;
}

static void d3d11_term(void) {
    /* Release all D3D11 resources */
    if (sContext) sContext->lpVtbl->ClearState(sContext);

    for (int i = 0; i < sTextureCount; i++) {
        if (sTextures[i].srv) sTextures[i].srv->lpVtbl->Release(sTextures[i].srv);
        if (sTextures[i].tex) sTextures[i].tex->lpVtbl->Release(sTextures[i].tex);
    }

    for (int i = 0; i < MAX_TARGETS; i++) {
        if (sColorSRVs[i]) sColorSRVs[i]->lpVtbl->Release(sColorSRVs[i]);
        if (sDepthSRVs[i]) sDepthSRVs[i]->lpVtbl->Release(sDepthSRVs[i]);
        if (sColorRTVs[i]) sColorRTVs[i]->lpVtbl->Release(sColorRTVs[i]);
        if (sAuxRTVs[i]) sAuxRTVs[i]->lpVtbl->Release(sAuxRTVs[i]);
        if (sDSVs[i]) sDSVs[i]->lpVtbl->Release(sDSVs[i]);
        if (sColorTextures[i]) sColorTextures[i]->lpVtbl->Release(sColorTextures[i]);
        if (sAuxTextures[i]) sAuxTextures[i]->lpVtbl->Release(sAuxTextures[i]);
        if (sDepthTextures[i]) sDepthTextures[i]->lpVtbl->Release(sDepthTextures[i]);
    }

    for (int i = 0; i < 16; i++) {
        if (sSamplers[i]) sSamplers[i]->lpVtbl->Release(sSamplers[i]);
    }

    if (sRasterizerState) sRasterizerState->lpVtbl->Release(sRasterizerState);
    if (sConstantBuffer) sConstantBuffer->lpVtbl->Release(sConstantBuffer);
    if (sIndexBuffer) sIndexBuffer->lpVtbl->Release(sIndexBuffer);
    if (sVertexBuffer) sVertexBuffer->lpVtbl->Release(sVertexBuffer);
    if (sInputLayout) sInputLayout->lpVtbl->Release(sInputLayout);
    if (sPS) sPS->lpVtbl->Release(sPS);
    if (sVS) sVS->lpVtbl->Release(sVS);
    if (sBackBufferRTV) sBackBufferRTV->lpVtbl->Release(sBackBufferRTV);
    if (sSwapChain) sSwapChain->lpVtbl->Release(sSwapChain);
    if (sContext) sContext->lpVtbl->Release(sContext);
    if (sDevice) sDevice->lpVtbl->Release(sDevice);
}

/* ---- Texture Management ---- */

static uint64_t d3d11_texture_create(uint32_t w, uint32_t h, uint32_t format) {
    if (sTextureCount >= MAX_TEXTURES) {
        fprintf(stderr, "bt3: texture limit reached\n");
        return 0;
    }

    DXGI_FORMAT dxgiFormat;
    switch (format & 3) {
        case 0: dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        case 1: dxgiFormat = DXGI_FORMAT_BC1_UNORM; break;
        case 2: dxgiFormat = DXGI_FORMAT_BC2_UNORM; break;
        case 3: dxgiFormat = DXGI_FORMAT_BC3_UNORM; break;
        default: dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = w;
    td.Height = h;
    td.MipLevels = (format & 0x10) ? 0 : 1;
    td.ArraySize = 1;
    td.Format = dxgiFormat;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D *tex;
    HRESULT hr = sDevice->lpVtbl->CreateTexture2D(sDevice, &td, NULL, &tex);
    if (FAILED(hr)) {
        fprintf(stderr, "bt3: CreateTexture2D failed\n");
        return 0;
    }

    ID3D11ShaderResourceView *srv;
    hr = sDevice->lpVtbl->CreateShaderResourceView(sDevice, (ID3D11Resource*)tex, NULL, &srv);
    if (FAILED(hr)) {
        tex->lpVtbl->Release(tex);
        fprintf(stderr, "bt3: CreateShaderResourceView failed\n");
        return 0;
    }

    int idx = sTextureCount++;
    sTextures[idx].tex = tex;
    sTextures[idx].srv = srv;
    sTextures[idx].w = w;
    sTextures[idx].h = h;

    return (uint64_t)(idx + 1);
}

static void d3d11_texture_upload(uint64_t handle, uint32_t level, const void *data, uint32_t bytes) {
    int idx = (int)handle - 1;
    if (idx < 0 || idx >= sTextureCount) return;

    /* TODO: Handle compressed formats and mip levels */
    sContext->lpVtbl->UpdateSubresource(sContext, (ID3D11Resource*)sTextures[idx].tex,
                                        level, NULL, data, sTextures[idx].w * 4, 0);
}

static void d3d11_texture_destroy(uint64_t handle) {
    int idx = (int)handle - 1;
    if (idx < 0 || idx >= sTextureCount) return;

    if (sTextures[idx].srv) {
        sTextures[idx].srv->lpVtbl->Release(sTextures[idx].srv);
        sTextures[idx].srv = NULL;
    }
    if (sTextures[idx].tex) {
        sTextures[idx].tex->lpVtbl->Release(sTextures[idx].tex);
        sTextures[idx].tex = NULL;
    }
}

/* ---- Drawing ---- */

static void d3d11_begin_frame(void) {
    /* Clear state */
}

static void d3d11_end_frame(void) {
    /* Present */
    sSwapChain->lpVtbl->Present(sSwapChain, 1, 0);
}

static void d3d11_draw(const GsDraw *draw, const void *vertices, uint32_t vertex_count) {
    /* TODO: Implement draw call submission */
    /* 1. Update vertex buffer */
    /* 2. Set pipeline state (blend, depth, shaders) */
    /* 3. Bind textures and samplers */
    /* 4. Set render targets */
    /* 5. Draw */
}

/* ---- Backend Interface ---- */

GsBackend sD3D11Backend = {
    .init = d3d11_init,
    .term = d3d11_term,
    .texture_create = d3d11_texture_create,
    .texture_upload = d3d11_texture_upload,
    .texture_destroy = d3d11_texture_destroy,
    .begin_frame = d3d11_begin_frame,
    .end_frame = d3d11_end_frame,
    .draw = d3d11_draw,
};

#endif /* _WIN32 */
