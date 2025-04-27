Texture2D Texture : register(t0);
sampler TextureSampler : register(s0);

struct PS_INPUT {
	float4 pos: SV_POSITION0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD0;
};

cbuffer constant : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 projection;
    unsigned int time;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return Texture.Sample(TextureSampler, input.texCoord);

}
