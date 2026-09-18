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

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 color = Source.Sample(SourceSampler, input.texCoord).rgb;

    float threshold = bloomParams.x;
    float knee = max(bloomParams.y, 0.0001f);

    float brightness = max(color.r, max(color.g, color.b));

	// М'яке коліно робить перехід до світіння плавним замість різкої межі
    float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee);

    float contribution = max(soft, brightness - threshold) / max(brightness, 0.0001f);

    return float4(color * contribution, 1.0f);
}
