Texture2D Source : register(t2);
sampler SourceSampler : register(s2);

struct PS_INPUT
{
    float4 pos : SV_POSITION0;
    float2 texCoord : TEXCOORD0;
};

cbuffer postProcess : register(b0)
{
    float4 texelSize;
    float4 blurDirection;
    float4 bloomParams;
    float4 vignetteParams;
    float4 vignetteColor;
};

// Ваги дев'ятитапового фільтра Гауса
static const float weights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

float4 main(PS_INPUT input) : SV_TARGET
{
	// Розмиття виконується окремо по горизонталі та вертикалі, напрямок задає константа
    float2 offset = blurDirection.xy * texelSize.xy * blurDirection.z;

    float3 result = Source.Sample(SourceSampler, input.texCoord).rgb * weights[0];

    [unroll]
    for (int i = 1; i < 5; i++)
    {
        result += Source.Sample(SourceSampler, input.texCoord + offset * i).rgb * weights[i];
        result += Source.Sample(SourceSampler, input.texCoord - offset * i).rgb * weights[i];
    }

    return float4(result, 1.0f);
}
