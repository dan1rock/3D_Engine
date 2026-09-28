// Unlit-матеріал: основна карта й колір без освітлення, у лінійному просторі
#include "Lit.hlsli"

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = sampleBaseColor(transformUv(input.texCoord, tilingOffset));

    if (hasFeature(FEATURE_ALPHA_CLIP)) clip(color.a - extraParams.y);

    if (!hasFeature(FEATURE_TRANSPARENT)) return float4(color.rgb, 1.0f);

    if (flags.z == BLEND_PREMULTIPLY) color.rgb *= color.a;
    if (flags.z == BLEND_MULTIPLY) color.rgb = lerp(float3(1.0f, 1.0f, 1.0f), color.rgb, color.a);

    return color;
}
