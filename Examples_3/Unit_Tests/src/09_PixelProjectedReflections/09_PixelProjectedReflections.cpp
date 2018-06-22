/*
*
* Copyright (c) 2018 Confetti Interactive Inc.
*
* This file is part of The-Forge
* (see https://github.com/ConfettiFX/The-Forge).
*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*/

// Unit Test for testing materials and pbr.

//asimp importer
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"

//Renderer
#include "../../../../Common_3/Renderer/GpuProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"



#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"


/// Camera Controller
#define GUI_CAMERACONTROLLER 1
#define FPS_CAMERACONTROLLER 2

#define USE_CAMERACONTROLLER FPS_CAMERACONTROLLER

#if defined(DIRECT3D12)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
#if defined(_WIN32)
#define RESOURCE_DIR "PCVulkan"
#elif defined(LINUX)
#define RESOURCE_DIR "LINUXVulkan"
#endif
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
#else
#error PLATFORM NOT SUPPORTED
#endif


#ifdef _DURANGO
// Durango load assets from 'Layout\Image\Loose'
const char* pszRoots[] =
{
	"Shaders/Binary/",	// FSR_BinShaders
	"Shaders/",		// FSR_SrcShaders
	"Shaders/Binary/",			// FSR_BinShaders_Common
	"Shaders/",					// FSR_SrcShaders_Common
	"Textures/",						// FSR_Textures
	"Meshes/",						// FSR_Meshes
	"Fonts/",						// FSR_Builtin_Fonts
	"",								// FSR_GpuConfig
	"",															// FSR_OtherFiles
};
#else
//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[] =
{
	"../../../src/09_PixelProjectedReflections/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../../src/09_PixelProjectedReflections/" RESOURCE_DIR "/",			// FSR_SrcShaders
	"",													// FSR_BinShaders_Common
	"",													// FSR_SrcShaders_Common
	"../../../../../Art/Sponza/Textures/",			// FSR_Textures
	"../../../../../Art/Sponza/Meshes/",				// FSR_Meshes
	"../../../UnitTestResources/Fonts/",				// FSR_Builtin_Fonts
	"../../../src/09_PixelProjectedReflections/GPUCfg/",				// FSR_GpuConfig
	"",													// FSR_OtherFiles
};
#endif

LogManager gLogManager;


#define MAX_IN_ROW  4
#define TOTAL_SPHERE 16

#define DEFERRED_RT_COUNT 3

#define LOAD_MATERIAL_BALL

#define MAX_PLANES 4

#define DegToRad 0.01745329251994329576923690768489f;

struct Vertex {
	float3 mPos;
	float3 mNormal;
	float2 mUv;
};

// Have a uniform for camera data
struct UniformCamData
{
	mat4 mProjectView;
	vec3 mCamPos;
};

// Have a uniform for extended camera data
struct UniformExtendedCamData
{
	mat4 mViewMat;
	mat4 mProjMat;
	mat4 mViewProjMat;
	mat4 mInvViewProjMat;

	vec4 mCameraWorldPos;
	vec4 mViewPortSize;
};

// Have a uniform for PPR properties
struct UniformPPRProData
{
	uint renderMode;
	float useHolePatching;
	float useExpensiveHolePatching;
	float useNormalMap;

	float intensity;
	float useFadeEffect;
	float padding01;
	float padding02;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4 mWorldMat;
	float mRoughness = 0.04f;
	float mMetallic = 0.0f;
	int pbrMaterials = -1;
};

struct Light
{
	vec4 mPos;
	vec4 mCol;
	float mRadius;
	float mIntensity;
	float _pad0;
	float _pad1;
};

struct UniformLightData
{
	// Used to tell our shaders how many lights are currently present 
	Light mLights[16]; // array of lights seem to be broken so just a single light for now
	int mCurrAmountOfLights = 0;
};

struct DirectionalLight
{
	vec4 mPos;
	vec4 mCol; //alpha is the intesity
	vec4 mDir;
};

struct UniformDirectionalLightData
{
	// Used to tell our shaders how many lights are currently present 
	DirectionalLight mLights[16]; // array of lights seem to be broken so just a single light for now
	int mCurrAmountOfDLights = 0;
};

struct PlaneInfo
{
	mat4 rotMat;
	vec4 centerPoint;
	vec4 size;
};

struct UniformPlaneInfoData
{
	PlaneInfo planeInfo[MAX_PLANES];
	uint32_t numPlanes;
	uint32_t pad00;
	uint32_t pad01;
	uint32_t pad02;
};

enum
{
	SCENE_ONLY = 0,
	PPR_ONLY = 1,
	SCENE_WITH_PPR = 2,
	SCENE_EXCLU_PPR = 3,
};

static bool gUseHolePatching = true;
static bool gUseExpensiveHolePatching = true;

static bool gUseNormalMap = false;
static bool gUseFadeEffect = true;

static int gRenderMode = SCENE_WITH_PPR;

static int gPlaneNumber = 1;
static float gPlaneSize = 75.0f;
static float gRRP_Intensity = 0.2f;

const char*		pMaterialImageFileNames[];

const uint32_t				gImageCount = 3;
bool						gToggleVSync = false;

Renderer*					pRenderer = NULL;
UIApp						gAppUI;

Queue*						pGraphicsQueue = NULL;
CmdPool*					pCmdPool = NULL;
Cmd**						ppCmds = NULL;

CmdPool*					pPreCmdPool = NULL;
Cmd**						pPrepCmds = NULL;

CmdPool*					pBrdfCmdPool = NULL;
Cmd**						pBrdfCmds = NULL;

CmdPool*					pPPR_ProjectionCmdPool = NULL;
Cmd**						pPPR_ProjectionCmds = NULL;

CmdPool*					pPPR_ReflectionCmdPool = NULL;
Cmd**						pPPR_ReflectionCmds = NULL;


SwapChain*					pSwapChain = NULL;

RenderTarget*				pRenderTargetDeferredPass[DEFERRED_RT_COUNT] = { nullptr };

RenderTarget*				pSceneBuffer = NULL;
RenderTarget*				pReflectionBuffer = NULL;

RenderTarget*				pDepthBuffer = NULL;
Fence*						pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*					pImageAcquiredSemaphore = NULL;
Semaphore*					pRenderCompleteSemaphores[gImageCount] = { NULL };


Shader*						pShaderBRDF = NULL;
Pipeline*					pPipelineBRDF = NULL;
RootSignature*				pRootSigBRDF = NULL;

Buffer*						pSkyboxVertexBuffer = NULL;
Shader*						pSkyboxShader = NULL;
Pipeline*					pSkyboxPipeline = NULL;
RootSignature*				pSkyboxRootSignature = NULL;

Shader*						pPPR_ProjectionShader = NULL;
RootSignature*				pPPR_ProjectionRootSignature = NULL;
Pipeline*					pPPR_ProjectionPipeline = NULL;

Shader*						pPPR_ReflectionShader = NULL;
RootSignature*				pPPR_ReflectionRootSignature = NULL;
Pipeline*					pPPR_ReflectionPipeline = NULL;

Shader*						pPPR_HolePatchingShader = NULL;
RootSignature*				pPPR_HolePatchingRootSignature = NULL;
Pipeline*					pPPR_HolePatchingPipeline = NULL;

Buffer*						pScreenQuadVertexBuffer = NULL;

Shader*						pShaderGbuffers = NULL;
Pipeline*					pPipelineGbuffers = NULL;
RootSignature*				pRootSigGbuffers = NULL;

Texture*					pSkybox = NULL;
Texture*					pBRDFIntegrationMap = NULL;
Texture*					pIrradianceMap = NULL;
Texture*					pSpecularMap = NULL;

Buffer*						pIntermediateBuffer[gImageCount] = { NULL };

#define TOTAL_IMGS 84
Texture*			        pMaterialTextures[TOTAL_IMGS];

tinystl::vector<int>		gSponzaTextureIndexforMaterial;

//For clearing Intermediate Buffer
tinystl::vector<uint32_t> gInitializeVal;

#ifdef TARGET_IOS
Texture*					pVirtualJoystickTex = NULL;
#endif

UniformObjData				pUniformDataMVP;


/************************************************************************/
// Vertex buffers for the model
/************************************************************************/

//Sponza
tinystl::vector<Buffer*>	pSponzaVertexBufferPosition;
tinystl::vector<Buffer*>	pSponzaIndexBuffer;
Buffer*						pSponzaBuffer;
tinystl::vector<int>		gSponzaMaterialID;

//Lion
Buffer*						pLionVertexBufferPosition;
Buffer*						pLionIndexBuffer;
Buffer*						pLionBuffer;


Buffer*						pBufferUniformCamera[gImageCount] = { NULL };
UniformCamData				pUniformDataCamera;

UniformCamData				gUniformDataSky;

Buffer*						pBufferUniformExtendedCamera[gImageCount] = { NULL };
UniformExtendedCamData		pUniformDataExtenedCamera;

Buffer*                     pBufferUniformCameraSky[gImageCount] = { NULL };

Buffer*						pBufferUniformPPRPro[gImageCount] = { NULL };
UniformPPRProData			pUniformPPRProData;

Buffer*						pBufferUniformLights = NULL;
UniformLightData			pUniformDataLights;

Buffer*						pBufferUniformDirectionalLights = NULL;
UniformDirectionalLightData	pUniformDataDirectionalLights;


Buffer*						pBufferUniformPlaneInfo[gImageCount] = { NULL };
UniformPlaneInfoData		pUniformDataPlaneInfo;

Shader*						pShaderPostProc = NULL;
Pipeline*					pPipelinePostProc = NULL;

DepthState*					pDepth = NULL;
RasterizerState*			pRasterstateDefault = NULL;
Sampler*					pSamplerBilinear = NULL;
Sampler*                    pSamplerLinear = NULL;

Sampler*					pSamplerNearest = NULL;

uint32_t					gFrameIndex = 0;

GpuProfiler*				pGpuProfiler = NULL;

BlendState*					pBlendStateOneZero = nullptr;

// PBR Texture values (these values are mirrored on the shaders).
const uint32_t gBRDFIntegrationSize = 512;
const uint32_t gSkyboxSize = 1024;
const uint32_t gSkyboxMips = 11;
const uint32_t gIrradianceSize = 32;
const uint32_t gSpecularSize = 128;
const uint32_t gSpecularMips = 5;

tinystl::vector<Buffer*>	gSphereBuffers;

ICameraController*			pCameraController = NULL;

DebugTextDrawDesc gFrameTimeDraw = DebugTextDrawDesc(0, 0xff00ffff, 18);

tinystl::vector<int> gSponzaIndicesArray;
tinystl::vector<int> gLionIndicesArray;

GuiComponent* pGui;

const char*		pTextureName[] =
{
	"albedoMap",
	"normalMap",
	"metallicMap",
	"roughnessMap",
	"aoMap"
};

const char*		gModel_Sponza = "sponza.obj";
const char*		gModel_Lion = "lion.obj";

void transitionRenderTargets()
{
	// Transition render targets to desired state
	const uint32_t numBarriers = gImageCount + 1;
	TextureBarrier rtBarriers[numBarriers] = {};
	for (uint32_t i = 0; i < gImageCount; ++i)
		rtBarriers[i] = { pSwapChain->ppSwapchainRenderTargets[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
	rtBarriers[numBarriers - 1] = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
	beginCmd(ppCmds[0]);
	cmdResourceBarrier(ppCmds[0], 0, 0, numBarriers, rtBarriers, false);
	endCmd(ppCmds[0]);
	queueSubmit(pGraphicsQueue, 1, &ppCmds[0], pRenderCompleteFences[0], 0, NULL, 0, NULL);
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[0], false);
}

// Compute PBR maps (skybox, BRDF Integration Map, Irradiance Map and Specular Map).
void computePBRMaps()
{
	// Temporary resources that will be loaded on PBR preprocessing.
	Texture* pPanoSkybox = NULL;
	Buffer* pSkyBuffer = NULL;
	Buffer* pIrrBuffer = NULL;
	Buffer* pSpecBuffer = NULL;

	Shader* pPanoToCubeShader = NULL;
	RootSignature* pPanoToCubeRootSignature = NULL;
	Pipeline* pPanoToCubePipeline = NULL;
	Shader* pBRDFIntegrationShader = NULL;
	RootSignature* pBRDFIntegrationRootSignature = NULL;
	Pipeline* pBRDFIntegrationPipeline = NULL;
	Shader* pIrradianceShader = NULL;
	RootSignature* pIrradianceRootSignature = NULL;
	Pipeline* pIrradiancePipeline = NULL;
	Shader* pSpecularShader = NULL;
	RootSignature* pSpecularRootSignature = NULL;
	Pipeline* pSpecularPipeline = NULL;
	Sampler* pSkyboxSampler = NULL;

	SamplerDesc samplerDesc = {
		FILTER_TRILINEAR, FILTER_TRILINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
	};
	addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

	// Load the skybox panorama texture.
	TextureLoadDesc panoDesc = {};
#ifndef TARGET_IOS
	panoDesc.mRoot = FSR_Textures;
#else
	panoDesc.mRoot = FSRoot::FSR_Absolute; // Resources on iOS are bundled with the application.
#endif
	panoDesc.mUseMipmaps = true;
	panoDesc.pFilename = "LA_Helipad.hdr";
	panoDesc.ppTexture = &pPanoSkybox;
	addResource(&panoDesc);

	// Create empty images for each PBR texture.
	Image skyboxImg, irrImg, specImg;
	unsigned char* skyboxImgBuff = skyboxImg.Create(ImageFormat::RGBA32F, gSkyboxSize, gSkyboxSize, 0, gSkyboxMips);
	unsigned char* irrImgBuff = irrImg.Create(ImageFormat::RGBA32F, gIrradianceSize, gIrradianceSize, 0, 1);
	unsigned char* specImgBuff = specImg.Create(ImageFormat::RGBA32F, gSpecularSize, gSpecularSize, 0, gSpecularMips);


	// Get the images buffer size.
	uint32_t skyboxSize = skyboxImg.GetMipMappedSize(0, gSkyboxMips, ImageFormat::RGBA32F);
	uint32_t irrSize = irrImg.GetMipMappedSize(0, 1, ImageFormat::RGBA32F);
	uint32_t specSize = specImg.GetMipMappedSize(0, gSpecularMips, ImageFormat::RGBA32F);

	// Create empty texture for BRDF integration map.
	TextureLoadDesc brdfIntegrationLoadDesc = {};
	TextureDesc brdfIntegrationDesc = {};
	brdfIntegrationDesc.mType = TEXTURE_TYPE_2D;
	brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
	brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
	brdfIntegrationDesc.mDepth = 1;
	brdfIntegrationDesc.mArraySize = 1;
	brdfIntegrationDesc.mMipLevels = 1;
	brdfIntegrationDesc.mFormat = ImageFormat::RG32F;
	brdfIntegrationDesc.mUsage = (TextureUsage)(TEXTURE_USAGE_SAMPLED_IMAGE | TEXTURE_USAGE_UNORDERED_ACCESS);
	brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
	brdfIntegrationDesc.mHostVisible = false;
	brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
	brdfIntegrationLoadDesc.ppTexture = &pBRDFIntegrationMap;
	addResource(&brdfIntegrationLoadDesc);

	// Add empty buffer resource for storing the skybox cubemap texture values.
	BufferLoadDesc skyboxBufferDesc = {};
	skyboxBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV;
	skyboxBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	skyboxBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#ifndef METAL
	skyboxBufferDesc.mDesc.mStructStride = sizeof(float) * 4;
	skyboxBufferDesc.mDesc.mElementCount = skyboxSize / skyboxBufferDesc.mDesc.mStructStride;
	skyboxBufferDesc.mDesc.mSize = skyboxBufferDesc.mDesc.mStructStride * skyboxBufferDesc.mDesc.mElementCount;
#else
	skyboxBufferDesc.mDesc.mStructStride = sizeof(float);
	skyboxBufferDesc.mDesc.mElementCount = skyboxSize;
	skyboxBufferDesc.mDesc.mSize = skyboxSize;
#endif
	skyboxBufferDesc.pData = NULL;
	skyboxBufferDesc.ppBuffer = &pSkyBuffer;
	addResource(&skyboxBufferDesc);

	// Add empty buffer resource for storing the irradiance cubemap texture values.
	BufferLoadDesc irradianceBufferDesc = {};
	irradianceBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV;
	irradianceBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	irradianceBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#ifndef METAL
	irradianceBufferDesc.mDesc.mStructStride = sizeof(float) * 4;
	irradianceBufferDesc.mDesc.mElementCount = irrSize;
	irradianceBufferDesc.mDesc.mSize = irradianceBufferDesc.mDesc.mStructStride * irradianceBufferDesc.mDesc.mElementCount;
#else
	irradianceBufferDesc.mDesc.mStructStride = sizeof(float);
	irradianceBufferDesc.mDesc.mElementCount = irrSize;
	irradianceBufferDesc.mDesc.mSize = irrSize;
#endif
	irradianceBufferDesc.pData = NULL;
	irradianceBufferDesc.ppBuffer = &pIrrBuffer;
	addResource(&irradianceBufferDesc);

	// Add empty buffer resource for storing the specular cubemap texture values.
	BufferLoadDesc specularBufferDesc = {};
	specularBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV;
	specularBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	specularBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#ifndef METAL
	specularBufferDesc.mDesc.mStructStride = sizeof(float) * 4;
	specularBufferDesc.mDesc.mElementCount = specSize;
	specularBufferDesc.mDesc.mSize = specularBufferDesc.mDesc.mStructStride * specularBufferDesc.mDesc.mElementCount;
#else
	specularBufferDesc.mDesc.mStructStride = sizeof(float);
	specularBufferDesc.mDesc.mElementCount = specSize;
	specularBufferDesc.mDesc.mSize = specSize;
#endif

	specularBufferDesc.pData = NULL;
	specularBufferDesc.ppBuffer = &pSpecBuffer;
	addResource(&specularBufferDesc);

	// Load pre-processing shaders.
	ShaderLoadDesc panoToCubeShaderDesc = {};
	panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, FSR_SrcShaders };

	ShaderLoadDesc brdfIntegrationShaderDesc = {};
	brdfIntegrationShaderDesc.mStages[0] = { "BRDFIntegration.comp", NULL, 0, FSR_SrcShaders };

	ShaderLoadDesc irradianceShaderDesc = {};
	irradianceShaderDesc.mStages[0] = { "computeIrradianceMap.comp", NULL, 0, FSR_SrcShaders };

	ShaderLoadDesc specularShaderDesc = {};
	specularShaderDesc.mStages[0] = { "computeSpecularMap.comp", NULL, 0, FSR_SrcShaders };

	addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);
	addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);
	addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
	addShader(pRenderer, &specularShaderDesc, &pSpecularShader);

	RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
	RootSignatureDesc brdfRootDesc = { &pBRDFIntegrationShader, 1 };
	RootSignatureDesc irradianceRootDesc = { &pIrradianceShader, 1 };
	RootSignatureDesc specularRootDesc = { &pSpecularShader, 1 };
	addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);
	addRootSignature(pRenderer, &brdfRootDesc, &pBRDFIntegrationRootSignature);
	addRootSignature(pRenderer, &irradianceRootDesc, &pIrradianceRootSignature);
	addRootSignature(pRenderer, &specularRootDesc, &pSpecularRootSignature);

	ComputePipelineDesc pipelineSettings = { 0 };
	pipelineSettings.pShaderProgram = pPanoToCubeShader;
	pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
	addComputePipeline(pRenderer, &pipelineSettings, &pPanoToCubePipeline);
	pipelineSettings.pShaderProgram = pBRDFIntegrationShader;
	pipelineSettings.pRootSignature = pBRDFIntegrationRootSignature;
	addComputePipeline(pRenderer, &pipelineSettings, &pBRDFIntegrationPipeline);
	pipelineSettings.pShaderProgram = pIrradianceShader;
	pipelineSettings.pRootSignature = pIrradianceRootSignature;
	addComputePipeline(pRenderer, &pipelineSettings, &pIrradiancePipeline);
	pipelineSettings.pShaderProgram = pSpecularShader;
	pipelineSettings.pRootSignature = pSpecularRootSignature;
	addComputePipeline(pRenderer, &pipelineSettings, &pSpecularPipeline);

	// Since this happens on iniatilization, use the first cmd/fence pair available.
	Cmd* cmd = ppCmds[0];
	Fence* pRenderCompleteFence = pRenderCompleteFences[0];

	// Compute the BRDF Integration map.
	beginCmd(cmd);
	cmdBindPipeline(cmd, pBRDFIntegrationPipeline);
	DescriptorData params[3] = {};
	params[0].pName = "dstTexture";
	params[0].ppTextures = &pBRDFIntegrationMap;
	cmdBindDescriptors(cmd, pBRDFIntegrationRootSignature, 1, params);
	const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
	cmdDispatch(cmd, gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

	TextureBarrier srvBarrier = { pBRDFIntegrationMap, RESOURCE_STATE_SHADER_RESOURCE };
	cmdResourceBarrier(cmd, 0, NULL, 1, &srvBarrier, false);

	// Store the panorama texture inside a cubemap.
	cmdBindPipeline(cmd, pPanoToCubePipeline);
	params[0].pName = "srcTexture";
	params[0].ppTextures = &pPanoSkybox;
	params[1].pName = "dstBuffer";
	params[1].ppBuffers = &pSkyBuffer;
	params[2].pName = "skyboxSampler";
	params[2].ppSamplers = &pSkyboxSampler;
	cmdBindDescriptors(cmd, pPanoToCubeRootSignature, 2, params);
	pThreadGroupSize = pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
	cmdDispatch(cmd, gSkyboxSize / pThreadGroupSize[0], gSkyboxSize / pThreadGroupSize[1], 6);
	endCmd(cmd);
	queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 0, 0, 0, 0);
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

	// Upload the cubemap skybox's CPU image contents to the GPU.
	memcpy(skyboxImgBuff, pSkyBuffer->pCpuMappedAddress, skyboxSize);
	TextureLoadDesc skyboxUpload = {};
	skyboxUpload.pImage = &skyboxImg;
	skyboxUpload.ppTexture = &pSkybox;
	addResource(&skyboxUpload);

	// After the skybox cubemap is on GPU memory, we can precompute the irradiance and specular PBR maps.
	beginCmd(cmd);
	cmdBindPipeline(cmd, pIrradiancePipeline);
	params[0].pName = "srcTexture";
	params[0].ppTextures = &pSkybox;
	params[1].pName = "dstBuffer";
	params[1].ppBuffers = &pIrrBuffer;
	params[2].pName = "skyboxSampler";
	params[2].ppSamplers = &pSkyboxSampler;
	cmdBindDescriptors(cmd, pIrradianceRootSignature, 3, params);
	pThreadGroupSize = pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
	cmdDispatch(cmd, gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);

	cmdBindPipeline(cmd, pSpecularPipeline);
	params[0].pName = "srcTexture";
	params[0].ppTextures = &pSkybox;
	params[1].pName = "dstBuffer";
	params[1].ppBuffers = &pSpecBuffer;
	params[2].pName = "skyboxSampler";
	params[2].ppSamplers = &pSkyboxSampler;
	cmdBindDescriptors(cmd, pSpecularRootSignature, 3, params);
	pThreadGroupSize = pSpecularShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
	cmdDispatch(cmd, gSpecularSize / pThreadGroupSize[0], gSpecularSize / pThreadGroupSize[1], 6);
	endCmd(cmd);
	queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 0, 0, 0, 0);
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

	// Upload both the irradiance and specular maps to GPU.
	memcpy(irrImgBuff, pIrrBuffer->pCpuMappedAddress, irrSize);
	memcpy(specImgBuff, pSpecBuffer->pCpuMappedAddress, specSize);
	TextureLoadDesc irrUpload = {};
	irrUpload.pImage = &irrImg;
	irrUpload.ppTexture = &pIrradianceMap;
	addResource(&irrUpload);
	TextureLoadDesc specUpload = {};
	specUpload.pImage = &specImg;
	specUpload.ppTexture = &pSpecularMap;
	addResource(&specUpload);

	// Remove temporary resources.
	removePipeline(pRenderer, pSpecularPipeline);
	removeRootSignature(pRenderer, pSpecularRootSignature);
	removeShader(pRenderer, pSpecularShader);

	removePipeline(pRenderer, pIrradiancePipeline);
	removeRootSignature(pRenderer, pIrradianceRootSignature);
	removeShader(pRenderer, pIrradianceShader);

	removePipeline(pRenderer, pBRDFIntegrationPipeline);
	removeRootSignature(pRenderer, pBRDFIntegrationRootSignature);
	removeShader(pRenderer, pBRDFIntegrationShader);

	removePipeline(pRenderer, pPanoToCubePipeline);
	removeRootSignature(pRenderer, pPanoToCubeRootSignature);
	removeShader(pRenderer, pPanoToCubeShader);


	removeResource(pPanoSkybox);
	removeResource(pIrrBuffer);
	removeResource(pSpecBuffer);
	removeResource(pSkyBuffer);

	removeSampler(pRenderer, pSkyboxSampler);

	skyboxImg.Destroy();
	irrImg.Destroy();
	specImg.Destroy();
}

void assignSponzaTextures();

//loadModels
bool loadModels()
{
	//Load Sponza
	Model sponza;
	String sceneFullPath = FileSystem::FixPath(gModel_Sponza, FSRoot::FSR_Meshes);
#ifdef TARGET_IOS
	//TODO: need to unify this using filsystem interface
	//iOS requires path using bundle identifier
	NSString * fileUrl = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String : gModel_Sponza] ofType : @""];
	sceneFullPath = [fileUrl fileSystemRepresentation];
#endif

	if (!AssimpImporter::ImportModel(sceneFullPath.c_str(), &sponza))
	{
		ErrorMsg("Failed to load %s", FileSystem::GetFileNameAndExtension(sceneFullPath).c_str());
		return false;
	}

	size_t sponza_meshCount = sponza.mMeshArray.size();
	size_t sponza_matCount = sponza.mMaterialList.size();

	for (int i = 0; i < sponza_meshCount; i++)
	{
		Mesh subMesh = sponza.mMeshArray[i];

		gSponzaMaterialID.push_back(subMesh.mMaterialId);

		size_t size_Sponza = subMesh.mIndices.size();
		tinystl::vector<Vertex> sponzaVertices;
		tinystl::vector<uint> sponzaIndices;

		int vertexSize = (int)subMesh.mPositions.size();

		for (int i = 0; i < vertexSize; i++)
		{
			Vertex toAdd = { subMesh.mPositions[i],subMesh.mNormals[i], subMesh.mUvs[i] };
			sponzaVertices.push_back(toAdd);
		}

		for (int i = 0; i < size_Sponza; i++)
		{
			sponzaIndices.push_back(subMesh.mIndices[i]);
		}

		// Vertex position buffer for the scene
		BufferLoadDesc vbPosDesc_Sponza = {};
		vbPosDesc_Sponza.mDesc.mUsage = BUFFER_USAGE_VERTEX;
		vbPosDesc_Sponza.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbPosDesc_Sponza.mDesc.mVertexStride = sizeof(Vertex);
		vbPosDesc_Sponza.mDesc.mSize = sponzaVertices.size() * vbPosDesc_Sponza.mDesc.mVertexStride;
		vbPosDesc_Sponza.pData = sponzaVertices.data();

		Buffer* localBuffer = NULL;
		vbPosDesc_Sponza.ppBuffer = &localBuffer;
		vbPosDesc_Sponza.mDesc.pDebugName = L"Vertex Position Buffer Desc for Sponza";
		addResource(&vbPosDesc_Sponza);

		pSponzaVertexBufferPosition.push_back(localBuffer);

		// Index buffer for the scene
		BufferLoadDesc ibDesc_Sponza = {};
		ibDesc_Sponza.mDesc.mUsage = BUFFER_USAGE_INDEX;
		ibDesc_Sponza.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ibDesc_Sponza.mDesc.mIndexType = INDEX_TYPE_UINT32;
		ibDesc_Sponza.mDesc.mSize = sizeof(uint32_t) * (uint32_t)sponzaIndices.size();
		ibDesc_Sponza.pData = sponzaIndices.data();

		gSponzaIndicesArray.push_back((int)sponzaIndices.size());

		Buffer* localIndexBuffer = NULL;
		ibDesc_Sponza.ppBuffer = &localIndexBuffer;
		ibDesc_Sponza.mDesc.pDebugName = L"Index Buffer Desc for Sponza";
		addResource(&ibDesc_Sponza);

		pSponzaIndexBuffer.push_back(localIndexBuffer);
	}

	Model lion;
	sceneFullPath = FileSystem::FixPath(gModel_Lion, FSRoot::FSR_Meshes);

	if (!AssimpImporter::ImportModel(sceneFullPath.c_str(), &lion))
	{
		ErrorMsg("Failed to load %s", FileSystem::GetFileNameAndExtension(sceneFullPath).c_str());
		return false;
	}

	size_t lion_meshCount = lion.mMeshArray.size();
	size_t lion_matCount = lion.mMaterialList.size();

	Mesh subMesh = lion.mMeshArray[0];

	size_t size_Lion = subMesh.mIndices.size();
	int vertexSize = (int)subMesh.mPositions.size();

	tinystl::vector<Vertex> lionVertices;
	tinystl::vector<uint> lionIndices;

	for (int i = 0; i < vertexSize; i++)
	{
		Vertex toAdd = { subMesh.mPositions[i],subMesh.mNormals[i], subMesh.mUvs[i] };
		lionVertices.push_back(toAdd);
	}

	for (int i = 0; i < size_Lion; i++)
	{
		lionIndices.push_back(subMesh.mIndices[i]);
	}

	// Vertex position buffer for the scene
	BufferLoadDesc vbPosDesc_Lion = {};
	vbPosDesc_Lion.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	vbPosDesc_Lion.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbPosDesc_Lion.mDesc.mVertexStride = sizeof(Vertex);
	vbPosDesc_Lion.mDesc.mSize = lionVertices.size() * vbPosDesc_Lion.mDesc.mVertexStride;
	vbPosDesc_Lion.pData = lionVertices.data();

	vbPosDesc_Lion.ppBuffer = &pLionVertexBufferPosition;
	vbPosDesc_Lion.mDesc.pDebugName = L"Vertex Position Buffer Desc for Lion";
	addResource(&vbPosDesc_Lion);


	// Index buffer for the scene
	BufferLoadDesc ibDesc_Lion = {};
	ibDesc_Lion.mDesc.mUsage = BUFFER_USAGE_INDEX;
	ibDesc_Lion.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	ibDesc_Lion.mDesc.mIndexType = INDEX_TYPE_UINT32;
	ibDesc_Lion.mDesc.mSize = sizeof(uint32_t) * (uint32_t)lionIndices.size();
	ibDesc_Lion.pData = lionIndices.data();

	gLionIndicesArray.push_back((int)lionIndices.size());


	ibDesc_Lion.ppBuffer = &pLionIndexBuffer;
	ibDesc_Lion.mDesc.pDebugName = L"Index Buffer Desc for Lion";
	addResource(&ibDesc_Lion);

	//assinged right textures for each mesh
	assignSponzaTextures();

	return true;
}

class MaterialPlayground : public IApp
{
public:
	bool Init()
	{
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		// Create command pool and create a cmd buffer for each swapchain image
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pPreCmdPool);
		addCmd_n(pPreCmdPool, false, gImageCount, &pPrepCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pBrdfCmdPool);
		addCmd_n(pBrdfCmdPool, false, gImageCount, &pBrdfCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pPPR_ProjectionCmdPool);
		addCmd_n(pPPR_ProjectionCmdPool, false, gImageCount, &pPPR_ProjectionCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pPPR_ReflectionCmdPool);
		addCmd_n(pPPR_ReflectionCmdPool, false, gImageCount, &pPPR_ReflectionCmds);




		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts);

		tinystl::vector<Image> toLoad(TOTAL_IMGS);
		//adding material textures
		for (int i = 0; i <TOTAL_IMGS; ++i)
		{
			TextureLoadDesc textureDesc = {};
#ifndef TARGET_IOS
			textureDesc.mRoot = FSR_Textures;
#else
			textureDesc.mRoot = FSRoot::FSR_Absolute; // Resources on iOS are bundled with the application.
#endif
			toLoad[i].loadImage(pMaterialImageFileNames[i], true);

#ifdef TARGET_IOS
			if (toLoad[i].getFormat() == ImageFormat::RGBA8)
				toLoad[i].Convert(ImageFormat::BGRA8);
#endif
			textureDesc.mUseMipmaps = true;
			textureDesc.pImage = &toLoad[i];
			textureDesc.ppTexture = &pMaterialTextures[i];
			addResource(&textureDesc, true);
		}

#ifdef TARGET_IOS
		// Add virtual joystick texture.
		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSRoot::FSR_Absolute;
		textureDesc.mUseMipmaps = false;
		textureDesc.pFilename = "circlepad.png";
		textureDesc.ppTexture = &pVirtualJoystickTex;
		addResource(&textureDesc, true);
#endif

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);
		computePBRMaps();

		SamplerDesc samplerDesc = {
			FILTER_BILINEAR, FILTER_BILINEAR, MIPMAP_MODE_LINEAR,
			ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT
		};
		addSampler(pRenderer, &samplerDesc, &pSamplerBilinear);

		SamplerDesc nearstSamplerDesc = {
			FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST,
			ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT
		};
		addSampler(pRenderer, &nearstSamplerDesc, &pSamplerNearest);

		// GBuffer
		ShaderLoadDesc gBuffersShaderDesc = {};
		gBuffersShaderDesc.mStages[0] = { "fillGbuffers.vert", NULL, 0, FSR_SrcShaders };
		gBuffersShaderDesc.mStages[1] = { "fillGbuffers.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &gBuffersShaderDesc, &pShaderGbuffers);

		const char* pStaticSamplerNames[] = { "defaultSampler" };
		Sampler* pStaticSamplers[] = { pSamplerBilinear };

		RootSignatureDesc gBuffersRootDesc = { &pShaderGbuffers, 1 };
		gBuffersRootDesc.mStaticSamplerCount = 1;
		gBuffersRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		gBuffersRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &gBuffersRootDesc, &pRootSigGbuffers);


		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &skyboxShaderDesc, &pSkyboxShader);

		const char* pSkyboxamplerName = "skyboxSampler";
		RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = &pSkyboxamplerName;
		skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
		addRootSignature(pRenderer, &skyboxRootDesc, &pSkyboxRootSignature);

		//BRDF
		ShaderLoadDesc brdfRenderSceneShaderDesc = {};
		brdfRenderSceneShaderDesc.mStages[0] = { "renderSceneBRDF.vert", NULL, 0, FSR_SrcShaders };
		brdfRenderSceneShaderDesc.mStages[1] = { "renderSceneBRDF.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);


		const char* pStaticSampler2Names[] = { "envSampler", "defaultSampler" };
		Sampler* pStaticSamplers2[] = { pSamplerBilinear, pSamplerNearest };

		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = 2;
		brdfRootDesc.ppStaticSamplerNames = pStaticSampler2Names;
		brdfRootDesc.ppStaticSamplers = pStaticSamplers2;
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSigBRDF);

		//PPR_Projection
		ShaderLoadDesc PPR_ProjectionShaderDesc = {};
		PPR_ProjectionShaderDesc.mStages[0] = { "PPR_Projection.comp", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &PPR_ProjectionShaderDesc, &pPPR_ProjectionShader);

		RootSignatureDesc PPR_PRootDesc = { &pPPR_ProjectionShader, 1 };
		addRootSignature(pRenderer, &PPR_PRootDesc, &pPPR_ProjectionRootSignature);

		//PPR_Reflection
		ShaderLoadDesc PPR_ReflectionShaderDesc = {};
		PPR_ReflectionShaderDesc.mStages[0] = { "PPR_Reflection.vert", NULL, 0, FSR_SrcShaders };
		PPR_ReflectionShaderDesc.mStages[1] = { "PPR_Reflection.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &PPR_ReflectionShaderDesc, &pPPR_ReflectionShader);

		RootSignatureDesc PPR_RRootDesc = { &pPPR_ReflectionShader, 1 };
		PPR_RRootDesc.mStaticSamplerCount = 1;
		PPR_RRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		PPR_RRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &PPR_RRootDesc, &pPPR_ReflectionRootSignature);

		//PPR_HolePatching		
		ShaderLoadDesc PPR_HolePatchingShaderDesc = {};
		PPR_HolePatchingShaderDesc.mStages[0] = { "PPR_Holepatching.vert", NULL, 0, FSR_SrcShaders };
		PPR_HolePatchingShaderDesc.mStages[1] = { "PPR_Holepatching.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &PPR_HolePatchingShaderDesc, &pPPR_HolePatchingShader);


		const char* pStaticSamplerforHolePatchingNames[] = { "nearestSampler", "bilinearSampler" };
		Sampler* pStaticSamplersforHolePatching[] = { pSamplerNearest, pSamplerBilinear };

		RootSignatureDesc PPR_HolePatchingRootDesc = { &pPPR_HolePatchingShader, 1 };
		PPR_HolePatchingRootDesc.mStaticSamplerCount = 2;
		PPR_HolePatchingRootDesc.ppStaticSamplerNames = pStaticSamplerforHolePatchingNames;
		PPR_HolePatchingRootDesc.ppStaticSamplers = pStaticSamplersforHolePatching;
		addRootSignature(pRenderer, &PPR_HolePatchingRootDesc, &pPPR_HolePatchingRootSignature);

		// Create depth state and rasterizer state
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterstateDefault);

		if (!loadModels())
		{
			finishResourceLoading();
			return false;
		}

		BufferLoadDesc sponza_buffDesc = {};
		sponza_buffDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		sponza_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		sponza_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		sponza_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		sponza_buffDesc.pData = NULL;
		sponza_buffDesc.ppBuffer = &pSponzaBuffer;
		addResource(&sponza_buffDesc);

		BufferLoadDesc lion_buffDesc = {};
		lion_buffDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		lion_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lion_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		lion_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lion_buffDesc.pData = NULL;
		lion_buffDesc.ppBuffer = &pLionBuffer;
		addResource(&lion_buffDesc);

		//Generate sky box vertex buffer
		float skyBoxPoints[] = {
			0.5f,  -0.5f, -0.5f,1.0f, // -z
			-0.5f, -0.5f, -0.5f,1.0f,
			-0.5f, 0.5f, -0.5f,1.0f,
			-0.5f, 0.5f, -0.5f,1.0f,
			0.5f,  0.5f, -0.5f,1.0f,
			0.5f,  -0.5f, -0.5f,1.0f,

			-0.5f, -0.5f,  0.5f,1.0f,  //-x
			-0.5f, -0.5f, -0.5f,1.0f,
			-0.5f,  0.5f, -0.5f,1.0f,
			-0.5f,  0.5f, -0.5f,1.0f,
			-0.5f,  0.5f,  0.5f,1.0f,
			-0.5f, -0.5f,  0.5f,1.0f,

			0.5f, -0.5f, -0.5f,1.0f, //+x
			0.5f, -0.5f,  0.5f,1.0f,
			0.5f,  0.5f,  0.5f,1.0f,
			0.5f,  0.5f,  0.5f,1.0f,
			0.5f,  0.5f, -0.5f,1.0f,
			0.5f, -0.5f, -0.5f,1.0f,

			-0.5f, -0.5f,  0.5f,1.0f,  // +z
			-0.5f,  0.5f,  0.5f,1.0f,
			0.5f,  0.5f,  0.5f,1.0f,
			0.5f,  0.5f,  0.5f,1.0f,
			0.5f, -0.5f,  0.5f,1.0f,
			-0.5f, -0.5f,  0.5f,1.0f,

			-0.5f,  0.5f, -0.5f, 1.0f,  //+y
			0.5f,  0.5f, -0.5f,1.0f,
			0.5f,  0.5f,  0.5f,1.0f,
			0.5f,  0.5f,  0.5f,1.0f,
			-0.5f,  0.5f,  0.5f,1.0f,
			-0.5f,  0.5f, -0.5f,1.0f,

			0.5f,  -0.5f, 0.5f, 1.0f,  //-y
			0.5f,  -0.5f, -0.5f,1.0f,
			-0.5f,  -0.5f,  -0.5f,1.0f,
			-0.5f,  -0.5f,  -0.5f,1.0f,
			-0.5f,  -0.5f,  0.5f,1.0f,
			0.5f,  -0.5f, 0.5f,1.0f,
		};

		uint64_t skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
		BufferLoadDesc skyboxVbDesc = {};
		skyboxVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
		skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
		addResource(&skyboxVbDesc);


		float screenQuadPoints[] = {
			-1.0f,  3.0f, 0.5f, 0.0f, -1.0f,
			-1.0f, -1.0f, 0.5f, 0.0f, 1.0f,
			3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
		};

		uint64_t screenQuadDataSize = 5 * 3 * sizeof(float);
		BufferLoadDesc screenQuadVbDesc = {};
		screenQuadVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
		screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		screenQuadVbDesc.mDesc.mSize = screenQuadDataSize;
		screenQuadVbDesc.mDesc.mVertexStride = sizeof(float) * 5;
		screenQuadVbDesc.pData = screenQuadPoints;
		screenQuadVbDesc.ppBuffer = &pScreenQuadVertexBuffer;
		addResource(&screenQuadVbDesc);


		// Uniform buffer for camera data
		BufferLoadDesc ubCamDesc = {};
		ubCamDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubCamDesc.mDesc.mSize = sizeof(UniformCamData);
		ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
		ubCamDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubCamDesc.ppBuffer = &pBufferUniformCamera[i];
			addResource(&ubCamDesc);
			ubCamDesc.ppBuffer = &pBufferUniformCameraSky[i];
			addResource(&ubCamDesc);
		}

		// Uniform buffer for extended camera data
		BufferLoadDesc ubECamDesc = {};
		ubECamDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		ubECamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubECamDesc.mDesc.mSize = sizeof(UniformExtendedCamData);
		ubECamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
		ubECamDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubECamDesc.ppBuffer = &pBufferUniformExtendedCamera[i];
			addResource(&ubECamDesc);
		}




		// Uniform buffer for PPR's properties
		BufferLoadDesc ubPPR_ProDesc = {};
		ubPPR_ProDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		ubPPR_ProDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubPPR_ProDesc.mDesc.mSize = sizeof(UniformPPRProData);
		ubPPR_ProDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
		ubPPR_ProDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubPPR_ProDesc.ppBuffer = &pBufferUniformPPRPro[i];
			addResource(&ubPPR_ProDesc);
		}


		// Uniform buffer for light data
		BufferLoadDesc ubLightsDesc = {};
		ubLightsDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		ubLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubLightsDesc.mDesc.mSize = sizeof(UniformLightData);
		ubLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
		ubLightsDesc.pData = NULL;
		ubLightsDesc.ppBuffer = &pBufferUniformLights;
		addResource(&ubLightsDesc);


		// Uniform buffer for DirectionalLight data
		BufferLoadDesc ubDLightsDesc = {};
		ubDLightsDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		ubDLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDLightsDesc.mDesc.mSize = sizeof(UniformDirectionalLightData);
		ubDLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
		ubDLightsDesc.pData = NULL;
		ubDLightsDesc.ppBuffer = &pBufferUniformDirectionalLights;
		addResource(&ubDLightsDesc);


		// Uniform buffer for extended camera data
		BufferLoadDesc ubPlaneInfoDesc = {};
		ubPlaneInfoDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		ubPlaneInfoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubPlaneInfoDesc.mDesc.mSize = sizeof(UniformPlaneInfoData);
		ubPlaneInfoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
		ubPlaneInfoDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubPlaneInfoDesc.ppBuffer = &pBufferUniformPlaneInfo[i];
			addResource(&ubPlaneInfoDesc);
		}

		finishResourceLoading();

		for (uint32_t i = 0; i < (uint32_t)toLoad.size(); ++i)
			toLoad[i].Destroy();
		toLoad.clear();

		// prepare resources

		// Update the uniform buffer for the objects
		mat4 sponza_modelmat = mat4::translation(vec3(0.0f, -6.0f, 0.0f)) * mat4::scale(vec3(0.02f, 0.02f, 0.02f));
		pUniformDataMVP.mWorldMat = sponza_modelmat;
		pUniformDataMVP.mMetallic = 0;
		pUniformDataMVP.mRoughness = 0.5f;
		pUniformDataMVP.pbrMaterials = 1;
		BufferUpdateDesc sponza_objBuffUpdateDesc = { pSponzaBuffer, &pUniformDataMVP };
		updateResource(&sponza_objBuffUpdateDesc);


		// Update the uniform buffer for the objects
		mat4 lion_modelmat = mat4::translation(vec3(0.0f, -6.0f, 1.0f)) * mat4::rotationY(-1.5708f) * mat4::scale(vec3(0.2f, 0.2f, 0.2f));
		pUniformDataMVP.mWorldMat = lion_modelmat;
		pUniformDataMVP.mMetallic = 0;
		pUniformDataMVP.mRoughness = 0.5f;
		pUniformDataMVP.pbrMaterials = 1;
		BufferUpdateDesc lion_objBuffUpdateDesc = { pLionBuffer, &pUniformDataMVP };
		updateResource(&lion_objBuffUpdateDesc);

		// Add light to scene

		//Point light
		Light light;
		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(-12.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		pUniformDataLights.mLights[0] = light;

		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(-12.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		pUniformDataLights.mLights[1] = light;

		// Add light to scene
		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(9.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		pUniformDataLights.mLights[2] = light;

		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(9.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		pUniformDataLights.mLights[3] = light;

		pUniformDataLights.mCurrAmountOfLights = 4;
		BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights, &pUniformDataLights };
		updateResource(&lightBuffUpdateDesc);

		//Directional light
		DirectionalLight dLight;
		dLight.mCol = vec4(1.0f, 1.0f, 1.0f, 5.0f);
		dLight.mPos = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		dLight.mDir = vec4(-1.0f, -1.5f, 1.0f, 0.0f);

		pUniformDataDirectionalLights.mLights[0] = dLight;
		pUniformDataDirectionalLights.mCurrAmountOfDLights = 1;
		BufferUpdateDesc directionalLightBuffUpdateDesc = { pBufferUniformDirectionalLights, &pUniformDataDirectionalLights };
		updateResource(&directionalLightBuffUpdateDesc);

				
		
		


		// Create UI
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts);


		GuiDesc guiDesc = {};
		guiDesc.mStartSize = vec2(370.0f, 320.0f);
		guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());


		pGui = gAppUI.AddGuiComponent("Pixel-Projected Reflections", &guiDesc);


		static const int enumRenderModes[] = {
			SCENE_ONLY,
			PPR_ONLY,
			SCENE_WITH_PPR,
			SCENE_EXCLU_PPR,
			-1
		};

		static const char* enumRenderModeNames[] = {
			"Render Scene Only",
			"Render PPR Only",
			"Render Scene with PPR ",
			"Render Scene with exclusive PPR",
			NULL
		};


#if !defined(TARGET_IOS) && !defined(_DURANGO)
		UIProperty vsyncProp = UIProperty("Toggle VSync", gToggleVSync);
		pGui->AddProperty(vsyncProp);
#endif

		pGui->AddProperty(UIProperty("Render Mode", gRenderMode, enumRenderModeNames, enumRenderModes));

		pGui->AddProperty(UIProperty("Use Holepatching", gUseHolePatching));
		pGui->AddProperty(UIProperty("Use Expensive Holepatching", gUseExpensiveHolePatching));

		//pGui->AddProperty(UIProperty("Use Normalmap", gUseNormalMap));

		pGui->AddProperty(UIProperty("Use Fade Effect", gUseFadeEffect));

		pGui->AddProperty(UIProperty("Intensity of PPR", gRRP_Intensity, 0.0f, 1.0f));
		pGui->AddProperty(UIProperty("Number of Planes", gPlaneNumber, 1, 4));
		pGui->AddProperty(UIProperty("Size of Main Plane", gPlaneSize, 5.0f, 100.0f));

#if USE_CAMERACONTROLLER
		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };


		vec3 camPos{ 20.0f, -2.0f, 0.9f };
		vec3 lookat{ 0.0f, -2.0f, 0.9f };

#if USE_CAMERACONTROLLER == FPS_CAMERACONTROLLER
		pCameraController = createFpsCameraController(camPos, lookat);
		requestMouseCapture(true);
#elif USE_CAMERACONTROLLER == GUI_CAMERACONTROLLER
		pCameraController = createGuiCameraController(camPos, lookat);
#endif

		pCameraController->setMotionParameters(camParameters);

#if !defined(_DURANGO)
		registerRawMouseMoveEvent(cameraMouseMove);
		registerMouseButtonEvent(cameraMouseButton);
		registerMouseWheelEvent(cameraMouseWheel);
#endif

#ifdef TARGET_IOS
		registerTouchEvent(cameraTouch);
		registerTouchMoveEvent(cameraTouchMove);
#endif
#endif
		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

#if USE_CAMERACONTROLLER
		destroyCameraController(pCameraController);
#endif

		removeDebugRendererInterface();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);

		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeResource(pSpecularMap);
		removeResource(pIrradianceMap);
		removeResource(pSkybox);
		removeResource(pBRDFIntegrationMap);


#ifdef TARGET_IOS
		removeResource(pVirtualJoystickTex);
#endif

		removeGpuProfiler(pRenderer, pGpuProfiler);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pBufferUniformPlaneInfo[i]);
			removeResource(pBufferUniformPPRPro[i]);
			removeResource(pBufferUniformExtendedCamera[i]);
			removeResource(pBufferUniformCameraSky[i]);
			removeResource(pBufferUniformCamera[i]);
		}


		removeResource(pBufferUniformLights);
		removeResource(pBufferUniformDirectionalLights);
		removeResource(pSkyboxVertexBuffer);
		removeResource(pScreenQuadVertexBuffer);
		removeResource(pSponzaBuffer);
		removeResource(pLionBuffer);

		for (int i = 0; i<pSponzaVertexBufferPosition.size(); i++)
			removeResource(pSponzaVertexBufferPosition[i]);

		for (int i = 0; i<pSponzaIndexBuffer.size(); i++)
			removeResource(pSponzaIndexBuffer[i]);

		removeResource(pLionVertexBufferPosition);
		removeResource(pLionIndexBuffer);


		gAppUI.Exit();

		removeShader(pRenderer, pPPR_HolePatchingShader);
		removeShader(pRenderer, pPPR_ReflectionShader);
		removeShader(pRenderer, pPPR_ProjectionShader);
		removeShader(pRenderer, pShaderBRDF);
		removeShader(pRenderer, pSkyboxShader);
		removeShader(pRenderer, pShaderGbuffers);

		removeDepthState(pDepth);
		removeRasterizerState(pRasterstateDefault);
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerNearest);

		removeRootSignature(pRenderer, pPPR_HolePatchingRootSignature);
		removeRootSignature(pRenderer, pPPR_ReflectionRootSignature);
		removeRootSignature(pRenderer, pPPR_ProjectionRootSignature);
		removeRootSignature(pRenderer, pRootSigBRDF);
		removeRootSignature(pRenderer, pSkyboxRootSignature);

		removeRootSignature(pRenderer, pRootSigGbuffers);


		// Remove commands and command pool&
		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeQueue(pGraphicsQueue);

		removeCmd_n(pPreCmdPool, gImageCount, pPrepCmds);
		removeCmdPool(pRenderer, pPreCmdPool);

		removeCmd_n(pBrdfCmdPool, gImageCount, pBrdfCmds);
		removeCmdPool(pRenderer, pBrdfCmdPool);

		removeCmd_n(pPPR_ProjectionCmdPool, gImageCount, pPPR_ProjectionCmds);
		removeCmdPool(pRenderer, pPPR_ProjectionCmdPool);

		removeCmd_n(pPPR_ReflectionCmdPool, gImageCount, pPPR_ReflectionCmds);
		removeCmdPool(pRenderer, pPPR_ReflectionCmdPool);


		for (uint i = 0; i < TOTAL_IMGS; ++i)
			removeResource(pMaterialTextures[i]);

		// Remove resource loader and renderer
		removeResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addSceneBuffer())
			return false;

		if (!addReflectionBuffer())
			return false;

		if (!addGBuffers())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!addIntermeditateBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;


		// fill Gbuffers
		// Create vertex layout
		VertexLayout vertexLayoutSphere = {};
		vertexLayoutSphere.mAttribCount = 3;

		vertexLayoutSphere.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSphere.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutSphere.mAttribs[0].mBinding = 0;
		vertexLayoutSphere.mAttribs[0].mLocation = 0;
		vertexLayoutSphere.mAttribs[0].mOffset = 0;

		//normals
		vertexLayoutSphere.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutSphere.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutSphere.mAttribs[1].mLocation = 1;

		vertexLayoutSphere.mAttribs[1].mBinding = 0;
		vertexLayoutSphere.mAttribs[1].mOffset = 3 * sizeof(float);

		//texture
		vertexLayoutSphere.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutSphere.mAttribs[2].mFormat = ImageFormat::RG32F;
		vertexLayoutSphere.mAttribs[2].mLocation = 2;
		vertexLayoutSphere.mAttribs[2].mBinding = 0;
		vertexLayoutSphere.mAttribs[2].mOffset = 6 * sizeof(float); // first attribute contains 3 floats

																	/************************************************************************/
																	// Setup the resources needed for the Deferred Pass Pipeline
																	/************************************************************************/
		ImageFormat::Enum deferredFormats[DEFERRED_RT_COUNT] = {};
		bool deferredSrgb[DEFERRED_RT_COUNT] = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			deferredFormats[i] = pRenderTargetDeferredPass[i]->mDesc.mFormat;
			deferredSrgb[i] = pRenderTargetDeferredPass[i]->mDesc.mSrgb;
		}

		GraphicsPipelineDesc deferredPassPipelineSettings = {};
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = pDepth;

		deferredPassPipelineSettings.pColorFormats = deferredFormats;
		deferredPassPipelineSettings.pSrgbValues = deferredSrgb;

		deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0]->mDesc.mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0]->mDesc.mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		deferredPassPipelineSettings.pRootSignature = pRootSigGbuffers;
		deferredPassPipelineSettings.pShaderProgram = pShaderGbuffers;
		deferredPassPipelineSettings.pVertexLayout = &vertexLayoutSphere;
		deferredPassPipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &deferredPassPipelineSettings, &pPipelineGbuffers);

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;

		deferredPassPipelineSettings = {};
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = NULL;

		deferredPassPipelineSettings.pColorFormats = deferredFormats;
		deferredPassPipelineSettings.pSrgbValues = deferredSrgb;
		deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0]->mDesc.mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0]->mDesc.mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		deferredPassPipelineSettings.pRootSignature = pSkyboxRootSignature;
		deferredPassPipelineSettings.pShaderProgram = pSkyboxShader;
		deferredPassPipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		deferredPassPipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &deferredPassPipelineSettings, &pSkyboxPipeline);


		// BRDF
		//Position
		VertexLayout vertexLayoutScreenQuad = {};
		vertexLayoutScreenQuad.mAttribCount = 2;

		vertexLayoutScreenQuad.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutScreenQuad.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutScreenQuad.mAttribs[0].mBinding = 0;
		vertexLayoutScreenQuad.mAttribs[0].mLocation = 0;
		vertexLayoutScreenQuad.mAttribs[0].mOffset = 0;

		//Uv
		vertexLayoutScreenQuad.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutScreenQuad.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayoutScreenQuad.mAttribs[1].mLocation = 1;
		vertexLayoutScreenQuad.mAttribs[1].mBinding = 0;
		vertexLayoutScreenQuad.mAttribs[1].mOffset = 3 * sizeof(float); // first attribute contains 3 floats


		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pSceneBuffer->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSceneBuffer->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSceneBuffer->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSceneBuffer->mDesc.mSampleQuality;

		// pipelineSettings.pDepthState is NULL, pipelineSettings.mDepthStencilFormat should be NONE
		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pRootSigBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineBRDF);

		//PPR_Projection
		ComputePipelineDesc cpipelineSettings = { 0 };
		cpipelineSettings.pShaderProgram = pPPR_ProjectionShader;
		cpipelineSettings.pRootSignature = pPPR_ProjectionRootSignature;
		addComputePipeline(pRenderer, &cpipelineSettings, &pPPR_ProjectionPipeline);

		//PPR_Reflection
		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pReflectionBuffer->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pReflectionBuffer->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pReflectionBuffer->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pReflectionBuffer->mDesc.mSampleQuality;

		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pPPR_ReflectionRootSignature;
		pipelineSettings.pShaderProgram = pPPR_ReflectionShader;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &pipelineSettings, &pPPR_ReflectionPipeline);

		//PPR_HolePatching -> Present
		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;

		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pPPR_HolePatchingRootSignature;
		pipelineSettings.pShaderProgram = pPPR_HolePatchingShader;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &pipelineSettings, &pPPR_HolePatchingPipeline);

#if defined(VULKAN)
		transitionRenderTargets();
#endif

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		gAppUI.Unload();

		removePipeline(pRenderer, pPipelineBRDF);
		removePipeline(pRenderer, pSkyboxPipeline);
		removePipeline(pRenderer, pPPR_ProjectionPipeline);
		removePipeline(pRenderer, pPPR_ReflectionPipeline);
		removePipeline(pRenderer, pPPR_HolePatchingPipeline);
		removePipeline(pRenderer, pPipelineGbuffers);

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pSceneBuffer);
		removeRenderTarget(pRenderer, pReflectionBuffer);

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pIntermediateBuffer[i]);		

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
			removeRenderTarget(pRenderer, pRenderTargetDeferredPass[i]);

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		if (pSwapChain->mDesc.mEnableVsync != gToggleVSync)
		{
			::toggleVSync(pRenderer, &pSwapChain);
	}
#endif

#if USE_CAMERACONTROLLER
#ifndef TARGET_IOS
#ifdef _DURANGO
		if (getJoystickButtonDown(BUTTON_A))
#else
		if (getKeyDown(KEY_F))
#endif
		{
			RecenterCameraView(170.0f);
		}
#endif

		pCameraController->update(deltaTime);
#endif

		// Update camera
		mat4 viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		mat4 ViewProjMat = projMat * viewMat;

		pUniformDataCamera.mProjectView = ViewProjMat;
		pUniformDataCamera.mCamPos = pCameraController->getViewPosition();



		viewMat.setTranslation(vec3(0));
		gUniformDataSky = pUniformDataCamera;
		gUniformDataSky.mProjectView = projMat * viewMat;



		pUniformDataExtenedCamera.mCameraWorldPos = vec4(pCameraController->getViewPosition(), 1.0);
		pUniformDataExtenedCamera.mViewMat = pCameraController->getViewMatrix();
		pUniformDataExtenedCamera.mProjMat = projMat;
		pUniformDataExtenedCamera.mViewProjMat = ViewProjMat;
		pUniformDataExtenedCamera.mInvViewProjMat = inverse(ViewProjMat);
		pUniformDataExtenedCamera.mViewPortSize = vec4(static_cast<float>(mSettings.mWidth), static_cast<float>(mSettings.mHeight), 0.0, 0.0);

		pUniformPPRProData.renderMode = gRenderMode;
		pUniformPPRProData.useHolePatching = gUseHolePatching == true ? 1.0f : 0.0f;
		pUniformPPRProData.useExpensiveHolePatching = gUseExpensiveHolePatching == true ? 1.0f : 0.0f;
		pUniformPPRProData.useNormalMap = gUseNormalMap == true ? 1.0f : 0.0f;
		pUniformPPRProData.useFadeEffect = gUseFadeEffect == true ? 1.0f : 0.0f;
		pUniformPPRProData.intensity = gRRP_Intensity;


		//Planes
		pUniformDataPlaneInfo.numPlanes = gPlaneNumber;
		pUniformDataPlaneInfo.planeInfo[0].centerPoint = vec4(0.0, -6.0f, 0.9f, 0.0);
		pUniformDataPlaneInfo.planeInfo[0].size = vec4(gPlaneSize);

		pUniformDataPlaneInfo.planeInfo[1].centerPoint = vec4(10.0, -5.0f, -1.25f, 0.0);
		pUniformDataPlaneInfo.planeInfo[1].size = vec4(9.0f, 2.0f, 0.0f, 0.0f);

		pUniformDataPlaneInfo.planeInfo[2].centerPoint = vec4(10.0, -5.0f, 3.0f, 0.0);
		pUniformDataPlaneInfo.planeInfo[2].size = vec4(9.0f, 2.0f, 0.0f, 0.0f);

		pUniformDataPlaneInfo.planeInfo[3].centerPoint = vec4(10.0, 1.0f, 0.9f, 0.0);
		pUniformDataPlaneInfo.planeInfo[3].size = vec4(10.0f);


		mat4 basicMat;
		basicMat[0] = vec4(1.0, 0.0, 0.0, 0.0); //tan
		basicMat[1] = vec4(0.0, 0.0, -1.0, 0.0); //bitan
		basicMat[2] = vec4(0.0, 1.0, 0.0, 0.0); //normal
		basicMat[3] = vec4(0.0, 0.0, 0.0, 1.0);


		pUniformDataPlaneInfo.planeInfo[0].rotMat = basicMat;

		pUniformDataPlaneInfo.planeInfo[1].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * -80.0f);
		pUniformDataPlaneInfo.planeInfo[2].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * -100.0f);
		pUniformDataPlaneInfo.planeInfo[3].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * 90.0f);;

#if DIRECT3D12
		
		// Need to check why this should be transposed on DX12
		// Even view or proj matrices work well....

		pUniformDataPlaneInfo.planeInfo[0].rotMat = transpose(pUniformDataPlaneInfo.planeInfo[0].rotMat);
		pUniformDataPlaneInfo.planeInfo[1].rotMat = transpose(pUniformDataPlaneInfo.planeInfo[1].rotMat);
		pUniformDataPlaneInfo.planeInfo[2].rotMat = transpose(pUniformDataPlaneInfo.planeInfo[2].rotMat);
		pUniformDataPlaneInfo.planeInfo[3].rotMat = transpose(pUniformDataPlaneInfo.planeInfo[3].rotMat);

#endif
		
		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		// This will acquire the next swapchain image
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		tinystl::vector<Cmd*> allCmds;

		BufferUpdateDesc camBuffUpdateDesc = { pBufferUniformCamera[gFrameIndex], &pUniformDataCamera };
		updateResource(&camBuffUpdateDesc);

		BufferUpdateDesc skyboxViewProjCbv = { pBufferUniformCameraSky[gFrameIndex], &gUniformDataSky };
		updateResource(&skyboxViewProjCbv);


		BufferUpdateDesc CbvExtendedCamera = { pBufferUniformExtendedCamera[gFrameIndex], &pUniformDataExtenedCamera };
		updateResource(&CbvExtendedCamera);

		BufferUpdateDesc CbPPR_Prop = { pBufferUniformPPRPro[gFrameIndex], &pUniformPPRProData };
		updateResource(&CbPPR_Prop);

		BufferUpdateDesc planeInfoBuffUpdateDesc = { pBufferUniformPlaneInfo[gFrameIndex], &pUniformDataPlaneInfo };
		updateResource(&planeInfoBuffUpdateDesc);


		// Draw G-buffers
		Cmd* cmd = pPrepCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		//Clear G-buffers and Depth buffer
		LoadActionsDesc loadActions = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetDeferredPass[i]->mDesc.mClearValue;
		}

		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { 1.0f, 0.0f }; // Clear depth to the far plane and stencil to 0

												  // Transfer G-buffers to render target state for each buffer
		TextureBarrier barrier;
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			barrier = { pRenderTargetDeferredPass[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
			cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		}


		// Transfer DepthBuffer to a DephtWrite State
		barrier = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);


		cmdBindRenderTargets(cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass, pDepthBuffer, &loadActions);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mDesc.mWidth, (float)pRenderTargetDeferredPass[0]->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mDesc.mWidth, pRenderTargetDeferredPass[0]->mDesc.mHeight);


		// Draw the skybox.
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render SkyBox", true);

		cmdBindPipeline(cmd, pSkyboxPipeline);
		DescriptorData skyParams[2] = {};
		skyParams[0].pName = "uniformBlock";
		skyParams[0].ppBuffers = &pBufferUniformCameraSky[gFrameIndex];
		skyParams[1].pName = "skyboxTex";
		skyParams[1].ppTextures = &pSkybox;
		cmdBindDescriptors(cmd, pSkyboxRootSignature, 2, skyParams);
		cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer);
		cmdDraw(cmd, 36, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);


		// Draw Sponza
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Fill GBuffers", true);

		cmdBindPipeline(cmd, pPipelineGbuffers);
		DescriptorData params[8] = {};
		params[0].pName = "cbCamera";
		params[0].ppBuffers = &pBufferUniformCamera[gFrameIndex];

		for (int i = 0; i < pSponzaVertexBufferPosition.size(); i++)
		{


			Buffer* pSponzaVertexBuffers[] = { pSponzaVertexBufferPosition[i] };

			cmdBindVertexBuffer(cmd, 1, pSponzaVertexBuffers);
			cmdBindIndexBuffer(cmd, pSponzaIndexBuffer[i]);


			params[1].pName = "cbObject";
			params[1].ppBuffers = &pSponzaBuffer;

			int materialID = gSponzaMaterialID[i];
			materialID *= 5; //because it uses 5 basic textures for redering BRDF

			for (int j = 0; j <5; ++j) {
				//added
				params[2 + j].pName = pTextureName[j];
				params[2 + j].ppTextures = &pMaterialTextures[gSponzaTextureIndexforMaterial[materialID + j]];
			}

			
#ifdef METAL
			//bind samplers for metal
			params[7].pName = "defaultSampler";
			params[7].ppSamplers = &pSamplerBilinear;
			cmdBindDescriptors(cmd, pRootSigGbuffers, 8, params);
#else
			cmdBindDescriptors(cmd, pRootSigGbuffers, 7, params);
#endif

			cmdDrawIndexed(cmd, gSponzaIndicesArray[i], 0);

		}

		//Draw Lion

		cmdBindVertexBuffer(cmd, 1, &pLionVertexBufferPosition);
		cmdBindIndexBuffer(cmd, pLionIndexBuffer);

		params[0].pName = "cbCamera";
		params[0].ppBuffers = &pBufferUniformCamera[gFrameIndex];

		params[1].pName = "cbObject";
		params[1].ppBuffers = &pLionBuffer;

		params[2].pName = pTextureName[0];
		params[2].ppTextures = &pMaterialTextures[81];

		params[3].pName = pTextureName[1];
		params[3].ppTextures = &pMaterialTextures[83];

		params[4].pName = pTextureName[2];
		params[4].ppTextures = &pMaterialTextures[6];

		params[5].pName = pTextureName[3];
		params[5].ppTextures = &pMaterialTextures[6];

		params[6].pName = pTextureName[4];
		params[6].ppTextures = &pMaterialTextures[0];

#ifdef METAL
		//bind samplers for metal
		params[7].pName = "defaultSampler";
		params[7].ppSamplers = &pSamplerBilinear;

		cmdBindDescriptors(cmd, pRootSigGbuffers, 8, params);
#else
		cmdBindDescriptors(cmd, pRootSigGbuffers, 7, params);
#endif

		cmdDrawIndexed(cmd, gLionIndicesArray[0], 0);


		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);


		// Render BRDF
		cmd = pBrdfCmds[gFrameIndex];
		beginCmd(cmd);


		// Transfer G-buffers to a Shader resource state
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			barrier = { pRenderTargetDeferredPass[i]->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		}

		// Transfer current render target to a render target state
		barrier = { pSceneBuffer->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		// Transfer DepthBuffer to a Shader resource state
		barrier = { pDepthBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pSceneBuffer->mDesc.mClearValue;


		cmdBindRenderTargets(cmd, 1, &pSceneBuffer, NULL, &loadActions);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneBuffer->mDesc.mWidth, (float)pSceneBuffer->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSceneBuffer->mDesc.mWidth, pSceneBuffer->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render BRDF", false);

		cmdBindPipeline(cmd, pPipelineBRDF);
		DescriptorData BRDFParams[12] = {};

		BRDFParams[0].pName = "cbExtendCamera";
		BRDFParams[0].ppBuffers = &pBufferUniformExtendedCamera[gFrameIndex];

		BRDFParams[1].pName = "cbLights";
		BRDFParams[1].ppBuffers = &pBufferUniformLights;

		BRDFParams[2].pName = "cbDLights";
		BRDFParams[2].ppBuffers = &pBufferUniformDirectionalLights;

		BRDFParams[3].pName = "brdfIntegrationMap";
		BRDFParams[3].ppTextures = &pBRDFIntegrationMap;

		BRDFParams[4].pName = "irradianceMap";
		BRDFParams[4].ppTextures = &pIrradianceMap;

		BRDFParams[5].pName = "specularMap";
		BRDFParams[5].ppTextures = &pSpecularMap;

		BRDFParams[6].pName = "AlbedoTexture";
		BRDFParams[6].ppTextures = &pRenderTargetDeferredPass[0]->pTexture;

		BRDFParams[7].pName = "NormalTexture";
		BRDFParams[7].ppTextures = &pRenderTargetDeferredPass[1]->pTexture;

		BRDFParams[8].pName = "RoughnessTexture";
		BRDFParams[8].ppTextures = &pRenderTargetDeferredPass[2]->pTexture;

		BRDFParams[9].pName = "DepthTexture";
		BRDFParams[9].ppTextures = &pDepthBuffer->pTexture;

		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer);

#ifdef METAL

		BRDFParams[10].pName = "envSampler";
		BRDFParams[10].ppSamplers = &pSamplerBilinear;
		BRDFParams[11].pName = "defaultSampler";
		BRDFParams[11].ppSamplers = &pSamplerBilinear;

		cmdBindDescriptors(cmd, pRootSigBRDF, 12, BRDFParams);
#else
		cmdBindDescriptors(cmd, pRootSigBRDF, 10, BRDFParams);
#endif

		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);
		

		//ClearIntermediateBuffer
		//BufferUpdateDesc clearIntermediateBufferDesc = { pIntermediateBuffer[gFrameIndex], gInitializeVal.data() };
		//updateResource(&clearIntermediateBufferDesc);

		// PixelProjectReflections_ProjectionPass
		cmd = pPPR_ProjectionCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Pixel-Projected Reflections", false);
		
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "ProjectionPass", true);

		cmdBindPipeline(cmd, pPPR_ProjectionPipeline);

		DescriptorData PPR_ProjectionParams[5] = {};

		PPR_ProjectionParams[0].pName = "cbExtendCamera";
		PPR_ProjectionParams[0].ppBuffers = &pBufferUniformExtendedCamera[gFrameIndex];
		

		PPR_ProjectionParams[1].pName = "IntermediateBuffer";
		PPR_ProjectionParams[1].ppBuffers = &pIntermediateBuffer[gFrameIndex];

		PPR_ProjectionParams[2].pName = "DepthTexture";
		PPR_ProjectionParams[2].ppTextures = &pDepthBuffer->pTexture;

		PPR_ProjectionParams[3].pName = "planeInfoBuffer";
		PPR_ProjectionParams[3].ppBuffers = &pBufferUniformPlaneInfo[gFrameIndex];

#ifdef METAL
		PPR_ProjectionParams[4].pName = "defaultSampler";
		PPR_ProjectionParams[4].ppBuffers = &pSamplerBilinear;

		cmdBindDescriptors(cmd, pPPR_ProjectionRootSignature, 5, PPR_ProjectionParams);
#else
		cmdBindDescriptors(cmd, pPPR_ProjectionRootSignature, 4, PPR_ProjectionParams);

#endif
		const uint32_t* pThreadGroupSize = pPPR_ProjectionShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(cmd, (mSettings.mWidth * mSettings.mHeight / pThreadGroupSize[0]) + 1, 1, 1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);

		// PixelProjectReflections_ReflectionPass
		cmd = pPPR_ReflectionCmds[gFrameIndex];
		beginCmd(cmd);

		RenderTarget* pRenderTarget = pReflectionBuffer;

		barrier = { pSceneBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		// Transfer current render target to a render target state
		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);


		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);


		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "ReflectionPass", true);
		cmdBindPipeline(cmd, pPPR_ReflectionPipeline);
		DescriptorData PPR_ReflectionParams[7] = {};

		PPR_ReflectionParams[0].pName = "cbExtendCamera";
		PPR_ReflectionParams[0].ppBuffers = &pBufferUniformExtendedCamera[gFrameIndex];

		PPR_ReflectionParams[1].pName = "SceneTexture";
		PPR_ReflectionParams[1].ppTextures = &pSceneBuffer->pTexture;

		PPR_ReflectionParams[2].pName = "planeInfoBuffer";
		PPR_ReflectionParams[2].ppBuffers = &pBufferUniformPlaneInfo[gFrameIndex];

		PPR_ReflectionParams[3].pName = "IntermediateBuffer";
		PPR_ReflectionParams[3].ppBuffers = &pIntermediateBuffer[gFrameIndex];

		PPR_ReflectionParams[4].pName = "DepthTexture";
		PPR_ReflectionParams[4].ppTextures = &pDepthBuffer->pTexture;

		PPR_ReflectionParams[5].pName = "cbProperties";
		PPR_ReflectionParams[5].ppBuffers = &pBufferUniformPPRPro[gFrameIndex];

#ifdef METAL
		PPR_ReflectionParams[6].pName = "defaultSampler";
		PPR_ReflectionParams[6].ppSamplers = &pSamplerBilinear;

		cmdBindDescriptors(cmd, pPPR_ReflectionRootSignature, 7, PPR_ReflectionParams);
#else
		cmdBindDescriptors(cmd, pPPR_ReflectionRootSignature, 6, PPR_ReflectionParams);
#endif
		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer);
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);


		//Present
		cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

		barrier = { pReflectionBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "HolePatching", true);
		cmdBindPipeline(cmd, pPPR_HolePatchingPipeline);

		DescriptorData PPR_HolePatchingParams[6] = {};

		PPR_HolePatchingParams[0].pName = "cbExtendCamera";
		PPR_HolePatchingParams[0].ppBuffers = &pBufferUniformExtendedCamera[gFrameIndex];

		PPR_HolePatchingParams[1].pName = "SceneTexture";
		PPR_HolePatchingParams[1].ppTextures = &pSceneBuffer->pTexture;

		PPR_HolePatchingParams[2].pName = "SSRTexture";
		PPR_HolePatchingParams[2].ppTextures = &pReflectionBuffer->pTexture;

		PPR_HolePatchingParams[3].pName = "cbProperties";
		PPR_HolePatchingParams[3].ppBuffers = &pBufferUniformPPRPro[gFrameIndex];

#ifdef METAL

		PPR_HolePatchingParams[4].pName = "nearestSampler";
		PPR_HolePatchingParams[4].ppBuffers = &pSamplerNearest;

		PPR_HolePatchingParams[5].pName = "bilinearSampler";
		PPR_HolePatchingParams[5].ppBuffers = &pSamplerBilinear;

		cmdBindDescriptors(cmd, pPPR_HolePatchingRootSignature, 6, PPR_HolePatchingParams);
#else
		cmdBindDescriptors(cmd, pPPR_HolePatchingRootSignature, 4, PPR_HolePatchingParams);
#endif

		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer);
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		// Draw the camera controller's virtual joysticks.
		float extSide = min(mSettings.mHeight, mSettings.mWidth) * pCameraController->getVirtualJoystickExternalRadius();
		float intSide = min(mSettings.mHeight, mSettings.mWidth) * pCameraController->getVirtualJoystickInternalRadius();

		float2 joystickSize = float2(extSide);
		vec2 leftJoystickCenter = pCameraController->getVirtualLeftJoystickCenter();
		float2 leftJoystickPos = float2(leftJoystickCenter.getX() * mSettings.mWidth, leftJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
		drawDebugTexture(cmd, leftJoystickPos.x, leftJoystickPos.y, joystickSize.x, joystickSize.y, pVirtualJoystickTex, 1.0f, 1.0f, 1.0f);
		vec2 rightJoystickCenter = pCameraController->getVirtualRightJoystickCenter();
		float2 rightJoystickPos = float2(rightJoystickCenter.getX() * mSettings.mWidth, rightJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
		drawDebugTexture(cmd, rightJoystickPos.x, rightJoystickPos.y, joystickSize.x, joystickSize.y, pVirtualJoystickTex, 1.0f, 1.0f, 1.0f);

		joystickSize = float2(intSide);
		leftJoystickCenter = pCameraController->getVirtualLeftJoystickPos();
		leftJoystickPos = float2(leftJoystickCenter.getX() * mSettings.mWidth, leftJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
		drawDebugTexture(cmd, leftJoystickPos.x, leftJoystickPos.y, joystickSize.x, joystickSize.y, pVirtualJoystickTex, 1.0f, 1.0f, 1.0f);
		rightJoystickCenter = pCameraController->getVirtualRightJoystickPos();
		rightJoystickPos = float2(rightJoystickCenter.getX() * mSettings.mWidth, rightJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
		drawDebugTexture(cmd, rightJoystickPos.x, rightJoystickPos.y, joystickSize.x, joystickSize.y, pVirtualJoystickTex, 1.0f, 1.0f, 1.0f);
#endif

		drawDebugText(cmd, 8, 15, String::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

#ifndef METAL // Metal doesn't support GPU profilers
		drawDebugText(cmd, 8, 40, String::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);

		drawDebugGpuProfile(cmd, 8, 65, pGpuProfiler, NULL);

#endif

#ifndef TARGET_IOS
		gAppUI.Gui(pGui);
#endif
		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL);

		// Transition our texture to present state
		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);
		allCmds.push_back(cmd);


		queueSubmit(pGraphicsQueue, (uint32_t)allCmds.size(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

		Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);
		}
	}

	String GetName()
	{
		return "09_PixelProjectedReflections";
	}

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addSceneBuffer()
	{

		RenderTargetDesc sceneRT = {};
		sceneRT.mArraySize = 1;
		sceneRT.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		sceneRT.mDepth = 1;
		sceneRT.mFormat = ImageFormat::RGBA8;

		sceneRT.mHeight = mSettings.mHeight;
		sceneRT.mWidth = mSettings.mWidth;

		sceneRT.mSampleCount = SAMPLE_COUNT_1;
		sceneRT.mSampleQuality = 0;
		sceneRT.mType = RENDER_TARGET_TYPE_2D;
		sceneRT.mUsage = RENDER_TARGET_USAGE_COLOR;

		sceneRT.pDebugName = L"Scene Buffer";

		addRenderTarget(pRenderer, &sceneRT, &pSceneBuffer);

		return pSceneBuffer != NULL;
	}

	bool addReflectionBuffer()
	{
		RenderTargetDesc RT = {};
		RT.mArraySize = 1;
		RT.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		RT.mDepth = 1;
		RT.mFormat = ImageFormat::RGBA8;

		RT.mHeight = mSettings.mHeight;
		RT.mWidth = mSettings.mWidth;

		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
		RT.mType = RENDER_TARGET_TYPE_2D;
		RT.mUsage = RENDER_TARGET_USAGE_COLOR;

		RT.pDebugName = L"Reflection Buffer";

		addRenderTarget(pRenderer, &RT, &pReflectionBuffer);

		return pReflectionBuffer != NULL;
	}





	bool addGBuffers()
	{
		ClearValue optimizedColorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };

		/************************************************************************/
		// Deferred pass render targets
		/************************************************************************/
		RenderTargetDesc deferredRTDesc = {};
		deferredRTDesc.mArraySize = 1;
		deferredRTDesc.mClearValue = optimizedColorClearBlack;
		deferredRTDesc.mDepth = 1;

		deferredRTDesc.mWidth = mSettings.mWidth;
		deferredRTDesc.mHeight = mSettings.mHeight;
		deferredRTDesc.mSampleCount = SAMPLE_COUNT_1;
		deferredRTDesc.mSampleQuality = 0;
		deferredRTDesc.mType = RENDER_TARGET_TYPE_2D;
		deferredRTDesc.mUsage = RENDER_TARGET_USAGE_COLOR;

		deferredRTDesc.pDebugName = L"G-Buffer RTs";

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			if (i == 1 || i == 2)
				deferredRTDesc.mFormat = ImageFormat::RGBA32F;
			else
				deferredRTDesc.mFormat = ImageFormat::RGBA8;

			addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[i]);
		}

		return pRenderTargetDeferredPass[0] != NULL;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = { 1.0f, 0 };
		depthRT.mDepth = 1;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mType = RENDER_TARGET_TYPE_2D;
		depthRT.mUsage = RENDER_TARGET_USAGE_DEPTH_STENCIL;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	bool addIntermeditateBuffer()
	{
		// Add Intermediate buffer
		BufferLoadDesc IntermediateBufferDesc = {};
		IntermediateBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV | BUFFER_USAGE_STORAGE_SRV;
		IntermediateBufferDesc.mDesc.mElementCount = mSettings.mWidth * mSettings.mHeight;
		IntermediateBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		IntermediateBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		IntermediateBufferDesc.mDesc.mSize = IntermediateBufferDesc.mDesc.mStructStride * IntermediateBufferDesc.mDesc.mElementCount;

		gInitializeVal.clear();
		for (int i = 0; i < mSettings.mWidth * mSettings.mHeight; i++)
		{
			gInitializeVal.push_back(UINT32_MAX);
		}


		IntermediateBufferDesc.pData = gInitializeVal.data();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			IntermediateBufferDesc.ppBuffer = &pIntermediateBuffer[i];
			addResource(&IntermediateBufferDesc);
		}

		return pIntermediateBuffer != NULL;
	}

	void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0))
	{
		vec3 p = pCameraController->getViewPosition();
		vec3 d = p - lookAt;

		float lenSqr = lengthSqr(d);
		if (lenSqr > (maxDistance * maxDistance))
		{
			d *= (maxDistance / sqrtf(lenSqr));
		}

		p = d + lookAt;
		pCameraController->moveTo(p);
		pCameraController->lookAt(lookAt);
	}

	// Camera controller functionality
#if USE_CAMERACONTROLLER
#if !defined(_DURANGO)
	static bool cameraMouseMove(const RawMouseMoveEventData* data)
	{
		pCameraController->onMouseMove(data);
		return true;
	}

	static bool cameraMouseButton(const MouseButtonEventData* data)
	{
		pCameraController->onMouseButton(data);
		return true;
	}

	static bool cameraMouseWheel(const MouseWheelEventData* data)
	{
		pCameraController->onMouseWheel(data);
		return true;
	}

#ifdef TARGET_IOS
	static bool cameraTouch(const TouchEventData* data)
	{
		pCameraController->onTouch(data);
		return true;
	}

	static bool cameraTouchMove(const TouchEventData* data)
	{
		pCameraController->onTouchMove(data);
		return true;
	}
#endif
#endif
#endif
};


const char*		pMaterialImageFileNames[] =
{
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/ao.png",

	//common
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/Dielectric_metallic.tga",
	"SponzaPBR_Textures/Metallic_metallic.tga",
	"SponzaPBR_Textures/gi_flag.png",

	//Background
	"SponzaPBR_Textures/Background/Background_Albedo.tga",
	"SponzaPBR_Textures/Background/Background_Normal.tga",
	"SponzaPBR_Textures/Background/Background_Roughness.tga",

	//ChainTexture
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Albedo.tga",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Metallic.tga",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Normal.tga",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Roughness.tga",

	//Lion
	"SponzaPBR_Textures/Lion/Lion_Albedo.tga",
	"SponzaPBR_Textures/Lion/Lion_Normal.tga",
	"SponzaPBR_Textures/Lion/Lion_Roughness.tga",

	//Sponza_Arch
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_normal.tga",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_roughness.tga",

	//Sponza_Bricks
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Albedo.tga",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Normal.tga",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Roughness.tga",

	//Sponza_Ceiling
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_normal.tga",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_roughness.tga",

	//Sponza_Column
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_normal.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_roughness.tga",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_normal.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_roughness.tga",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_normal.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_roughness.tga",

	//Sponza_Curtain
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_normal.tga",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_normal.tga",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_normal.tga",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_metallic.tga",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_roughness.tga",

	//Sponza_Details
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_metallic.tga",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_normal.tga",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_roughness.tga",

	//Sponza_Fabric
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_normal.tga",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_normal.tga",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_metallic.tga",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_roughness.tga",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_normal.tga",

	//Sponza_FlagPole
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_diffuse.tga",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_normal.tga",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_roughness.tga",

	//Sponza_Floor
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_normal.tga",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_roughness.tga",

	//Sponza_Roof
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_normal.tga",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_roughness.tga",

	//Sponza_Thorn
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_normal.tga",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_roughness.tga",

	//Vase
	"SponzaPBR_Textures/Vase/Vase_diffuse.tga",
	"SponzaPBR_Textures/Vase/Vase_normal.tga",
	"SponzaPBR_Textures/Vase/Vase_roughness.tga",

	//VaseHanging
	"SponzaPBR_Textures/VaseHanging/VaseHanging_diffuse.tga",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_normal.tga",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_roughness.tga",


	//VasePlant
	"SponzaPBR_Textures/VasePlant/VasePlant_diffuse.tga",
	"SponzaPBR_Textures/VasePlant/VasePlant_normal.tga",
	"SponzaPBR_Textures/VasePlant/VasePlant_roughness.tga",

	//VaseRound
	"SponzaPBR_Textures/VaseRound/VaseRound_diffuse.tga",
	"SponzaPBR_Textures/VaseRound/VaseRound_normal.tga",
	"SponzaPBR_Textures/VaseRound/VaseRound_roughness.tga",

	"lion/lion_albedo.png",
	"lion/lion_specular.png",
	"lion/lion_normal.png",

};


void assignSponzaTextures()
{
	int AO = 5;
	int NoMetallic = 6;
	int Metallic = 7;

	//00 : leaf
	gSponzaTextureIndexforMaterial.push_back(66);
	gSponzaTextureIndexforMaterial.push_back(67);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(68);
	gSponzaTextureIndexforMaterial.push_back(AO);

	//01 : vase_round
	gSponzaTextureIndexforMaterial.push_back(78);
	gSponzaTextureIndexforMaterial.push_back(79);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(80);
	gSponzaTextureIndexforMaterial.push_back(AO);

	//02 : Material__57 (Plant)
	gSponzaTextureIndexforMaterial.push_back(75);
	gSponzaTextureIndexforMaterial.push_back(76);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(77);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 03 : Material__298
	gSponzaTextureIndexforMaterial.push_back(9);
	gSponzaTextureIndexforMaterial.push_back(10);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(11);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 04 : 16___Default (gi_flag)
	gSponzaTextureIndexforMaterial.push_back(8);
	gSponzaTextureIndexforMaterial.push_back(8); // !!!!!!
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(8); // !!!!!
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 05 : bricks
	gSponzaTextureIndexforMaterial.push_back(22);
	gSponzaTextureIndexforMaterial.push_back(23);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(24);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 06 :  arch
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 07 : ceiling
	gSponzaTextureIndexforMaterial.push_back(25);
	gSponzaTextureIndexforMaterial.push_back(26);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(27);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 08 : column_a
	gSponzaTextureIndexforMaterial.push_back(28);
	gSponzaTextureIndexforMaterial.push_back(29);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(30);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 09 : Floor
	gSponzaTextureIndexforMaterial.push_back(60);
	gSponzaTextureIndexforMaterial.push_back(61);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(62);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 10 : column_c
	gSponzaTextureIndexforMaterial.push_back(34);
	gSponzaTextureIndexforMaterial.push_back(35);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(36);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 11 : details
	gSponzaTextureIndexforMaterial.push_back(45);
	gSponzaTextureIndexforMaterial.push_back(47);
	gSponzaTextureIndexforMaterial.push_back(46);
	gSponzaTextureIndexforMaterial.push_back(48);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 12 : column_b
	gSponzaTextureIndexforMaterial.push_back(31);
	gSponzaTextureIndexforMaterial.push_back(32);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(33);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 13 : Material__47 - it seems missing
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 14 : flagpole
	gSponzaTextureIndexforMaterial.push_back(57);
	gSponzaTextureIndexforMaterial.push_back(58);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(59);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 15 : fabric_e (green)
	gSponzaTextureIndexforMaterial.push_back(51);
	gSponzaTextureIndexforMaterial.push_back(52);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 16 : fabric_d (blue)
	gSponzaTextureIndexforMaterial.push_back(49);
	gSponzaTextureIndexforMaterial.push_back(50);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 17 : fabric_a (red)
	gSponzaTextureIndexforMaterial.push_back(55);
	gSponzaTextureIndexforMaterial.push_back(56);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 18 : fabric_g (curtain_blue)
	gSponzaTextureIndexforMaterial.push_back(37);
	gSponzaTextureIndexforMaterial.push_back(38);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 19 : fabric_c (curtain_red)
	gSponzaTextureIndexforMaterial.push_back(41);
	gSponzaTextureIndexforMaterial.push_back(42);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 20 : fabric_f (curtain_green)
	gSponzaTextureIndexforMaterial.push_back(39);
	gSponzaTextureIndexforMaterial.push_back(40);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 21 : chain
	gSponzaTextureIndexforMaterial.push_back(12);
	gSponzaTextureIndexforMaterial.push_back(14);
	gSponzaTextureIndexforMaterial.push_back(13);
	gSponzaTextureIndexforMaterial.push_back(15);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 22 : vase_hanging
	gSponzaTextureIndexforMaterial.push_back(72);
	gSponzaTextureIndexforMaterial.push_back(73);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(74);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 23 : vase
	gSponzaTextureIndexforMaterial.push_back(69);
	gSponzaTextureIndexforMaterial.push_back(70);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(71);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 24 : Material__25 (lion)
	gSponzaTextureIndexforMaterial.push_back(16);
	gSponzaTextureIndexforMaterial.push_back(17);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(18);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 25 : roof
	gSponzaTextureIndexforMaterial.push_back(63);
	gSponzaTextureIndexforMaterial.push_back(64);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(65);
	gSponzaTextureIndexforMaterial.push_back(AO);
}

DEFINE_APPLICATION_MAIN(MaterialPlayground)