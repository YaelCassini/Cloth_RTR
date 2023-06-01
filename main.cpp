#define _CRT_SECURE_NO_WARNINGS
#define _APPLE_

// 头文件引用
#include <glad/glad.h>
#include <glfw/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
using namespace std;


#include "Headers/render_text.h"
#include "Headers/shader.h"
#include "Headers/filesystem.h"
#include "Headers/camera.h"
#include "Headers/model.h"


// 函数声明
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processMovement(GLFWwindow* window);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void configureFBO(GLuint* FBO, vector<GLuint*>* textures, bool multisample, bool mipmap, bool depthOrStencil);
void inputTexture(string dir, vector<GLuint>* textures);
void RenderModelWithTextures(int index, Shader& renderShader);
void RenderModelsWithTextures(Shader& renderShader);
void RenderModels(Shader& renderShader);

// 全局变量
const unsigned int SCR_WIDTH = 800*1.5,  SCR_HEIGHT = 600*1.5;
const unsigned int SHADOW_WIDTH = 2048,  SHADOW_HEIGHT = 2048;
int numSamples = 4;
int activeShaderID = 1;

// 储存模型的List
bool renderWomanNow = true;
int modelsListIndex = 0;
vector<Model*> modelsList;
vector<vector<Model*>> modelsListsWoman;
vector<vector<Model*>> modelsListsMan;

// 储存贴图的List
vector<vector<GLuint> >  renderTextureListWoman;
vector<vector<GLuint> >  renderTextureListMan;
vector<vector<GLuint> >  renderTextureListNow;
//vector<vector<GLuint> >  renderTextureListWoman_test;

// 平行光的ShadowMap
GLuint depthMap_direct;
// 点光源的ShadowMap
GLuint depthMap;

map<GLchar, Character> Characters;

// 相机相关参数
// Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch)
Camera camera(0.0f, 5.0f, 10.0f, 0.0f, 1.0f, 0.0f, -90.0f, -20.0f);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// 时间
float currentFrame = 0.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 旋转
float rotationTime = 0.0f;
float directrotationTime = 0.0f;
bool rotationPaused = true;

// 平行光相关参数
glm::vec3 DIRECTLIGHTPOS = glm::vec3(-3.0f, 8.0f, 5.196152422f);
float directlightSourceFrame = -1.0471975f; // original position of light in its circular path
float directlightSourceVelocity = 2.0f;
float directlightPathRadius = 6.0f;
float directdirectionFlip = 1.0f;
bool directlightPaused = true;

// 点光源相关参数
glm::vec3 LIGHTPOS = glm::vec3(3.0f * 1.0f, 8.0f, 5.196152422f * 1.0f);
float lightSourceFrame = 1.0471975f; // original position of light in its circular path
float lightSourceVelocity = 2.0f;
float lightPathRadius = 6.0 * 1.0f;
float directionFlip = 1.0f;
bool lightPaused = true;

// 是否渲染阴影
int ifUseShadow = 1;



// 主函数
int main(int argc, char** argv)
{
	// GLFW初始化
	glfwInit();

	// tell GLFW to use OpenGL 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); //主版本号
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); //次版本号
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// MSAA 多重采样
	glfwWindowHint(GLFW_SAMPLES, numSamples);

	// MAC系统上需要设置
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// Create Window
	// ----------------------------------------------------------------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Cloth Project", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwSetWindowPos(window, 100, 100);

	// 声明回调函数
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);

	// tell GLFW to capture our mouse and keyboard
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

	// Initialize glad: load all OpenGL function pointers
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
	stbi_set_flip_vertically_on_load(true);

	// 配置OpenGL状态
	// ----------------------------------------------------------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	// 读入Shader代码并创建Shader Program
	// ----------------------------------------------------------------------------
	Shader phongShader("Shaders/default.vert", "Shaders/PhongLighting.frag");	
	Shader toonShader("Shaders/default.vert", "Shaders/ToonShading.frag");
	Shader lightSourceShader("Shaders/LightSource.vert", "Shaders/LightSource.frag");
	Shader renderTextShader("Shaders/RenderText.vert", "Shaders/RenderText.frag");
	Shader shadowMapShader("Shaders/Shadowmap.vert", "Shaders/Shadowmap.frag");
	Shader renderTextureShader("Shaders/RenderTexture.vert", "Shaders/RenderTexture.frag");
	Shader PBRShader("Shaders/PBRShader.vert", "Shaders/PBRShader.frag");

	// 读入模型并分别建立两个人物两个姿势的ModelList
	// ----------------------------------------------------------------------------
	Model backgroundModel("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/background.obj");
	
	Model bodyModel1_1("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/body1_1.obj");
	Model clothModel1_1("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth1_1.obj");
	Model clothModel2_1("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth2_1.obj");
	Model clothModel3_1("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth3_1.obj");

	Model bodyModel1_2("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/body1_2.obj");
	Model clothModel1_2("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth1_2.obj");
	Model clothModel2_2("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth2_2.obj");
	Model clothModel3_2("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth3_2.obj");

	Model bodyModel2_1("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/body2_1.obj");
	Model clothModel4_1("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth4_1.obj");
	Model clothModel5_1("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth5_1.obj");
	Model clothModel6_1("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth6_1.obj");

	Model bodyModel2_2("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/body2_2.obj");
	Model clothModel4_2("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth4_2.obj");
	Model clothModel5_2("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth5_2.obj");
	Model clothModel6_2("D:/STUDY/GPU_Rendering/Cloth/Non-Photorealistic-GLSL-Shaders-master/Models/cloth6_2.obj");

	vector<Model*> modelsList01, modelsList02;
	vector<Model*> modelsList11, modelsList12;

	modelsList01.push_back(&backgroundModel);
	modelsList01.push_back(&bodyModel1_1);
	modelsList01.push_back(&clothModel1_1);
	modelsList01.push_back(&clothModel2_1);
	modelsList01.push_back(&clothModel3_1);

	modelsList02.push_back(&backgroundModel);
	modelsList02.push_back(&bodyModel1_2);
	modelsList02.push_back(&clothModel1_2);
	modelsList02.push_back(&clothModel2_2);
	modelsList02.push_back(&clothModel3_2);

	modelsList11.push_back(&backgroundModel);
	modelsList11.push_back(&bodyModel2_1);
	modelsList11.push_back(&clothModel4_1);
	modelsList11.push_back(&clothModel5_1);
	modelsList11.push_back(&clothModel6_1);

	modelsList12.push_back(&backgroundModel);
	modelsList12.push_back(&bodyModel2_2);
	modelsList12.push_back(&clothModel4_2);
	modelsList12.push_back(&clothModel5_2);
	modelsList12.push_back(&clothModel6_2);

	modelsListsWoman.push_back(modelsList01);
	modelsListsWoman.push_back(modelsList02);
	modelsListsMan.push_back(modelsList11);
	modelsListsMan.push_back(modelsList12);
	modelsList = modelsListsWoman[0];



	// 读入两个人物的贴图并建立List
	// ----------------------------------------------------------------------------
	vector<GLuint> bgrRenderTargets(5);
	inputTexture("background_", &bgrRenderTargets);

	vector<GLuint> body1RenderTargets(5);
	inputTexture("set1/body0_", &body1RenderTargets);

	vector<GLuint> cloth1RenderTargets(5);
	inputTexture("set1/cloth1_", &cloth1RenderTargets);

	vector<GLuint> cloth2RenderTargets(5);
	inputTexture("set1/cloth2_", &cloth2RenderTargets);

	vector<GLuint> cloth3RenderTargets(5);
	inputTexture("set1/cloth3_", &cloth3RenderTargets);
	
	vector<GLuint> cloth4RenderTargets(5);
	inputTexture("set2/cloth4_", &cloth4RenderTargets);

	vector<GLuint> cloth5RenderTargets(5);
	inputTexture("set2/cloth5_", &cloth5RenderTargets);

	vector<GLuint> cloth6RenderTargets(5);
	inputTexture("set2/cloth6_", &cloth6RenderTargets);

	renderTextureListWoman.push_back(bgrRenderTargets);
	renderTextureListWoman.push_back(body1RenderTargets);
	renderTextureListWoman.push_back(cloth1RenderTargets);
	renderTextureListWoman.push_back(cloth2RenderTargets);
	renderTextureListWoman.push_back(cloth3RenderTargets);

	renderTextureListMan.push_back(bgrRenderTargets);
	renderTextureListMan.push_back(body1RenderTargets);
	renderTextureListMan.push_back(cloth4RenderTargets);
	renderTextureListMan.push_back(cloth5RenderTargets);
	renderTextureListMan.push_back(cloth6RenderTargets);


	// 光源Cube
	float lightSourceVertices[] = {
		-0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,
		-0.5f,  0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f, -0.5f,  0.5f,
		 0.5f, -0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,
		-0.5f, -0.5f,  0.5f,

		-0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,

		 0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,

		-0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f,  0.5f,
		 0.5f, -0.5f,  0.5f,
		-0.5f, -0.5f,  0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f, -0.5f,
	};


	// 光源Cube对应的VAO和VBO
	// ---------------------------------
	GLuint lightSourceVBO, lightSourceVAO;
	glGenVertexArrays(1, &lightSourceVAO);
	glGenBuffers(1, &lightSourceVBO);
	glBindVertexArray(lightSourceVAO);
	// load data into vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, lightSourceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(lightSourceVertices), lightSourceVertices, GL_STATIC_DRAW);
	// set the vertex attribute pointers
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glBindVertexArray(0);


	// configure uniform buffer objects for each shader program
	// 配置Uniform buffer objects
	// ---------------------------------
	// first. We get the relevant block indices
	GLuint uniformBlockIndexPhong = glGetUniformBlockIndex(phongShader.ID, "Matrices");
	GLuint uniformBlockIndexToon = glGetUniformBlockIndex(toonShader.ID, "Matrices");
	GLuint uniformBlockIndexLightSource = glGetUniformBlockIndex(lightSourceShader.ID, "Matrices");
	// then we link each shader's uniform block to this uniform binding point
	glUniformBlockBinding(phongShader.ID, uniformBlockIndexPhong, 0);
	glUniformBlockBinding(toonShader.ID, uniformBlockIndexToon, 0);
	glUniformBlockBinding(lightSourceShader.ID, uniformBlockIndexLightSource, 0);
	// Now actually create the buffer
	GLuint uboMatrices;
	glGenBuffers(1, &uboMatrices);
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	// define the range of the buffer that links to a uniform binding point
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));



	// 平行光的ShadowMap
	// GLuint depthMap_direct;
	glGenTextures(1, &depthMap_direct);
	glBindTexture(GL_TEXTURE_2D, depthMap_direct);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);


	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, 0);


	GLuint depthMapFBO_direct;
	glGenFramebuffers(1, &depthMapFBO_direct);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_direct);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap_direct, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	
	// 点光源的ShadowMap
	// GLuint depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);


	// No need to force GL_DEPTH_COMPONENT24, drivers usually give you the max precision if available 
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, 0);


	GLuint depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// 一个铺满屏幕的矩形的顶点坐标及纹理坐标，用于显示ShadowMap
	float quadVertices[] = {
		// 顶点坐标             // 纹理坐标
		-1.0f,  1.0f, 0.0f,     0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f,     0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,     1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f,     1.0f, 0.0f,
	};

	// 能够铺满屏幕的VAO和VBO
	GLuint quadVAO, quadVBO;
	glGenVertexArrays(1, &quadVAO);
	glBindVertexArray(quadVAO);

	glGenBuffers(1, &quadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);

	// 将数据从数组存入VBO中
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

	// 将数据存入VAO
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));


	// set up FreeType
	// 	// -----------------------------------
	int returnVal = SetUpFreeType(&Characters);
	if (returnVal != 0)
	{
		return returnVal;
	}
	// configure VAO/VBO for texture quads
	GLuint textVAO, textVBO;
	glGenVertexArrays(1, &textVAO);
	glGenBuffers(1, &textVBO);
	glBindVertexArray(textVAO);
	glBindBuffer(GL_ARRAY_BUFFER, textVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glm::mat4 textProjection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));


	// shader configuration
	renderTextureShader.use();
	renderTextureShader.setInt("ourTexture", 0);

	PBRShader.use();
	PBRShader.setInt("albedoMap", 0);
	PBRShader.setInt("normalMap", 1);
	PBRShader.setInt("metallicMap", 2);
	PBRShader.setInt("roughnessMap", 3);
	PBRShader.setInt("aoMap", 4);
	PBRShader.setInt("shadowMap_direct", 5);
	PBRShader.setInt("shadowMap", 6);


	float fpsTimer = glfwGetTime();
	int numFrames = 0, numFramesToDisplay = 0;

	glm::vec3 lightPos = LIGHTPOS;
	glm::vec3 directlightPos = DIRECTLIGHTPOS;

	// 绘制
	while (!glfwWindowShouldClose(window))
	{
		// 选择要绘制的模型和使用的贴图
		if (renderWomanNow)
		{
			renderTextureListNow = renderTextureListWoman;
			modelsList = modelsListsWoman[modelsListIndex];
		}
		else
		{
			renderTextureListNow = renderTextureListMan;
			modelsList = modelsListsMan[modelsListIndex];
		}

		// per-frame time logic
		// --------------------
		currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		processMovement(window);

		// 清空屏幕
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// set the view matrix in the uniform block - we only have to do this once per loop iteration.
		// 设置view变换矩阵
		// -----------------------------------
		glm::mat4 view = camera.GetViewMatrix();
		glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// store the projection matrix
		// 设置projection变换矩阵
		// -----------------------------------
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
		glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// world transformation
		// scale then rotation then translation -> T * R * S
		// 设置模型空间到世界空间的变换矩阵
		// -----------------------------------
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::scale(modelMatrix, glm::vec3(0.8f));
		// modelMatrix = glm::rotate(modelMatrix, -90.0f, glm::vec3(1.0f, 0.0f, 0.0f));
		modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, 2.0f, 0.0f));

		// 设置法线变换矩阵
		glm::mat3 normalMatrix = glm::mat3(transpose(inverse(modelMatrix)));


		// 根据当前的activeShaderID决定绘制方式
		// -----------------------------------
		switch (activeShaderID)
		{
		// 正常绘制
		case 1:
		{
			// 平行光
			// 绘制平行光下的ShadowMap到其对应的FBO
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_direct);
			glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
			glClear(GL_DEPTH_BUFFER_BIT);
			// Disable color rendering, we only want to write to the Z-Buffer
			// 不写入颜色
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			// Culling switching, rendering only backface, this is done to avoid self-shadowing
			// 只绘制背面，为了防止自阴影
			glCullFace(GL_FRONT);

			//从光源位置的正交投影矩阵
			float near_plane = -0.0f, far_plane = 60.0f;
			glm::mat4 lightOrthoProjection = glm::ortho(-20.0f, 20.0f, -10.0f, 30.0f, near_plane, far_plane);

			//从光源位置的观察矩阵
			glm::mat4 directlightView = glm::lookAt(directlightPos, glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			//组合成光源空间的变换矩阵
			glm::mat4 directlightSpaceMatrix = lightOrthoProjection * directlightView;

			//将矩阵传给着色器并设置着色器Uniform变量
			shadowMapShader.use();
			shadowMapShader.setMat4("model", modelMatrix);
			shadowMapShader.setMat4("lightSpaceMatrix", directlightSpaceMatrix);

			// 绘制模型
			//modelsList[0]->Draw(shadowMapShader);
			//modelsList[1]->Draw(shadowMapShader);
			//modelsList[2]->Draw(shadowMapShader);
			//modelsList[3]->Draw(shadowMapShader);
			//modelsList[4]->Draw(shadowMapShader);
			RenderModels(shadowMapShader);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);



			// 点光源
			// 绘制平行光下的ShadowMap到其对应的FBO
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
			glClear(GL_DEPTH_BUFFER_BIT);
			//Disable color rendering, we only want to write to the Z-Buffer
			// 不写入颜色
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			// Culling switching, rendering only backface, this is done to avoid self-shadowing
			// 只绘制背面，为了防止自阴影
			glCullFace(GL_FRONT);

			//从光源位置的正交投影矩阵（是否应该选择透视投影）
			// glm::mat4 lightPersPoojection = glm::perspective(glm::radians(90.0f), 1.0f, 0.3f, 10000.0f);
			glm::mat4 lightPersPoojection = glm::ortho(-20.0f, 20.0f, -10.0f, 30.0f, near_plane, far_plane);

			//从光源位置的观察矩阵
			glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			//组合成光源空间的变换矩阵
			glm::mat4 lightSpaceMatrix = lightPersPoojection * lightView;

			//将矩阵传给着色器并设置着色器Uniform变量
			shadowMapShader.use();
			shadowMapShader.setMat4("model", modelMatrix);
			shadowMapShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

			// 绘制模型
			//modelsList[0]->Draw(shadowMapShader);
			//modelsList[1]->Draw(shadowMapShader);
			//modelsList[2]->Draw(shadowMapShader);
			//modelsList[3]->Draw(shadowMapShader);
			//modelsList[4]->Draw(shadowMapShader);
			RenderModels(shadowMapShader);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);




			// 正常绘制Pass
			glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			PBRShader.use();
			// set uniforms
			PBRShader.setMat4("model", modelMatrix);
			PBRShader.setMat4("projection", projection);
			PBRShader.setMat4("view", view);
			PBRShader.setMat3("normalMatrix", normalMatrix);
			PBRShader.setVec3("viewPos", camera.Position);

			PBRShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
			PBRShader.setVec3("lightPos", lightPos);
			PBRShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);

			PBRShader.setMat4("directlightSpaceMatrix", directlightSpaceMatrix);
			PBRShader.setVec3("directionLightDir", glm::vec3(0.0, -2.0, 0.0) - directlightPos);
			PBRShader.setVec3("directionLightColor", 1.0f, 1.0f, 1.0f);

			PBRShader.setInt("ifUseShadow", ifUseShadow);
			PBRShader.setFloat("xPixelOffset", 1.0 / SHADOW_WIDTH);
			PBRShader.setFloat("yPixelOffset", 1.0 / SHADOW_HEIGHT);

			// 只绘制模型正面
			glCullFace(GL_BACK);

			RenderModelsWithTextures(PBRShader);

			break;
		}
		// 绘制平行光下的ShadowMap
		case 2:
		{
			// 平行光
			// 绘制平行光下的ShadowMap到其对应的FBO
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_direct);
			glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
			glClear(GL_DEPTH_BUFFER_BIT);
			// Disable color rendering, we only want to write to the Z-Buffer
			// 不写入颜色
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			// Culling switching, rendering only backface, this is done to avoid self-shadowing
			// 只绘制背面，为了防止自阴影
			glCullFace(GL_FRONT);

			//从光源位置的正交投影矩阵
			float near_plane = -0.0f, far_plane = 60.0f;
			glm::mat4 lightOrthoProjection = glm::ortho(-20.0f, 20.0f, -10.0f, 30.0f, near_plane, far_plane);

			//从光源位置的观察矩阵
			glm::mat4 directlightView = glm::lookAt(directlightPos, glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			//组合成光源空间的变换矩阵
			glm::mat4 directlightSpaceMatrix = lightOrthoProjection * directlightView;

			//将矩阵传给着色器并设置着色器Uniform变量
			shadowMapShader.use();
			shadowMapShader.setMat4("model", modelMatrix);
			shadowMapShader.setMat4("lightSpaceMatrix", directlightSpaceMatrix);

			// 绘制模型
			//modelsList[0]->Draw(shadowMapShader);
			//modelsList[1]->Draw(shadowMapShader);
			//modelsList[2]->Draw(shadowMapShader);
			//modelsList[3]->Draw(shadowMapShader);
			//modelsList[4]->Draw(shadowMapShader);
			RenderModels(shadowMapShader);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);



			// 绘制一个铺满屏幕的正方形，展示ShadowMap
			glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			// 清屏
			glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

			// 选择着色器并传入参数
			renderTextureShader.use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthMap_direct); //平铺到整个屏幕的纹理图像
			// 绑定VAO
			glBindVertexArray(quadVAO);
			// 绘制
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		// 绘制点光源下的ShadowMap
		case 3:
		{
			// 点光源
			// 绘制平行光下的ShadowMap到其对应的FBO
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
			glClear(GL_DEPTH_BUFFER_BIT);
			//Disable color rendering, we only want to write to the Z-Buffer
			// 不写入颜色
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			// Culling switching, rendering only backface, this is done to avoid self-shadowing
			// 只绘制背面，为了防止自阴影
			glCullFace(GL_FRONT);

			//从光源位置的正交投影矩阵（是否应该选择透视投影）
			float near_plane = -0.0f, far_plane = 60.0f;
			// glm::mat4 lightPersPoojection = glm::perspective(glm::radians(90.0f), 1.0f, 0.3f, 10000.0f);
			glm::mat4 lightPersPoojection = glm::ortho(-20.0f, 20.0f, -10.0f, 30.0f, near_plane, far_plane);

			//从光源位置的观察矩阵
			glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			//组合成光源空间的变换矩阵
			glm::mat4 lightSpaceMatrix = lightPersPoojection * lightView;

			//将矩阵传给着色器并设置着色器Uniform变量
			shadowMapShader.use();
			shadowMapShader.setMat4("model", modelMatrix);
			shadowMapShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

			// 绘制模型
			//modelsList[0]->Draw(shadowMapShader);
			//modelsList[1]->Draw(shadowMapShader);
			//modelsList[2]->Draw(shadowMapShader);
			//modelsList[3]->Draw(shadowMapShader);
			//modelsList[4]->Draw(shadowMapShader);
			RenderModels(shadowMapShader);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);



			// 绘制一个铺满屏幕的正方形，展示ShadowMap
			glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			// 清屏
			glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

			// 选择着色器并传入参数
			renderTextureShader.use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthMap); //平铺到整个屏幕的纹理图像
			// 绑定VAO
			glBindVertexArray(quadVAO);
			// 绘制
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		}
		case 4: // toon shader
		{
			toonShader.use();
			// set uniforms
			toonShader.setMat4("model", modelMatrix);
			toonShader.setMat3("normalMatrix", normalMatrix);
			toonShader.setVec3("lightPos", lightPos);
			toonShader.setVec3("viewPos", camera.Position);
			toonShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
			toonShader.setVec3("objectColor", 1.0f, 0.5f, 0.5f);

			// render model
			//modelsList[0]->Draw(phongShader);
			//modelsList[1]->Draw(phongShader);
			//modelsList[2]->Draw(phongShader);
			//modelsList[3]->Draw(phongShader);
			//modelsList[4]->Draw(phongShader);
			RenderModels(shadowMapShader);
			break;
		}
		
		default: // Phong shader
		{
			phongShader.use();
			// set uniforms
			phongShader.setMat4("model", modelMatrix);
			phongShader.setMat3("normalMatrix", normalMatrix);
			phongShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
			phongShader.setVec3("lightPos", lightPos);
			phongShader.setVec3("objectColor", 1.0f, 0.5f, 0.5f);
			phongShader.setVec3("viewPos", camera.Position);

			// render model
			//modelsList[0]->Draw(phongShader);
			//modelsList[1]->Draw(phongShader);
			//modelsList[2]->Draw(phongShader);
			//modelsList[3]->Draw(phongShader);
			//modelsList[4]->Draw(phongShader);
			RenderModels(shadowMapShader);
			break;
		}

		}


		// 旋转平行光
		if (!directlightPaused)
		{
			directlightSourceFrame = directlightSourceFrame + deltaTime * directdirectionFlip;
			directlightPos.x = sin(directlightSourceFrame / directlightSourceVelocity) * directlightPathRadius;
			directlightPos.z = cos(directlightSourceFrame / directlightSourceVelocity) * directlightPathRadius;
		}
		// 旋转点光源
		if (!lightPaused)
		{
			lightSourceFrame = lightSourceFrame + deltaTime * directionFlip;
			lightPos.x = sin(lightSourceFrame / lightSourceVelocity) * lightPathRadius;
			lightPos.z = cos(lightSourceFrame / lightSourceVelocity) * lightPathRadius;
		}
		// 绘制光源Cude（点光源）
		// -----------------------------------
		lightSourceShader.use();
		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, lightPos);
		modelMatrix = glm::scale(modelMatrix, glm::vec3(0.2f)); // a smaller cube
		lightSourceShader.setMat4("model", modelMatrix);
		glBindVertexArray(lightSourceVAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);


		// render the FPS
		// -----------------------------------
		renderTextShader.use();
		renderTextShader.setMat4("projection", textProjection);
		numFrames++;
		if (currentFrame - fpsTimer >= 1.0f)
		{
			numFramesToDisplay = numFrames;
			numFrames = 0;
			fpsTimer += 1.0f;
		}
		// print FPS on screen
		std::string fpsText = std::string("FPS: ") + std::to_string(numFramesToDisplay);
		RenderText(renderTextShader, fpsText.c_str(), Characters, textVAO, textVBO, 25.0f, 25.0f, 1.0f, glm::vec3(1.0, 0.0f, 0.0f));

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	// deallocate all resources
	// -----------------------------------
	// drivers automatically handle releasing texture storage
	glDeleteVertexArrays(1, &lightSourceVAO);
	glDeleteVertexArrays(1, &quadVAO);
	glDeleteVertexArrays(1, &textVAO);

	glDeleteBuffers(1, &lightSourceVBO);
	glDeleteBuffers(1, &quadVBO);
	glDeleteBuffers(1, &textVBO);
	glDeleteBuffers(1, &uboMatrices);

	// glfw: terminate, clearing all previously allocated GLFW resources.
	glfwTerminate();
	return 0;
}

// 绘制模型（一组）
void RenderModels(Shader& renderShader)
{
	for (int i = 0; i < 5; i++)
	{
		modelsList[i]->Draw(renderShader);
	}
}

// 绘制带贴图的模型（一组）
void RenderModelsWithTextures(Shader& renderShader)
{
	for (int i = 0; i < 5; i++)
	{
		RenderModelWithTextures(i, renderShader);
	}
}

// 绘制一个带贴图的模型
void RenderModelWithTextures(int index, Shader& renderShader)
{
	// activate textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, (renderTextureListNow[index])[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, (renderTextureListNow[index])[1]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, (renderTextureListNow[index])[2]);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, (renderTextureListNow[index])[3]);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, (renderTextureListNow[index])[4]);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, depthMap_direct); // dark
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	// 绘制模型
	modelsList[index]->Draw(renderShader);
}

// resize window callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// 相机移动
void processMovement(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		camera.ProcessKeyboard(UP, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		camera.ProcessKeyboard(DOWN, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
		camera.ResetCamera();
}

// 键盘回调
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);


	// light movement 平行光
	if (key == GLFW_KEY_O && action == GLFW_PRESS){
		directlightPaused = !directlightPaused;
	}
	if (key == GLFW_KEY_P && action == GLFW_PRESS){
		directlightPaused = false;
		directdirectionFlip = 1.0f;
	}
	if (key == GLFW_KEY_I && action == GLFW_PRESS){
		directlightPaused = false;
		directdirectionFlip = -1.0f;
	}
	if ((key == GLFW_KEY_P || key == GLFW_KEY_I) && action == GLFW_RELEASE){
		directlightPaused = true;
	}

	// light movement 点光源
	if (key == GLFW_KEY_K && action == GLFW_PRESS) {
		lightPaused = !lightPaused;
	}
	if (key == GLFW_KEY_L && action == GLFW_PRESS) {
		lightPaused = false;
		directionFlip = 1.0f;
	}
	if (key == GLFW_KEY_J && action == GLFW_PRESS) {
		lightPaused = false;
		directionFlip = -1.0f;
	}
	if ((key == GLFW_KEY_L || key == GLFW_KEY_J) && action == GLFW_RELEASE) {
		lightPaused = true;
	}


	// 切换两个人物的渲染
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
	{
		renderWomanNow = !renderWomanNow;
	}

	/*if ((key == GLFW_KEY_RIGHT || key == GLFW_KEY_LEFT) && action == GLFW_RELEASE)
	{
		lightPaused = true;
	}*/

	// 切换渲染方式
	if (key == GLFW_KEY_0 && action == GLFW_PRESS) // Gooch shading
	{
		if (ifUseShadow == 1) ifUseShadow = 0;
		else ifUseShadow = 1;
	}
	// shaders
	if (key == GLFW_KEY_1 && action == GLFW_PRESS) // toon shading
	{
		activeShaderID = 1;
		directlightPaused = true;
		lightPaused = true;
	}
	if (key == GLFW_KEY_2 && action == GLFW_PRESS) // Gooch shading
	{
		activeShaderID = 2;
		directlightPaused = true;
		lightPaused = true;
	}
	if (key == GLFW_KEY_3 && action == GLFW_PRESS) // hatching
	{
		activeShaderID = 3;
		directlightPaused = true;
		lightPaused = true;
	}
	if (key == GLFW_KEY_4 && action == GLFW_PRESS) // Phong shading
	{
		activeShaderID = 4;
		directlightPaused = true;
		lightPaused = true;
	}

	if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
	{
		modelsListIndex = (modelsListIndex + 1)%2;
	}
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	//std::cout << "we got here" << std::endl;
	camera.ProcessMouseScroll(yoffset);
}


// 读入贴图并绑定
void inputTexture(string dir, vector<GLuint>* textures)
{
	// get default textures
	int width, height, nrChannels;

	// generate texture buffers
	for (int i = 0; i < (*textures).size(); i++)
	{
		glGenTextures(1, &(*textures)[i]);
		glBindTexture(GL_TEXTURE_2D, (*textures)[i]);

		std::string filename = std::string("Textures/") + dir + std::to_string(i) + std::string(".png");
		unsigned char* tex = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, tex);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // trilinear filtering
		stbi_image_free(tex);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glBindTexture(GL_TEXTURE_2D, 0);
	}
}


// Bind textures and buffers to frame buffer object
// ----------------------------------------------------------------------
void configureFBO(GLuint* FBO, vector<GLuint*>* textures, bool multisample, bool mipmap, bool depthOrStencil) {

	glGenFramebuffers(1, FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, *FBO);

	// get default textures
	int width, height, nrChannels;

	// generate texture buffers
	for (int i = 0; i < (*textures).size(); i++)
	{
		if (multisample)
		{
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, *(*textures)[i]);

			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_RGB, SCR_WIDTH, SCR_HEIGHT, GL_TRUE);

			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D_MULTISAMPLE, *(*textures)[i], 0);

			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, *(*textures)[i]);

			if (mipmap) // create mipmaps for hatching textures
			{
				std::string filename = std::string("Textures/Hatch/hatch_") + std::to_string(i) + std::string(".jpg");
				unsigned char* tex = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, tex);
				glGenerateMipmap(GL_TEXTURE_2D);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // trilinear filtering
				stbi_image_free(tex);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, *(*textures)[i], 0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	if (depthOrStencil)
	{
		// create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
		GLuint rbo;
		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);

		// use a single renderbuffer object for both a depth AND stencil buffer
		if (multisample)
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, numSamples, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
		else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it
	}

	// now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}