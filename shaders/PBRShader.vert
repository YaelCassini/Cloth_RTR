// Vertex Shader
#version 330 core
// 顶点变量（由Assimp库读入模型时计算得到）
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

// 输出到Fragment Shader的变量
out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 Tangent;
out vec3 Bitangent;

// 在光源为视点的投影空间中的坐标（直射光源和点光源）
out vec4 FragPosDirectLightSpace;
out vec4 FragPosLightSpace;

// 空间变换矩阵
layout(std140) uniform Matrices
{
	mat4 projection;
	mat4 view;
};
uniform mat4 model;
uniform mat3 normalMatrix;

// 从模型空间到光源为视点的投影空间的变换矩阵
uniform mat4 directlightSpaceMatrix;
uniform mat4 lightSpaceMatrix;


void main()
{
	// 片元在投影空间的坐标
	gl_Position = projection * view * model * vec4(aPos, 1.0f);

	// 在世界坐标系中的位置（用于计算光照等）
	WorldPos = vec3(model * vec4(aPos, 1.0));
    
	// UV坐标
	TexCoords = aTexCoords;
	// 法线、切线、副切线
	Normal = normalMatrix * aNormal;
	Tangent = normalMatrix * aTangent;
	Bitangent = normalMatrix * aBitangent;

	// 在光源为视点的投影空间中的坐标（直射光源和点光源）
	FragPosDirectLightSpace = directlightSpaceMatrix * model * vec4(aPos, 1.0);
	FragPosLightSpace = lightSpaceMatrix * model * vec4(aPos, 1.0); 
}