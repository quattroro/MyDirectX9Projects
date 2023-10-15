#ifndef _UTILS_HLSLI_
#define _UTILS_HLSLI_

LightColor CalculateLightColor(int lightIndex, float3 viewNormal, float3 viewPos)
{
    LightColor color = (LightColor)0.f;

    float3 viewLightDir = (float3)0.f;

    float diffuseRatio = 0.f;
    float specularRatio = 0.f;
    float distanceRatio = 1.f;

    if (g_light[lightIndex].lightType == 0)
    {
        // Directional Light
        //아웃에서 사용하는 좌표계는 view좌표계 이기 때문에 맞춰주기 위해서 view 행렬을 곱해준다.
        viewLightDir = normalize(mul(float4(g_light[lightIndex].direction.xyz, 0.f), g_matView).xyz);
        //내적을해서 cos세타 값을 구하고 saturate를 이용해서 혹시라도 아래에서 빛이 들어왔을 경우 -값이 나올 수 있기 때문에 -값을 0으로 바꿔주기 위해서 사용
        //어짜피 cos값이기 때문에 값은 -1~1 사이의 값으로 나온다.
        diffuseRatio = saturate(dot(-viewLightDir, viewNormal));
    }
    else if (g_light[lightIndex].lightType == 1)
    {
        // Point Light
        float3 viewLightPos = mul(float4(g_light[lightIndex].position.xyz, 1.f), g_matView).xyz;
        //광원이 향할 방향
        viewLightDir = normalize(viewPos - viewLightPos);
        diffuseRatio = saturate(dot(-viewLightDir, viewNormal));

        //거리에 따른 빛 감쇠 현상 구현
        float dist = distance(viewPos, viewLightPos);//광원의 위치에서 픽셀까지의 거리를 구한다.
        if (g_light[lightIndex].range == 0.f)
            distanceRatio = 0.f;
        else
            distanceRatio = saturate(1.f - pow(dist / g_light[lightIndex].range, 2));//거리의 최대 거리 지점까지 몇퍼선트 까지 왔는지 구한다. 1- 를 해줌 으로써 멀어질수록 줄어들도록
    }
    else
    {
        // Spot Light
        float3 viewLightPos = mul(float4(g_light[lightIndex].position.xyz, 1.f), g_matView).xyz;

        //광원에서 픽셀까지의 방향
        viewLightDir = normalize(viewPos - viewLightPos);

        //광원의 입사각과 표면의 노말이 이루는 각
        diffuseRatio = saturate(dot(-viewLightDir, viewNormal));

        if (g_light[lightIndex].range == 0.f)
            distanceRatio = 0.f;
        else
        {
            //스포트라이트의 최대 각도의 절반
            float halfAngle = g_light[lightIndex].angle / 2;

            //광원에서 픽셀까지의 벡터
            float3 viewLightVec = viewPos - viewLightPos;

            //스포트라이트의 빛을 쏴줄 방향, 역시 좌표계를 맞춰주기 위해 view 행렬을 곱해준다.
            float3 viewCenterLightDir = normalize(mul(float4(g_light[lightIndex].direction.xyz, 0.f), g_matView).xyz);

            //스포트라이트의 빛의 방향과 광원에서 픽셀까지의 벡터를 내적한다.
            //여기서 광원에서 픽셀까지의 벡터는 단위벡터가 아니기 때문에 결론적으로 
            //해당 값은 광원에서 픽셀까지의 벡터를 스포트라이트의 빛의 방향에 직교투영 한 길이가 된다.
            //내적이기 떄문에 음수가 나올 수도 있다.
            float centerDist = dot(viewLightVec, viewCenterLightDir);
            distanceRatio = saturate(1.f - centerDist / g_light[lightIndex].range);

            //시야각을 구해준다.
            //광원에서 픽셀까지의 벡터를 단워벡터로 바꿔주고 스포트라이트의 빛을 쏴줄 방향 벡터와 내적해서
            //cos세타 값을 구해주고 해당 값을 acos을 이용해 광원의 방향과 이루는 각을 구해준다.
            float lightAngle = acos(dot(normalize(viewLightVec), viewCenterLightDir));

            if (centerDist < 0.f || centerDist > g_light[lightIndex].range) // 최대 거리를 벗어났는지
                distanceRatio = 0.f;
            else if (lightAngle > halfAngle) // 최대 시야각을 벗어났는지
                distanceRatio = 0.f;
            else // 거리에 따라 적절히 세기를 조절
                distanceRatio = saturate(1.f - pow(centerDist / g_light[lightIndex].range, 2));
        }
    }

    //정반사 벡터를 구해주는 부분
    //입사 벡터와 노말을 이용해서 각을 구한 다음에(각이 노말방향으로 올라가야하는 거리의 절반의 크기) 
    //노말에 거리를 곱해서 위로가는 벡터를 구한 다음에 두번 더해줘서 최종적으로 반사벡터를 구해준다.
    float3 reflectionDir = normalize(viewLightDir + 2 * (saturate(dot(-viewLightDir, viewNormal)) * viewNormal));

    //그러곤 물체의 위치에서 카메라로 가는 벡터를 구해야 하는데 
    //해당 좌표계는 View 좌표계 이기 때문에 모든것이 카메라가 기준이다.(0,0,0)
    //따라서 그냥 viewPos를 노멀라이즈 하면 카메라에서 대상으로 가는 벡터가 나온다.
    float3 eyeDir = normalize(viewPos);

    //정반사벡터와 카메라 벡터의 각도를 구한다.
    specularRatio = saturate(dot(-eyeDir, reflectionDir));

    //구한 스펙큘러 값을 제곱 해줘서 더 강하게, 저더 좁은 영역에 도드라지도록 해 준다. (cos 그래프를 제곱해 보면 알 수 있다.)
    specularRatio = pow(specularRatio, 2);

    //원래 빛의 diffuse값에서 cos세타 만큼 곱해주고 거리 별 빛 세기 감소량을 곱해준다.
    color.diffuse = g_light[lightIndex].color.diffuse * diffuseRatio * distanceRatio;
    //빛 자체의 ambient값 그대로 적용
    color.ambient = g_light[lightIndex].color.ambient * distanceRatio;
    color.specular = g_light[lightIndex].color.specular * specularRatio * distanceRatio;

    return color;
}


#endif