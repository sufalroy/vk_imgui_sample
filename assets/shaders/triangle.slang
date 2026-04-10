struct VSInput {
    float3 position : POSITION;
    float3 color    : COLOR;
    float2 uv       : TEXCOORD0;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float4 sv_position : SV_Position;
    float3 color;
    float2 frag_uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.sv_position = mul(ubo.proj,
                         mul(ubo.view,
                         mul(ubo.model, float4(input.position, 1.0))));
    output.color = input.color;
    output.frag_uv = input.uv;
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    return texture.Sample(input.frag_uv);
}