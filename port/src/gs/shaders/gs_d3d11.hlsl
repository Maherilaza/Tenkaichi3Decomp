// Direct3D 11 HLSL shaders for Tenkaichi3Decomp
// Ported from the GLSL shaders used by the Vulkan and OpenGL backends.
// These are templates that need to be adapted to match the exact GLSL shader semantics.

cbuffer GSConstants : register(b0) {
    float4 viewport;      // x, y, width, height
    float4 texScale;      // texture coordinate scale
    float4 fogColor;      // fog color
    float4 alphaRef;      // alpha test reference
    float4 blendColor;    // blend constant
    uint4 flags;          // rendering flags
};

struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR0;
    float3 texcoord : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float3 texcoord : TEXCOORD0;
    float depth : TEXCOORD1;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    output.texcoord = input.texcoord;
    output.depth = input.position.z;
    return output;
}

Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

struct PSOutput {
    float4 color : SV_TARGET0;
    float aux : SV_TARGET1;
    float depth : SV_DEPTH;
};

PSOutput PSMain(PSInput input) {
    PSOutput output;
    float4 texel = diffuseTexture.Sample(diffuseSampler, input.texcoord.xy);
    output.color = input.color * texel;
    output.aux = output.color.a;
    output.depth = input.depth;
    return output;
}

// TODO: Implement PS2 GS-specific effects:
// - Alpha test with configurable reference and function
// - Destination alpha test (DATE)
// - Framebuffer alpha handling (FBA)
// - Fog and depth tint
// - Outline, glow, and distance blur effects
// - Texture color function (MODULATE, DECAL, HIGHLIGHT, HIGHLIGHT2)
// - Per-pixel depth output for PS2 depth accuracy
// - Texture coordinate wrapping modes and mipmap LOD
