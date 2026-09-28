// Прохід тіней для матеріалів з обрізанням за альфою: прозорі за порогом пікселі не кидають тіні
#include "Lit.hlsli"

struct SHADOW_INPUT
{
    float4 pos : SV_POSITION0;
    float2 texCoord : TEXCOORD0;
};

void main(SHADOW_INPUT input)
{
    float4 base = hasMap(MAP_BASE) ? BaseMap.Sample(TextureSampler, transformUv(input.texCoord, tilingOffset)) : float4(1.0f, 1.0f, 1.0f, 1.0f);

    clip(base.a * baseColor.a - extraParams.y);
}
