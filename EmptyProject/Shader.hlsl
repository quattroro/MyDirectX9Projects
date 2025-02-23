float4 g_Color;
float4x4 g_mWorldViewProjection; // World * View * Projection matrix

float4 vs_PositionOnly(float4 vPos : POSITION) : POSITION
{
    return mul(float4(vPos.xyz, 1), g_mWorldViewProjection);
}

float4 ps_ConstantColor() : COLOR
{
    return g_Color;
}


technique RenderScene
{
    pass P0
    {
        VertexShader = compile vs_2_a vs_PositionOnly();
        PixelShader = compile ps_2_a ps_ConstantColor();
    }
}
