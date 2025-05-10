struct VS_INPUT
{
    float4 pos : POSITION0;
    float3 normal : NORMAL0;
    float2 texCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;
    return output;
}
