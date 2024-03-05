#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "EW/Shader.h"
#include "EW/EwMath.h"
#include "EW/Camera.h"
#include "EW/Mesh.h"
#include "EW/Transform.h"
#include "EW/ShapeGen.h"

void processInput(GLFWwindow* window);
void resizeFrameBufferCallback(GLFWwindow* window, int width, int height);
void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void mousePosCallback(GLFWwindow* window, double xpos, double ypos);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
GLuint createTexture(const char* filePath);
void drawScene(Shader& aShader);

float lastFrameTime;
float deltaTime;

int SCREEN_WIDTH = 1080;
int SCREEN_HEIGHT = 720;

double prevMouseX;
double prevMouseY;
bool firstMouseInput = false;

/* Button to lock / unlock mouse
* 1 = right, 2 = middle
* Mouse will start locked. Unlock it to use UI
* */
const int MOUSE_TOGGLE_BUTTON = 1;
const float MOUSE_SENSITIVITY = 0.1f;
const float CAMERA_MOVE_SPEED = 5.0f;
const float CAMERA_ZOOM_SPEED = 3.0f;

Camera camera((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT);

bool wireFrame = false;
const int MAX_LIGHTS = 8;

glm::vec3 bgColor = glm::vec3(0);
glm::vec3 pointLightColors[MAX_LIGHTS];
glm::vec3 spotLightColors[MAX_LIGHTS];

struct PointLight {
	float radius;
	glm::vec3 position;
	glm::vec3 color;
	float intensity;
	int isOn;
};
PointLight pointLights[MAX_LIGHTS];

struct DirectionLight {
	glm::vec3 direction;
	float intensity;
	glm::vec3 color;
	int isOn;
};
DirectionLight dirLight[MAX_LIGHTS];

struct SpotLight {
	float radius;
	glm::vec3 direction;
	float intensity;
	glm::vec3 color;
	glm::vec3 position;
	float minAngle;
	float maxAngle;
	int isOn;
};
SpotLight spotLight[MAX_LIGHTS];

struct Material {
	glm::vec3 color;
	float ambientK;
	float diffuseK;
	float specularK;
	float shininess;
};
Material material;

//Meshes and Transforms
ew::Mesh cubeMesh;
ew::Mesh sphereMesh;
ew::Mesh planeMesh;
ew::Mesh cylinderMesh;
ew::Mesh quadMesh;
ew::Mesh fullscreenQuadMesh;

ew::Transform cubeTransform[2];
ew::Transform sphereTransform[2];
ew::Transform planeTransform[2];
ew::Transform cylinderTransform[2];
ew::Transform quadTransform[4];

bool isRotating = false;
float rotationAngle = 0.01;

//Rotation Matrices
glm::mat3 rotationX(float angle) {
	return glm::mat3(
		1, 0, 0,
		0, cos(angle), sin(angle),
		0, -sin(angle), cos(angle)
	);
}

glm::mat3 rotationY(float angle) {
	return glm::mat3(
		cos(angle), 0, -sin(angle),
		0, 1, 0,
		sin(angle), 0, cos(angle)
	);
}

glm::mat3 rotationZ(float angle) {
	return glm::mat3(
		cos(angle), sin(angle), 0,
		-sin(angle), cos(angle), 0,
		0, 0, 1
	);
}

glm::mat3 rotationMatrix(glm::vec3 angles) {
	return (rotationX(angles.x) * rotationY(angles.y) * rotationZ(angles.z));
}

int main() {
	if (!glfwInit()) {
		printf("glfw failed to init");
		return 1;
	}

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Lighting", 0, 0);
	glfwMakeContextCurrent(window);

	if (glewInit() != GLEW_OK) {
		printf("glew failed to init");
		return 1;
	}

	glfwSetFramebufferSizeCallback(window, resizeFrameBufferCallback);
	glfwSetKeyCallback(window, keyboardCallback);
	glfwSetScrollCallback(window, mouseScrollCallback);
	glfwSetCursorPosCallback(window, mousePosCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);

	//Hide cursor
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Setup UI Platform/Renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	//Dark UI theme.
	ImGui::StyleColorsDark();

	//Used to draw shapes. This is the shader you will be completing.
	Shader litShader("shaders/defaultLit.vert", "shaders/defaultLit.frag");

	//Used to draw light sphere
	Shader unlitShader("shaders/defaultLit.vert", "shaders/unlit.frag");

	//Used for shadow mapping
	Shader depthShader("shaders/depthShader.vert", "shaders/depthShader.geom", "shaders/depthShader.frag");

	// Setup Textures
	GLuint floorTexture = createTexture("Textures/MetalPlates017A_2K_Color.png");
	GLuint objectTexture = createTexture("Textures/Tiles084_2K_Color.png");
	GLuint objectNormalTexture = createTexture("Textures/Tiles084_2K_NormalGL.png");
	float normalIntensity = 1.0;

	//Depth Frame Buffer
	GLuint depthTexture;
	glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 2048, 2048, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glGenerateMipmap(GL_TEXTURE_2D);

	GLuint depthfbo;
	glGenFramebuffers(1, &depthfbo);
	glBindFramebuffer(GL_FRAMEBUFFER, depthfbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("Error loading Depth Buffer FBO");

	//Point light shadow map
	GLuint depthCubemap;
	glGenTextures(1, &depthCubemap);

	const int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
	glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
	for (int i = 0; i < 6; i++)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	GLuint depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("Error loading Depth Buffer FBO");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//Create data for shapes
	ew::MeshData cubeMeshData;
	ew::createCube(1.0f, 1.0f, 1.0f, cubeMeshData);
	ew::MeshData sphereMeshData;
	ew::createSphere(0.5f, 64, sphereMeshData);
	ew::MeshData cylinderMeshData;
	ew::createCylinder(1.0f, 0.5f, 64, cylinderMeshData);
	ew::MeshData planeMeshData;
	ew::createPlane(1.0f, 1.0f, planeMeshData);
	ew::MeshData quadMeshData;
	ew::createQuad(1.0f, 1.0f, quadMeshData);

	cubeMesh.Load(&cubeMeshData);
	sphereMesh.Load(&sphereMeshData);
	planeMesh.Load(&planeMeshData);
	cylinderMesh.Load(&cylinderMeshData);
	quadMesh.Load(&quadMeshData);

	//Enable back face culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	//Initialize shape transforms
	ew::Transform lightTransformPoint[MAX_LIGHTS];
	ew::Transform lightTransformSpot[MAX_LIGHTS];

	cubeTransform[0].position = glm::vec3(-3.0f, 0.0f, 0.0f);
	cubeTransform[1].position = glm::vec3(0.0f, 3.0f, 0.0f);

	sphereTransform[0].position = glm::vec3(0.0f, 0.0f, 3.0f);
	sphereTransform[1].position = glm::vec3(0.0f, -3.0f, 0.0f);
	
	planeTransform[0].position = glm::vec3(0.0f, -7.0f, 0.0f);
	planeTransform[0].scale = glm::vec3(15.0f);

	planeTransform[1].position = glm::vec3(0.0f, 7.0f, 0.0f);
	planeTransform[1].scale = glm::vec3(15.0f);
	planeTransform[1].rotation = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);
	
	cylinderTransform[0].position = glm::vec3(3.0f, 0.0f, 0.0f);
	cylinderTransform[1].position = glm::vec3(0.0f, 0.0f, -3.0f);
	
	quadTransform[0].position = glm::vec3(0.0f, 0.0f, -7.0f);
	quadTransform[0].scale = glm::vec3(15.0f);

	quadTransform[1].position = glm::vec3(0.0f, 0.0f, 7.0f);
	quadTransform[1].rotation = glm::vec3(0.0f, glm::radians(180.0f), 0.0f);
	quadTransform[1].scale = glm::vec3(15.0f);

	quadTransform[2].position = glm::vec3(-7.0f, 0.0f, 0.0f);
	quadTransform[2].rotation = glm::vec3(0.0f, glm::radians(270.0f), 0.0f);
	quadTransform[2].scale = glm::vec3(15.0f);

	quadTransform[3].position = glm::vec3(7.0f, 0.0f, 0.0f);
	quadTransform[3].rotation = glm::vec3(0.0f, glm::radians(90.0f), 0.0f);
	quadTransform[3].scale = glm::vec3(15.0f);

	//Material Set up
	material.color = glm::vec3(1.0f, 1.0f, 1.0f);
	material.ambientK = 0.2;
	material.diffuseK = 0.7;
	material.specularK = 0.1;
	material.shininess = 64.0;

	//Point Light Set Up
	lightTransformPoint[0].scale = glm::vec3(0.5f);
	lightTransformPoint[0].position = glm::vec3(0.0f, 0.0f, 0.0f);

	pointLights[0].radius = 15.0;
	pointLights[0].position = lightTransformPoint[0].position;
	pointLights[0].color = glm::vec3(1.0, 1.0, 1.0);
	pointLights[0].intensity = 1.0;
	pointLights[0].isOn = 1;

	//Directional Light Set Up
	dirLight[0].color = glm::vec3(1);
	dirLight[0].intensity = 1.0;
	dirLight[0].direction = glm::vec3(1.0, -1.0, 0.0);
	dirLight[0].isOn = 0;

	//Spot Light Set Up
	lightTransformSpot[0].scale = glm::vec3(0.5f);
	lightTransformSpot[0].position = glm::vec3(0.0f, 5.0f, 0.0f);

	spotLight[0].radius = 8.0;
	spotLight[0].direction = glm::vec3(0.0, -1.0, 0.0);
	spotLight[0].intensity = 1.0;
	spotLight[0].color = glm::vec3(1);
	spotLight[0].position = lightTransformSpot[0].position;
	spotLight[0].minAngle = 15.0;
	spotLight[0].maxAngle = 45.0;
	spotLight[0].isOn = 0;

	//Shadow Data setup
	float minBias = 0.005;
	float maxBias = 0.015;

	while (!glfwWindowShouldClose(window)) {
		litShader.use();

		processInput(window);
		glClearColor(bgColor.r,bgColor.g,bgColor.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		float time = (float)glfwGetTime();
		deltaTime = time - lastFrameTime;
		lastFrameTime = time;

		litShader.setFloat("_Time", time / 5);

		//Set Textures for Shader
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		
		litShader.setInt("_FloorTexture", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, objectTexture);

		litShader.setInt("_ObjectTexture", 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, objectNormalTexture);

		litShader.setInt("_ObjectNormalMap", 2);
		litShader.setFloat("_NormalIntensity", normalIntensity);

		//Update PointLight Positions
		for (int i = 0; i < MAX_LIGHTS; i++) {
			lightTransformPoint[i].position = pointLights[i].position;
		}

		//Update PointLight Colors
		for (int i = 0; i < MAX_LIGHTS; i++) {
			pointLightColors[i] = pointLights[i].color * (float)pointLights[i].isOn;
		}

		//Update object positions
		if (isRotating) {
			glm::vec3 toRotate = { glm::radians(0.0f), glm::radians(0.2f), glm::radians(0.0f) };
			cubeTransform[0].position = cubeTransform[0].position * rotationMatrix(toRotate);
			toRotate = { glm::radians(0.1f), glm::radians(0.0f), glm::radians(0.0f) };
			cubeTransform[1].position = cubeTransform[1].position * rotationMatrix(toRotate);

			toRotate = { glm::radians(0.0f), glm::radians(0.0f), glm::radians(0.15f) };
			cylinderTransform[0].position = cylinderTransform[0].position * rotationMatrix(toRotate);
			toRotate = { glm::radians(-0.25f), glm::radians(0.25f), glm::radians(0.0f) };
			cylinderTransform[1].position = cylinderTransform[1].position * rotationMatrix(toRotate);

			toRotate = { glm::radians(0.3f), glm::radians(0.3f), glm::radians(0.0f) };
			sphereTransform[0].position = sphereTransform[0].position * rotationMatrix(toRotate);
			toRotate = { glm::radians(0.1f), glm::radians(0.0f), glm::radians(0.0f) };
			sphereTransform[1].position = sphereTransform[1].position * rotationMatrix(toRotate);
		}

		//Set Material Uniforms
		litShader.setVec3("_Material.color", material.color);
		litShader.setFloat("_Material.ambientK", material.ambientK);
		litShader.setFloat("_Material.diffuseK", material.diffuseK);
		litShader.setFloat("_Material.specularK", material.specularK);
		litShader.setFloat("_Material.shininess", material.shininess);

		//Set Point Light Uniforms
		for (int i = 0; i < MAX_LIGHTS; i++) {
			litShader.setFloat("_PointLights[" + std::to_string(i) + "].radius", pointLights[i].radius);
			litShader.setVec3("_PointLights[" + std::to_string(i) + "].position", pointLights[i].position); //need +std::to_string(0)+ for for loop
			litShader.setFloat("_PointLights[" + std::to_string(i) + "].intensity", pointLights[i].intensity);
			litShader.setVec3("_PointLights[" + std::to_string(i) + "].color", pointLights[i].color);
			litShader.setInt("_PointLights[" + std::to_string(i) + "].isOn", pointLights[i].isOn);
		}

		//Set Directiona Light Uniforms
		for (int i = 0; i < MAX_LIGHTS; i++) {
			litShader.setVec3("_DirLight[" + std::to_string(i) + "].direction", dirLight[i].direction);
			litShader.setFloat("_DirLight[" + std::to_string(i) + "].intensity", dirLight[i].intensity);
			litShader.setVec3("_DirLight[" + std::to_string(i) + "].color", dirLight[i].color);
			litShader.setInt("_DirLight[" + std::to_string(i) + "].isOn", dirLight[i].isOn);
		}
		
		//Set Spot Light Uniforms
		for (int i = 0; i < MAX_LIGHTS; i++) {
			litShader.setFloat("_SpotLight[" + std::to_string(i) + "].radius", spotLight[i].radius);
			litShader.setVec3("_SpotLight[" + std::to_string(i) + "].direction", spotLight[i].direction);
			litShader.setFloat("_SpotLight[" + std::to_string(i) + "].intensity", spotLight[i].intensity);
			litShader.setVec3("_SpotLight[" + std::to_string(i) + "].color", spotLight[i].color);
			litShader.setVec3("_SpotLight[" + std::to_string(i) + "].position", spotLight[i].position);
			litShader.setFloat("_SpotLight[" + std::to_string(i) + "].minAngle", spotLight[i].minAngle);
			litShader.setFloat("_SpotLight[" + std::to_string(i) + "].maxAngle", spotLight[i].maxAngle);
			litShader.setInt("_SpotLight[" + std::to_string(i) + "].isOn", spotLight[i].isOn);
		}

		//Draw from Camera POV
		litShader.use();
		litShader.setVec3("camPos", camera.getPosition());

		litShader.setMat4("_Projection", camera.getProjectionMatrix());
		litShader.setMat4("_View", camera.getViewMatrix());

		litShader.setFloat("_MinBias", minBias);
		litShader.setFloat("_MaxBias", maxBias);

		//Point light shadows render
		float aspect = (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT;
		float near = 1.0f;
		float far = 25.0f;
		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspect, near, far);

		std::vector<glm::mat4> shadowTransforms;
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
		shadowTransforms.push_back(shadowProj *
			glm::lookAt(pointLights[0].position, pointLights[0].position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));

		//Dpeth Render
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glCullFace(GL_FRONT);
		depthShader.use();
		depthShader.setVec3("lightPos", pointLights[0].position);
		depthShader.setFloat("far_plane", far);
		for (int i = 0; i < 6; i++) {
			depthShader.setMat4("_ShadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
		}
		drawScene(depthShader);

		//Normal Render
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		litShader.use();
		glCullFace(GL_BACK);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
		litShader.setInt("_PointShadowMap", 4);
		litShader.setFloat("_FarPlane", far);
		litShader.setInt("_UseTexture2", false);
		drawScene(litShader);

		//Draw light as a small sphere using unlit shader, ironically.
		unlitShader.use();
		unlitShader.setMat4("_Projection", camera.getProjectionMatrix());
		unlitShader.setMat4("_View", camera.getViewMatrix());
		unlitShader.setMat4("_Model", lightTransformPoint[0].getModelMatrix());
		unlitShader.setVec3("_Color", pointLightColors[0]);
		sphereMesh.draw();
		
		ImGui::Begin("Point Lights");
		ImGui::ColorEdit3("Light Color", &pointLights[0].color.r);
		ImGui::DragFloat3("Light Position", &pointLights[0].position.x, 0.1f);
		ImGui::SliderFloat("Light Intensity", &pointLights[0].intensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Light Radius", &pointLights[0].radius, 0.0f, 15.0f);
		ImGui::SliderInt("Light On", &pointLights[0].isOn, 0, 1);
		ImGui::SliderFloat("Normal Intensity", &normalIntensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Min Bias", &minBias, 0.0f, 0.05);
		ImGui::SliderFloat("Max Bias", &maxBias, 0.0f, 0.05);
		ImGui::Checkbox("Rotate Shapes", &isRotating);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwPollEvents();

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}

//Author: Nicholas Tvaroha
void drawScene(Shader &aShader) {
	//Draw cubes
	for (int i = 0; i < 2; i++) {
		aShader.setMat4("_Model", cubeTransform[i].getModelMatrix());
		cubeMesh.draw();
	}

	//Draw spheres
	for (int i = 0; i < 2; i++) {
		aShader.setMat4("_Model", sphereTransform[i].getModelMatrix());
		sphereMesh.draw();
	}

	//Draw cylinders
	for (int i = 0; i < 2; i++) {
		aShader.setMat4("_Model", cylinderTransform[i].getModelMatrix());
		cylinderMesh.draw();
	}

	aShader.setInt("_UseTexture2", true);
	//Draw planes
	for (int i = 0; i < 2; i++) {
		aShader.setMat4("_Model", planeTransform[i].getModelMatrix());
		planeMesh.draw();
	}

	//Draw quads
	for (int i = 0; i < 4; i++) {
		aShader.setMat4("_Model", quadTransform[i].getModelMatrix());
		quadMesh.draw();
	}
}

//Author: Nicholas Tvaroha
GLuint createTexture(const char* filePath) {
	GLuint tempTexture;
	glGenTextures(1, &tempTexture);
	glBindTexture(GL_TEXTURE_2D, tempTexture);

	int width, height, numComponents;
	unsigned char* textureData = stbi_load(filePath, &width, &height, &numComponents, 0);
	if (textureData == NULL) {
		printf("File failed to load");
	}

	//Texture Wrapping
	glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	//Texture Filtering
	glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	switch (numComponents) {
	case 1:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R, width, height, 0, GL_R, GL_UNSIGNED_BYTE, textureData);
		break;
	case 2:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, textureData);
		break;
	case 3:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);
		break;
	case 4:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);
		break;
	}

	glGenerateMipmap(GL_TEXTURE_2D);

	return tempTexture;
}
//Author: Eric Winebrenner
void resizeFrameBufferCallback(GLFWwindow* window, int width, int height)
{
	SCREEN_WIDTH = width;
	SCREEN_HEIGHT = height;
	camera.setAspectRatio((float)SCREEN_WIDTH / SCREEN_HEIGHT);
	glViewport(0, 0, width, height);
}
//Author: Eric Winebrenner
void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
	if (keycode == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}
	//Reset camera
	if (keycode == GLFW_KEY_R && action == GLFW_PRESS) {
		camera.setPosition(glm::vec3(0, 0, 5));
		camera.setYaw(-90.0f);
		camera.setPitch(0.0f);
		firstMouseInput = false;
	}
	if (keycode == GLFW_KEY_1 && action == GLFW_PRESS) {
		wireFrame = !wireFrame;
		glPolygonMode(GL_FRONT_AND_BACK, wireFrame ? GL_LINE : GL_FILL);
	}
}
//Author: Eric Winebrenner
void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (abs(yoffset) > 0) {
		float fov = camera.getFov() - (float)yoffset * CAMERA_ZOOM_SPEED;
		camera.setFov(fov);
	}
}
//Author: Eric Winebrenner
void mousePosCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
		return;
	}
	if (!firstMouseInput) {
		prevMouseX = xpos;
		prevMouseY = ypos;
		firstMouseInput = true;
	}
	float yaw = camera.getYaw() + (float)(xpos - prevMouseX) * MOUSE_SENSITIVITY;
	camera.setYaw(yaw);
	float pitch = camera.getPitch() - (float)(ypos - prevMouseY) * MOUSE_SENSITIVITY;
	pitch = glm::clamp(pitch, -89.9f, 89.9f);
	camera.setPitch(pitch);
	prevMouseX = xpos;
	prevMouseY = ypos;
}
//Author: Eric Winebrenner
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	//Toggle cursor lock
	if (button == MOUSE_TOGGLE_BUTTON && action == GLFW_PRESS) {
		int inputMode = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
		glfwSetInputMode(window, GLFW_CURSOR, inputMode);
		glfwGetCursorPos(window, &prevMouseX, &prevMouseY);
	}
}

//Author: Eric Winebrenner
//Returns -1, 0, or 1 depending on keys held
float getAxis(GLFWwindow* window, int positiveKey, int negativeKey) {
	float axis = 0.0f;
	if (glfwGetKey(window, positiveKey)) {
		axis++;
	}
	if (glfwGetKey(window, negativeKey)) {
		axis--;
	}
	return axis;
}

//Author: Eric Winebrenner
//Get input every frame
void processInput(GLFWwindow* window) {

	float moveAmnt = CAMERA_MOVE_SPEED * deltaTime;

	//Get camera vectors
	glm::vec3 forward = camera.getForward();
	glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
	glm::vec3 up = glm::normalize(glm::cross(forward, right));

	glm::vec3 position = camera.getPosition();
	position += forward * getAxis(window, GLFW_KEY_W, GLFW_KEY_S) * moveAmnt;
	position += right * getAxis(window, GLFW_KEY_D, GLFW_KEY_A) * moveAmnt;
	position += up * getAxis(window, GLFW_KEY_Q, GLFW_KEY_E) * moveAmnt;
	camera.setPosition(position);
}
