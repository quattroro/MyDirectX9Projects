//--------------------------------------------------------------------------------------
// 오디션 SDFFont 셰이더
//--------------------------------------------------------------------------------------

float4 	    _baseColor;		// BaseColor
float4x4 	_matWorld;              	// World matrix for object
float4x4 	_matView;              		// View matrix for object
float4x4 	_matProj;              		// Projection matrix for object
float4x4 	_matWorldViewProjection;	// World * View * Projection matrix

float _smoothing;
float _lowThreshold;
float _highThreshold;
float4 _borderColor;
float _borderAlpha;
float _FontThickness;
float _FontBorderWidth;
float _NeonPower;
float _NeonBrightness;

float _TestFontWidth;
float _TestOutlineWidth;
float _BorderWidth;
float _ShadowDist;

float       _OutlineWidth = 0.00001;
float3      _MainTex_TexelSize = { 0.0014, 0.0014,0.00 };
float4      _OutlineColor = { 1.0,1.0,1.0,1.0 };

//--------------------------------------------------------------------------------------
// 메인 텍스쳐 셈플러
//--------------------------------------------------------------------------------------
sampler _FontSampler : register(s0);


//--------------------------------------------------------------------------------------
// 정점 셰이더 입력값
//--------------------------------------------------------------------------------------
struct VS_INPUT 
{
   float4 Position : POSITION0;
   float2 TexCoord0 : TEXCOORD0;
   
};

//--------------------------------------------------------------------------------------
// 픽셀 셰이더에 넘겨줄 정점 셰이더 출력값
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
   float4 Position : POSITION;
   float2 TexCoord0 : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// 정점 셰이더
//--------------------------------------------------------------------------------------
VS_OUTPUT Vert(VS_INPUT In)
{
    VS_OUTPUT Output;
   Output.Position = mul(In.Position, _matWorldViewProjection);
   Output.TexCoord0 = In.TexCoord0;
   return Output;
}


//--------------------------------------------------------------------------------------
// 픽셀 셰이더 출력값
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
   float4 Color : COLOR0;
   float4 EmissiveColor : COLOR1;
};



PS_OUTPUT Frag(VS_OUTPUT In)
{
   PS_OUTPUT Out;
   float4 color = tex2D(_FontSampler, In.TexCoord0);
   color *= _baseColor;
   Out.Color = color;
   Out.EmissiveColor = float4(0,0,0,color.a);
   return Out;
}

PS_OUTPUT SoftEdgeFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;
   float4 color = _baseColor;

   color.a = smoothstep(0.5 - _smoothing, 0.5 + _smoothing, tex2D(_FontSampler, In.TexCoord0).a);

   Out.Color = color;
   Out.EmissiveColor = float4(0,0,0,color.a);
   return Out;
}




PS_OUTPUT SharpEdgeFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;
    float4 color = _baseColor;
   float alpha = tex2D(_FontSampler, In.TexCoord0).a;
   
   if(alpha >= _lowThreshold)
   {
	color.rgb = _baseColor;
	color.a = 1.0;
   }
   else
   {
	color.rgb = _baseColor;
	color.a = 0.0;
   }

   Out.Color = color;
   Out.EmissiveColor = float4(0,0,0,color.a);
   return Out;
}

PS_OUTPUT SharpEdgeWithGlowFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;
   float4 color = float4(0,0,0,0);
   float alpha = tex2D(_FontSampler, In.TexCoord0).a;
   if(alpha >= _lowThreshold)
   {
      color.rgb = _baseColor;
      color.a = 1.0;
   }
   else if(alpha>0.0f)
   {
      color.rgb = _borderColor;
      color.a = _borderAlpha;
   }

   Out.Color = color;
   Out.EmissiveColor = float4(0, 0, 0, color.a);
   return Out;

}

PS_OUTPUT BorderFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;

    float4 color = float4(0, 0, 0, 0);
    float alpha = tex2D(_FontSampler, In.TexCoord0).a;
    alpha = smoothstep(0.5 - _smoothing, 0.5 + _smoothing, tex2D(_FontSampler, In.TexCoord0).a);
    if (alpha >= _lowThreshold && alpha <= _highThreshold)
    {
        color.rgb = _borderColor;
        //color.a = 1;
        color.a = alpha + 0.3;
    }
    else if (alpha >= _highThreshold)
    {
        color.rgb = _baseColor;
        //color.a = 1;
        color.a = alpha;
    }
    else
    {
        color.a = 0.0;
    }


    Out.Color = color;
    Out.EmissiveColor = float4(0, 0, 0, color.a);
    return Out;
}



PS_OUTPUT SoftEdgeFrag2(VS_OUTPUT In)
{
    PS_OUTPUT Out;
    float4 color = _baseColor;
    float alpha = tex2D(_FontSampler, In.TexCoord0).a;
    if (alpha < 0.5)
    {
        color.a = smoothstep(0.5 - _smoothing, 0.5 + _smoothing, alpha);
    }
    else
    {
        color.a = smoothstep(0.5 - _smoothing, 0.5 + _smoothing, alpha * 0.75);
    }

    Out.Color = color;
    Out.EmissiveColor = float4(0, 0, 0, color.a);
    return Out;
}


PS_OUTPUT NewSoftEdgeFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;
    float4 color = _baseColor;

    float alpha = tex2D(_FontSampler, In.TexCoord0).a;
    color = float4(0, 0, 0, 0);
      
    float Thickness = clamp(1 - _TestFontWidth, 0, 1);
    float t = alpha / Thickness;
    t = t * t * t;
    t = clamp(t, 0.0, 1.0);
    color = lerp(float4(0, 0, 0, 0), _baseColor, t);
    
    Out.Color = color;
    Out.EmissiveColor = float4(0, 0, 0, color.a);
    return Out;
}

PS_OUTPUT NewBorderFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;

    //SoftBorder
    float4 color = float4(0, 0, 0, 0);
    float alpha = tex2D(_FontSampler, In.TexCoord0).a;
    float d = alpha;
    color = float4(0, 0, 0, 0);
      
    float fontWidth = clamp(1 - _TestFontWidth, 0, 1);
    float outlineWidth = clamp(1 - _TestOutlineWidth, 0, 1);
    float totalWidth = clamp(1 - (_TestFontWidth + _TestOutlineWidth), 0, 1);
      
      //color = float4(d,d,d,d);
      
    if (d < totalWidth)//Total Font Inside
    {
        float t = d / (totalWidth);
        t = t * t * t * t;
        color = lerp(float4(0, 0, 0, 0), _borderColor, t);
    }
    else if (d < fontWidth)//Font Outline
    {
        float t = d / (fontWidth);
        t = t * t * t * t;
        color = lerp(_borderColor, _baseColor, t);
    }
    else //Outline
    {
        color = _baseColor;
    }

    Out.Color = color;
    Out.EmissiveColor = float4(0, 0, 0, color.a);
    return Out;
}


PS_OUTPUT NewNeonFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;
    float4 color = _baseColor;
    float d = tex2D(_FontSampler, In.TexCoord0).a - clamp(1 - (_TestFontWidth + 0.2), 0, 1);
    color = float4(0, 0, 0, 0);


      //only do something if within range of border
    if (d > -_BorderWidth && d < _BorderWidth)
    {
         //calculate a value of 't' that goes from 0->1->0
         //around the edge of the geometry
        float t = d / _BorderWidth; //[-1:0:1]
        t = 1 - abs(t); //[0:1:0]


         //lerp between background and border using t
        color = lerp(float4(0, 0, 0, 0), _borderColor, t);


         //raise t to a high power and add in as white
         //to give bloom effect
        color.rgb += pow(t, _NeonPower) * _NeonBrightness;
    }


    color.a = color.a * _baseColor.a;

    Out.Color = color;
    Out.EmissiveColor = float4(0, 0, 0, color.a);
    return Out;
}

PS_OUTPUT NewDropShadowFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;
    
    float4 color = _baseColor;
    color = float4(0, 0, 0, 0);
    float d = tex2D(_FontSampler, In.TexCoord0).a;
    float d2 = tex2D(_FontSampler, In.TexCoord0 + _ShadowDist * _MainTex_TexelSize.xy).a;


    float Thickness = clamp(1 - _TestFontWidth, 0, 1);
      
    float fill_t = saturate((d) / Thickness);
    fill_t = fill_t * fill_t * fill_t;
    float shadow_t = saturate((d2) / Thickness);
    shadow_t = shadow_t * shadow_t * shadow_t;
      
    color = lerp(color, _borderColor, shadow_t);
    color = lerp(color, _baseColor, fill_t);
      
    color.a = color.a * _baseColor.a;

    Out.Color = color;
    Out.EmissiveColor = float4(0, 0, 0, color.a);
    return Out;
}

//PS_OUTPUT NeonFrag(VS_OUTPUT In)
//{
//    PS_OUTPUT Out;
//    float d = tex2D(_FontSampler, In.TexCoord0).a;
//    float4 color = _baseColor;
//    color = float4(0, 0, 0, 0);

//    float Thickness = 1.0 - _FontThickness;
//    float BoarderWidth = _FontBorderWidth * 0.5;


//    if (d < Thickness && d > Thickness - _FontBorderWidth)
//    {
//        float t = d / (Thickness - BoarderWidth);// [1+a:1:0]
//        t = 1 - t;// [1:0:-1]
//        t = 1 - abs(t);// [0:1:0]

//        //lerp between background and border using t
//        color = lerp(float4(0, 0, 0, 0), _borderColor, t);

//        //raise t to a high power and add in as white
//        //to give bloom effect
//        color.rgb += pow(t, _NeonPower) * _NeonBrightness;
//    }

//    Out.Color = color;
//    Out.EmissiveColor = float4(0, 0, 0, color.a);
//    return Out;
    
    
//    /*PS_OUTPUT Out;
//    float4 color = _baseColor;
//    float alpha = tex2D(_FontSampler, In.TexCoord0).a;
//    if (alpha < 0.5)
//    {
//        color.a = smoothstep(0.5 - _smoothing, 0.5 + _smoothing, alpha);
//    }
//    else
//    {
//        color.a = smoothstep(0.5 - _smoothing, 0.5 + _smoothing, alpha * 0.75);
//    }

//    Out.Color = color;
//    Out.EmissiveColor = float4(0, 0, 0, color.a);
//    return Out;*/
//}

PS_OUTPUT DebugFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;
    float4 color = _baseColor;
    color.a = 1.0;
    Out.Color = color;
    Out.EmissiveColor = float4(0, 0, 0, color.a);
    return Out;
}

PS_OUTPUT RectDrawFrag(VS_OUTPUT In)
{
    PS_OUTPUT Out;
    float4 color = tex2D(_FontSampler, In.TexCoord0);
    Out.Color = color;
    Out.EmissiveColor = float4(0, 0, 0, color.a);
    return Out;
}

technique BasicDraw
{
    pass P0
    {          
        VertexShader = compile vs_2_a Vert();
        PixelShader  = compile ps_2_a Frag();
    }
}


technique SoftEdgeDraw
{
    pass P0
    {          
        VertexShader = compile vs_2_a Vert();
        PixelShader  = compile ps_2_a SoftEdgeFrag();
    }
}

technique SharpEdgeDraw
{
    pass P0
    {          
        VertexShader = compile vs_2_a Vert();
        PixelShader  = compile ps_2_a SharpEdgeFrag();
    }
}

technique SharpEdgeWithGlowDraw
{
    pass P0
    {          
        VertexShader = compile vs_2_a Vert();
        PixelShader  = compile ps_2_a SharpEdgeWithGlowFrag();
    }
}

technique BorderDraw
{
    pass P0
    {          
        VertexShader = compile vs_2_a Vert();
        PixelShader  = compile ps_2_a BorderFrag();
    }
}



technique SoftEdge2Draw
{
    pass P0
    {          
        VertexShader = compile vs_2_a Vert();
        PixelShader  = compile ps_2_a SoftEdgeFrag2();
    }
}
technique DebugDraw
{
    pass P0
    {
        VertexShader = compile vs_2_a Vert();
        PixelShader = compile ps_2_a DebugFrag();
    }
}

technique NewSoftDraw
{
    pass P0
    {
        VertexShader = compile vs_2_a Vert();
        PixelShader = compile ps_2_a NewSoftEdgeFrag();
    }
}

technique NewBorderDraw
{
    pass P0
    {
        VertexShader = compile vs_2_a Vert();
        PixelShader = compile ps_2_a NewBorderFrag();
    }
}

technique NewNeonDraw
{
    pass P0
    {
        VertexShader = compile vs_2_a Vert();
        PixelShader = compile ps_2_a NewNeonFrag();
    }
}

technique NewDropShadow
{
    pass P0
    {
        VertexShader = compile vs_2_a Vert();
        PixelShader = compile ps_2_a NewDropShadowFrag();
    }
}

technique RectDraw
{
    pass P0
    {
        VertexShader = compile vs_2_a Vert();
        PixelShader = compile ps_2_a RectDrawFrag();
    }
}