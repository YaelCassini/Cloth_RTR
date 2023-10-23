// Fragment Shader
#version 330 core
// 点光源数量
#define POINT_LIGHT_NUMBER 1
// precision highp float;
const float PI = 3.14159265359;

#define MEDIUMP_FLT_MAX    65504.0
#define MEDIUMP_FLT_MIN    0.00006103515625
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)

// 输入变量
in vec3 WorldPos;
in vec2 TexCoords;
in vec3 Normal;
in vec3 Tangent;
in vec3 Bitangent;
in vec4 FragPosDirectLightSpace;
in vec4 FragPosLightSpace;

// 输入贴图
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D shadowMap_direct;
uniform sampler2D shadowMap;

uniform int ifUseShadow;

// Image Based Lighting, IBL
// uniform samplerCube irradianceMap;
// uniform samplerCube radianceMap;

// 在ShadowMap上移动一格像素对应的UV值
uniform float xPixelOffset;
uniform float yPixelOffset;

// 平行光
uniform vec3 directionLightDir;
uniform vec3 directionLightColor;

// 点光源
uniform vec3 lightPos;
uniform vec3 lightColor;

// 相机位置
uniform vec3 viewPos;

// 输出的片元颜色
out vec4 FragColor;


// 正态分布函数
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness*roughness;
	float a2 = a*a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;
	
	float nom   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return saturateMediump(nom / denom);
}

// 几何函数（考虑一个遮挡）G_sub
float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;

	float nom   = NdotV;
	float denom = NdotV * (1.0 - k) + k;
	return min(nom / denom, MEDIUMP_FLT_MAX);
}
// 几何函数（考虑了两种遮挡）
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return min(ggx1 * ggx2, MEDIUMP_FLT_MAX);
}

// 菲涅尔方程
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}


// 使用PCF生成软阴影
float PCF(float currentDepth, vec2 nowTexCoord, sampler2D shadowmap)
{
    float shadow;
    float x,y;
    float tmp;
	for (y = -4.0 ; y <=4.0 ; y+=1.0)
		for (x = -4.0 ; x <=4.0 ; x+=1.0) {
            tmp = texture(shadowmap, nowTexCoord + vec2(x*xPixelOffset, y*yPixelOffset)).r;
            shadow += (currentDepth - 0.005) > tmp? 1.0 : 0.0;
        }
			
	shadow /= 81.0;
    return shadow; 
}

// 计算阴影
float ShadowCalculation (vec4 fragPosLightSpace, sampler2D shadowmap) {
    //归一化坐标
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = (projCoords + 1) * 0.5;
    //当前的深度
    float currentDepth = projCoords.z;    
	
	// 使用PCF计算阴影
    float shadow = PCF(currentDepth, projCoords.xy, shadowmap);

    // 超出ShadowMap区域不渲染阴影
    if (projCoords.x > 1.0 || projCoords.y > 1.0 || projCoords.z > 1.0)
        shadow = 0.0;

    if (projCoords.x < 0.0 || projCoords.y < 0.0 || projCoords.z < 0.0)
        shadow = 0.0;

    return shadow;
}

// Bump Mapping
vec3 getNormalFromMap()
{
	// 求TBN矩阵，利用TNB相互垂直的特性求出B
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent- dot(Normal,Tangent)*Normal);
    vec3 B = cross(N,T);
    mat3 TBN = mat3(T,B,N);
 
    // 获取法线图中的法线值MapNormal
    vec3 MapNormal = texture(normalMap, TexCoords).rgb;
    // MapNormal的值变换到[-1,1]
    vec3 normal = 2.0*MapNormal-vec3(1.0,1.0,1.0);
    // 将MapNormal和TNB矩阵相乘就可以变换到物体坐标系
    normal = normalize(TBN*normal);

	return normal;
}

// 主函数
void main()
{
	// 从贴图采样
	vec3 albedo = pow(texture(albedoMap, TexCoords).rgb, vec3(2.2));
	float metallic = texture(metallicMap, TexCoords).r;
	float roughness = texture(roughnessMap, TexCoords).r;
	// float ao = texture(aoMap, TexCoords).r;
	// 实验中未使用AO贴图，因此AO值直接编码为1.0
	float ao = 1.0;

	// Bump Mapping 根据 Normal Map 计算得到片元法线
	vec3 N = getNormalFromMap();

	// 求视线方向
	vec3 V = normalize(viewPos - WorldPos);

	// 常见介电材料的F0被硬编码为0.04
	// 如果是金属（根据Metallic贴图判断），则将Albedo Color作为F0
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	// 反射率方程
	vec3 Lo = vec3(0.0);
	// point light 点光源
	for(int i = 0; i < POINT_LIGHT_NUMBER; ++i) {
		// calculate per-light radiance
		// 计算需要用到的向量
		vec3 L = normalize(lightPos - WorldPos);
		vec3 H = normalize(V + L);

		// 点光源的光照衰减（按距离平方反比衰减）
		float distance = length(lightPos - WorldPos)/10.0;
		float attenuation = 1.0 / (distance * distance);
		// attenuation = 1.0;
		vec3 radiance = lightColor * attenuation;
		

		// Cook-Torrance BRDF
		// 正态分布函数
		float NDF = DistributionGGX(N, H, roughness);
		// 几何函数
		float G   = GeometrySmith(N, V, L, roughness);
		// 菲涅尔方程
		vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

		// 计算镜面反射
		vec3 nominator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		// prevent divide by zero for NdotV=0.0 or NdotL=0.0
		vec3 specular = nominator / max(denominator, 0.001); 

		// Ks is equal to Fresnel
		vec3 kS = F;
		// 因为能量守恒，diffuse与specular二者是互斥的关系。能量总和永远不会超过入射光线的能量。
		vec3 kD = vec3(1.0) - kS;
		// 只有非金属才有diffuse，金属只有specular
		kD *= 1.0 - metallic;

		// 计算by NdotL
		float NdotL = max(dot(N, L), 0.0);

		// Ks实际上就是菲涅尔公式的F，因为金属会更多的吸收折射光线导致漫反射消失
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;  
	}
	// directional light
	{
		vec3 L = normalize(-directionLightDir);
		vec3 H = normalize(V + L);
		vec3 radiance = directionLightColor * 5.0;

		// Cook-Torrance BRDF
		float NDF = DistributionGGX(N, H, roughness);
		float G   = GeometrySmith(N, V, L, roughness);
		vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

		vec3 nominator    = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		vec3 specular = nominator / max(denominator, 0.001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0

		// Ks is equal to Fresnel
		vec3 kS = F;
		// 因为能量守恒，diffuse与specular二者是互斥的关系。能量总和永远不会超过入射光线的能量。
		vec3 kD = vec3(1.0) - kS;
		// 只有非金属才有diffuse，金属只有specular
		kD *= 1.0 - metallic;

		// 计算by NdotL
		float NdotL = max(dot(N, L), 0.0);

		// Ks实际上就是菲涅尔公式的F，因为金属会更多的吸收折射光线导致漫反射消失
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;  
	}

	// 环境光
	vec3 ambient = vec3(0.05,0.05,0.05);
	vec3 ambientColor = ambient * albedo * ao;

	// 计算阴影 shadow为平行光阴影、shadow2为点光源阴影
	float shadow = ShadowCalculation(FragPosDirectLightSpace, shadowMap_direct); 
	float shadow2 = ShadowCalculation(FragPosLightSpace, shadowMap); 
	vec3 lightDir = lightPos - vec3(0,-2,0);

	// 避免一些对光源来说是背面的面被渲染阴影，比如说背面光在人物前面产生阴影
    float backtest = dot(lightDir, N);
    if(backtest<0.2) shadow2 = 0;
	backtest = -dot(directionLightDir, N);
	if(backtest<0.2) shadow = 0;

	// 组合两个阴影
	shadow = (1-shadow*0.3)*(1-shadow2*0.2);
	if(ifUseShadow != 1) shadow = 1.0;


	// 最终颜色
	vec3 color = ambientColor + Lo * shadow;
	// vec3 color = ambientColor + Lo ;
	

	// HDR tonemapping
	color = color / (color + vec3(1.0));
	// 伽马矫正
	color = pow(color, vec3(1.0/2.2));

	FragColor = vec4(color, 1.0);
}