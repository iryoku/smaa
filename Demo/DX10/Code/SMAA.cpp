/**
 * Copyright (C) 2011 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2011 Belen Masia (bmasia@unizar.es) 
 * Copyright (C) 2011 Jose I. Echevarria (joseignacioechevarria@gmail.com) 
 * Copyright (C) 2011 Fernando Navarro (fernandn@microsoft.com) 
 * Copyright (C) 2011 Diego Gutierrez (diegog@unizar.es)
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the following disclaimer
 *       in the documentation and/or other materials provided with the 
 *       distribution:
 * 
 *      "Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria,
 *       Belen Masia, Fernando Navarro and Diego Gutierrez."
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS 
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are 
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the copyright holders.
 */


#include <sstream>
#include <d3d10_1.h>
#include <d3d9.h>
#include "SMAA.h"
#include "SearchTex.h"
#include "AreaTex.h"
using namespace std;


// This define is for testing the precomputed textures files:
// #define SMAA_TEST_DDS_FILES


#pragma region Useful Macros from DXUT (copy-pasted here as we prefer this to be as self-contained as possible)
#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x) { hr = (x); if (FAILED(hr)) { DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x) { hr = (x); if (FAILED(hr)) { return DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); } }
#endif
#else
#ifndef V
#define V(x) { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x) { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p) = NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }
#endif
#pragma endregion

#pragma region This stuff is for loading headers from resources
class ID3D10IncludeResource : public ID3D10Include {
    public:
        STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID *ppData, UINT *pBytes)  {
            wstringstream s;
            s << pFileName;
            HRSRC src = FindResource(GetModuleHandle(NULL), s.str().c_str(), RT_RCDATA);
            HGLOBAL res = LoadResource(GetModuleHandle(NULL), src);

            *pBytes = SizeofResource(GetModuleHandle(NULL), src);
            *ppData = (LPCVOID) LockResource(res);

            return S_OK;
        }

        STDMETHOD(Close)(THIS_ LPCVOID)  {
            return S_OK;
        }
};
#pragma endregion


SMAA::SMAA(ID3D10Device *device, int width, int height, Preset preset, bool predication, bool reprojection, const ExternalStorage &storage)
        : device(device),
          preset(preset),
          threshold(0.1f),
          cornerRounding(0.25f),
          maxSearchSteps(16),
          maxSearchStepsDiag(8) {
    HRESULT hr;

    // Check for DirectX 10.1 support:
    ID3D10Device1 *device1;
    bool dx10_1 = false;
    if (D3DX10GetFeatureLevel1(device, &device1) != E_FAIL) {
        dx10_1 = device1->GetFeatureLevel() == D3D10_FEATURE_LEVEL_10_1;
        SAFE_RELEASE(device1);
    }

    // Setup the defines for compiling the effect:
    vector<D3D10_SHADER_MACRO> defines;
    stringstream s;

    // Setup the pixel size macro:
    s << "float2(1.0 / " << width << ", 1.0 / " << height << ")";
    string pixelSizeText = s.str();
    D3D10_SHADER_MACRO pixelSizeMacro = { "SMAA_PIXEL_SIZE", pixelSizeText.c_str() };
    defines.push_back(pixelSizeMacro);

    // Setup the preset macro:
    D3D10_SHADER_MACRO presetMacros[] = {
        { "SMAA_PRESET_LOW", "1" },
        { "SMAA_PRESET_MEDIUM", "1" },
        { "SMAA_PRESET_HIGH", "1" },
        { "SMAA_PRESET_ULTRA", "1" },
        { "SMAA_PRESET_CUSTOM", "1" }
    };
    defines.push_back(presetMacros[int(preset)]);

    // Setup the predicated thresholding macro:
    if (predication) {
        D3D10_SHADER_MACRO predicationMacro = { "SMAA_PREDICATION", "1" };
        defines.push_back(predicationMacro);
    }

    // Setup the reprojection macro:
    if (reprojection) {
        D3D10_SHADER_MACRO reprojectionMacro = { "SMAA_REPROJECTION", "1" };
        defines.push_back(reprojectionMacro);
    }

    // Setup the target macro:
    if (dx10_1) {
        D3D10_SHADER_MACRO dx101Macro = { "SMAA_HLSL_4_1", "1" };
        defines.push_back(dx101Macro);
    }

    D3D10_SHADER_MACRO null = { NULL, NULL };
    defines.push_back(null);

    /**
     * IMPORTANT! Here we load and compile the SMAA effect from a *RESOURCE*
     * (Yeah, we like all-in-one executables for demos =)
     * In case you want it to be loaded from other place change this line accordingly.
     */
    ID3D10IncludeResource includeResource;
    string profile = dx10_1? "fx_4_1" : "fx_4_0";
    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"SMAA.fx", NULL, &defines.front(), &includeResource, profile.c_str(), D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    // This is for rendering the typical fullscreen quad later on:
    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("NeighborhoodBlending")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);
    
    // If storage for the edges is not specified we will create it:
    if (storage.edgesRTV != NULL && storage.edgesSRV != NULL)
        edgesRT = new RenderTarget(device, storage.edgesRTV, storage.edgesSRV);
    else
        edgesRT = new RenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    // Same for blending weights:
    if (storage.weightsRTV != NULL && storage.weightsSRV != NULL)
        blendRT = new RenderTarget(device, storage.weightsRTV, storage.weightsSRV);
    else
        blendRT = new RenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    // Load the pre-computed textures:
    loadAreaTex();
    loadSearchTex();

    // Create some handles for variables:
    thresholdVariable = effect->GetVariableByName("threshld")->AsScalar();
    cornerRoundingVariable = effect->GetVariableByName("cornerRounding")->AsScalar();
    maxSearchStepsVariable = effect->GetVariableByName("maxSearchSteps")->AsScalar();
    maxSearchStepsDiagVariable = effect->GetVariableByName("maxSearchStepsDiag")->AsScalar();
    blendFactorVariable = effect->GetVariableByName("blendFactor")->AsScalar();
    subsampleIndicesVariable = effect->GetVariableByName("subsampleIndices")->AsVector();
    areaTexVariable = effect->GetVariableByName("areaTex")->AsShaderResource();
    searchTexVariable = effect->GetVariableByName("searchTex")->AsShaderResource();
    colorTexVariable = effect->GetVariableByName("colorTex")->AsShaderResource();
    colorTexGammaVariable = effect->GetVariableByName("colorTexGamma")->AsShaderResource();
    colorTexPrevVariable = effect->GetVariableByName("colorTexPrev")->AsShaderResource();
    colorMSTexVariable = effect->GetVariableByName("colorMSTex")->AsShaderResource();
    depthTexVariable = effect->GetVariableByName("depthTex")->AsShaderResource();
    velocityTexVariable = effect->GetVariableByName("velocityTex")->AsShaderResource();
    edgesTexVariable = effect->GetVariableByName("edgesTex")->AsShaderResource();
    blendTexVariable = effect->GetVariableByName("blendTex")->AsShaderResource();

    // Create handles for techniques:
    edgeDetectionTechniques[0] = effect->GetTechniqueByName("LumaEdgeDetection");
    edgeDetectionTechniques[1] = effect->GetTechniqueByName("ColorEdgeDetection");
    edgeDetectionTechniques[2] = effect->GetTechniqueByName("DepthEdgeDetection");
    blendingWeightCalculationTechnique = effect->GetTechniqueByName("BlendingWeightCalculation");
    neighborhoodBlendingTechnique = effect->GetTechniqueByName("NeighborhoodBlending");
    resolveTechnique = effect->GetTechniqueByName("Resolve");
    separateTechnique = effect->GetTechniqueByName("Separate");

    // Detect MSAA 2x subsample order:
    detectMSAAOrder();
}


SMAA::~SMAA() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
    SAFE_DELETE(edgesRT);
    SAFE_DELETE(blendRT);
    SAFE_RELEASE(areaTex);
    SAFE_RELEASE(areaTexSRV);
    SAFE_RELEASE(searchTex);
    SAFE_RELEASE(searchTexSRV);
}


void SMAA::go(ID3D10ShaderResourceView *srcGammaSRV,
              ID3D10ShaderResourceView *srcSRV,
              ID3D10ShaderResourceView *depthSRV,
              ID3D10RenderTargetView *dstRTV,
              ID3D10DepthStencilView *dsv,
              Input input,
              Mode mode,
              int subsampleIndex,
              float blendFactor) {
    HRESULT hr;

    // Save the state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);
    SaveBlendStateScope saveBlendState(device);
    SaveDepthStencilScope saveDepthStencil(device);

    // Reset the render target:
    device->OMSetRenderTargets(0, NULL, NULL);

    // Setup the viewport and the vertex layout:
    edgesRT->setViewport();
    quad->setInputLayout();

    // Clear render targets:
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    device->ClearRenderTargetView(*edgesRT, clearColor);
    device->ClearRenderTargetView(*blendRT, clearColor);

    // Setup variables:
    if (preset == PRESET_CUSTOM) {
        V(thresholdVariable->SetFloat(threshold));
        V(cornerRoundingVariable->SetFloat(cornerRounding));
        V(maxSearchStepsVariable->SetFloat(float(maxSearchSteps)));
        V(maxSearchStepsDiagVariable->SetFloat(float(maxSearchStepsDiag)));
    }
    V(blendFactorVariable->SetFloat(blendFactor));
    V(colorTexVariable->SetResource(srcSRV));
    V(edgesTexVariable->SetResource(*edgesRT));
    V(blendTexVariable->SetResource(*blendRT));
    V(areaTexVariable->SetResource(areaTexSRV));
    V(searchTexVariable->SetResource(searchTexSRV));
    V(colorTexGammaVariable->SetResource(srcGammaSRV));
    V(depthTexVariable->SetResource(depthSRV));

    // And here we go!
    edgesDetectionPass(dsv, input);
    blendingWeightsCalculationPass(dsv, mode, subsampleIndex);
    neighborhoodBlendingPass(dstRTV, dsv);
}


void SMAA::reproject(ID3D10ShaderResourceView *currentSRV,
                     ID3D10ShaderResourceView *previousSRV,
                     ID3D10ShaderResourceView *velocitySRV,
                     ID3D10RenderTargetView *dstRTV) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"SMAA: temporal resolve");
    HRESULT hr;

    // Save the state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);
    SaveBlendStateScope saveBlendState(device);
    SaveDepthStencilScope saveDepthStencil(device);

    // Setup the viewport and the vertex layout:
    edgesRT->setViewport();
    quad->setInputLayout();
    
    // Setup variables:
    V(colorTexVariable->SetResource(currentSRV));
    V(colorTexPrevVariable->SetResource(previousSRV));
    V(velocityTexVariable->SetResource(velocitySRV));

    // Select the technique accordingly:
    V(resolveTechnique->GetPassByIndex(0)->Apply(0));

    // Do it!
    device->OMSetRenderTargets(1, &dstRTV, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    D3DPERF_EndEvent();
}


void SMAA::separate(ID3D10ShaderResourceView *srcSRV,
                    ID3D10RenderTargetView *dst1RTV,
                    ID3D10RenderTargetView *dst2RTV) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"SMAA: separate");
    HRESULT hr;

    // Save the state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);
    SaveBlendStateScope saveBlendState(device);
    SaveDepthStencilScope saveDepthStencil(device);

    // Setup the viewport and the vertex layout:
    edgesRT->setViewport();
    quad->setInputLayout();

    // Setup variables:
    V(colorMSTexVariable->SetResource(srcSRV));

    // Select the technique accordingly:
    V(separateTechnique->GetPassByIndex(0)->Apply(0));

    // Do it!
    ID3D10RenderTargetView *dst[] = { dst1RTV, dst2RTV };
    device->OMSetRenderTargets(2, dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    D3DPERF_EndEvent();
}


void SMAA::loadAreaTex() {
    #ifndef SMAA_TEST_DDS_FILES
    HRESULT hr;

    D3D10_SUBRESOURCE_DATA data;
    data.pSysMem = areaTexBytes;
    data.SysMemPitch = AREATEX_PITCH;
    data.SysMemSlicePitch = 0;

    D3D10_TEXTURE2D_DESC descTex;
    descTex.Width = AREATEX_WIDTH;
    descTex.Height = AREATEX_HEIGHT;
    descTex.MipLevels = descTex.ArraySize = 1;
    descTex.Format = DXGI_FORMAT_R8G8_UNORM;
    descTex.SampleDesc.Count = 1;
    descTex.SampleDesc.Quality = 0;
    descTex.Usage = D3D10_USAGE_DEFAULT;
    descTex.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    descTex.CPUAccessFlags = 0;
    descTex.MiscFlags = 0;
    V(device->CreateTexture2D(&descTex, &data, &areaTex));

    D3D10_SHADER_RESOURCE_VIEW_DESC descSRV;
    descSRV.Format = descTex.Format;
    descSRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MostDetailedMip = 0;
    descSRV.Texture2D.MipLevels = 1;
    V(device->CreateShaderResourceView(areaTex, &descSRV, &areaTexSRV));
    #else
    areaTex = NULL;
    HRESULT hr;
    D3DX10_IMAGE_LOAD_INFO info = D3DX10_IMAGE_LOAD_INFO();
    info.MipLevels = 1;
    info.Format = DXGI_FORMAT_R8G8_UNORM;
    V(D3DX10CreateShaderResourceViewFromFile(device, L"../../Textures/AreaTexDX10.dds", &info, NULL, &areaTexSRV, NULL));
    #endif
}


void SMAA::loadSearchTex() {
    #ifndef SMAA_TEST_DDS_FILES
    HRESULT hr;

    D3D10_SUBRESOURCE_DATA data;
    data.pSysMem = searchTexBytes;
    data.SysMemPitch = SEARCHTEX_PITCH;
    data.SysMemSlicePitch = 0;

    D3D10_TEXTURE2D_DESC descTex;
    descTex.Width = SEARCHTEX_WIDTH;
    descTex.Height = SEARCHTEX_HEIGHT;
    descTex.MipLevels = descTex.ArraySize = 1;
    descTex.Format = DXGI_FORMAT_R8_UNORM;
    descTex.SampleDesc.Count = 1;
    descTex.SampleDesc.Quality = 0;
    descTex.Usage = D3D10_USAGE_DEFAULT;
    descTex.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    descTex.CPUAccessFlags = 0;
    descTex.MiscFlags = 0;
    V(device->CreateTexture2D(&descTex, &data, &searchTex));

    D3D10_SHADER_RESOURCE_VIEW_DESC descSRV;
    descSRV.Format = descTex.Format;
    descSRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MostDetailedMip = 0;
    descSRV.Texture2D.MipLevels = 1;
    V(device->CreateShaderResourceView(searchTex, &descSRV, &searchTexSRV));
    #else
    searchTex = NULL;
    HRESULT hr;
    D3DX10_IMAGE_LOAD_INFO info = D3DX10_IMAGE_LOAD_INFO();
    info.MipLevels = 1;
    info.Format = DXGI_FORMAT_R8_UNORM;
    V(D3DX10CreateShaderResourceViewFromFile(device, L"../../Textures/SearchTex.dds", &info, NULL, &searchTexSRV, NULL));
    #endif
}


void SMAA::edgesDetectionPass(ID3D10DepthStencilView *dsv, Input input) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"SMAA: 1st pass");
    HRESULT hr;

    // Select the technique accordingly:
    V(edgeDetectionTechniques[int(input)]->GetPassByIndex(0)->Apply(0));

    // Do it!
    device->OMSetRenderTargets(1, *edgesRT, dsv);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    D3DPERF_EndEvent();
}


void SMAA::blendingWeightsCalculationPass(ID3D10DepthStencilView *dsv, Mode mode, int subsampleIndex) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"SMAA: 2nd pass");
    HRESULT hr;

    /**
     * Orthogonal indices:
     *     [0]:  0.0
     *     [1]: -0.25
     *     [2]:  0.25
     *     [3]: -0.125
     *     [4]:  0.125
     *     [5]: -0.375
     *     [6]:  0.375
     *
     * Diagonal indices:
     *     [0]:  0.00,   0.00
     *     [1]:  0.25,  -0.25
     *     [2]: -0.25,   0.25
     *     [3]:  0.125, -0.125
     *     [4]: -0.125,  0.125
     *
     * Indices layout: indices[4] = { |, --,  /, \ }
     */

    switch (mode) {
        case MODE_SMAA_1X: {
            int indices[4] = { 0, 0, 0, 0 };
            V(subsampleIndicesVariable->SetIntVector(indices));
            break;
        }
        case MODE_SMAA_T2X:
        case MODE_SMAA_S2X: {
            /***
             * Sample positions (bottom-to-top y axis):
             *   _______
             *  | S1    |  S0:  0.25    -0.25
             *  |       |  S1: -0.25     0.25
             *  |____S0_|
             */
              int indices[][4] = {
                { 1, 1, 1, 0 }, // S0
                { 2, 2, 2, 0 }  // S1
                // (it's 1 for the horizontal slot of S0 because horizontal
                //  blending is reversed: positive numbers point to the right)
            };
            V(subsampleIndicesVariable->SetIntVector(indices[subsampleIndex]));
            break;
        }
        case MODE_SMAA_4X: {
            /***
             * Sample positions (bottom-to-top y axis):
             *   ________
             *  |  S1    |  S0:  0.3750   -0.1250
             *  |      S0|  S1: -0.1250    0.3750
             *  |S3      |  S2:  0.1250   -0.3750
             *  |____S2__|  S3: -0.3750    0.1250
             */
            int indices[][4] = {
                { 5, 3, 1, 3 }, // S0
                { 4, 6, 2, 3 }, // S1
                { 3, 5, 1, 4 }, // S2
                { 6, 4, 2, 4 }  // S3
            };
            V(subsampleIndicesVariable->SetIntVector(indices[subsampleIndex]));
            break;
        }
    }

    // Setup the technique (again):
    V(blendingWeightCalculationTechnique->GetPassByIndex(0)->Apply(0));

    // And here we go!
    device->OMSetRenderTargets(1, *blendRT, dsv);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    D3DPERF_EndEvent();
}


void SMAA::neighborhoodBlendingPass(ID3D10RenderTargetView *dstRTV, ID3D10DepthStencilView *dsv) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"SMAA: 3rd pass");
    HRESULT hr;

    // Setup the technique (once again):
    V(neighborhoodBlendingTechnique->GetPassByIndex(0)->Apply(0));
    
    // Do the final pass!
    device->OMSetRenderTargets(1, &dstRTV, dsv);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    D3DPERF_EndEvent();
}


void SMAA::detectMSAAOrder() {
    HRESULT hr;

    // Create the effect:
    string s = "float4 RenderVS(float4 pos : POSITION,    inout float2 coord : TEXCOORD0) : SV_POSITION { pos.x = -0.5 + 0.5 * pos.x; return pos; }"
               "float4 RenderPS(float4 pos : SV_POSITION,       float2 coord : TEXCOORD0) : SV_TARGET   { return 1.0; }"
               "DepthStencilState DisableDepthStencil { DepthEnable = FALSE; StencilEnable = FALSE; };"
               "BlendState NoBlending { AlphaToCoverageEnable = FALSE; BlendEnable[0] = FALSE; };"
               "technique10 Render { pass Render {"
               "SetVertexShader(CompileShader(vs_4_0, RenderVS())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, RenderPS()));"
               "SetDepthStencilState(DisableDepthStencil, 0);"
               "SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);"
               "}}"
               "Texture2DMS<float4, 2> texMS;"
               "float4 LoadVS(float4 pos : POSITION,    inout float2 coord : TEXCOORD0) : SV_POSITION { return pos; }"
               "float4 LoadPS(float4 pos : SV_POSITION,       float2 coord : TEXCOORD0) : SV_TARGET   { int2 ipos = int2(pos.xy); return texMS.Load(ipos, 0); }"
               "technique10 Load { pass Load {"
               "SetVertexShader(CompileShader(vs_4_0, LoadVS())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, LoadPS()));"
               "SetDepthStencilState(DisableDepthStencil, 0);"
               "SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);"
               "}}";
    ID3D10Effect *effect;
    V(D3DX10CreateEffectFromMemory(s.c_str(), s.length(), NULL, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    // Create the buffers:
    DXGI_SAMPLE_DESC sampleDesc = { 2, 0 };
    RenderTarget *renderTargetMS = new RenderTarget(device, 1, 1, DXGI_FORMAT_R8_UNORM, sampleDesc);
    RenderTarget *renderTarget = new RenderTarget(device, 1, 1, DXGI_FORMAT_R8_UNORM);
    ID3D10Texture2D *stagingTexture = Utils::createStagingTexture(device, *renderTarget);

    // Create a quad:
    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("Render")->GetPassByIndex(0)->GetDesc(&desc));
    Quad *quad = new Quad(device, desc);

    // Save DX state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    // Set viewport and input layout:
    renderTargetMS->setViewport();
    quad->setInputLayout();

    // Render a quad that fills the left half of a 1x1 buffer:
    V(effect->GetTechniqueByName("Render")->GetPassByIndex(0)->Apply(0));
    device->OMSetRenderTargets(1, *renderTargetMS, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    // Load the sample 0 from previous 1x1 buffer:
    V(effect->GetVariableByName("texMS")->AsShaderResource()->SetResource(*renderTargetMS));
    V(effect->GetTechniqueByName("Load")->GetPassByIndex(0)->Apply(0));
    device->OMSetRenderTargets(1, *renderTarget, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    // Copy the sample #0 into CPU memory:
    device->CopyResource(stagingTexture, *renderTarget);
    D3D10_MAPPED_TEXTURE2D map;
    V(stagingTexture->Map(0, D3D10_MAP_READ, 0, &map));
    BYTE value = ((char *) map.pData)[0];
    stagingTexture->Unmap(0);

    // Set the map indices:
    msaaOrderMap[0] = int(value == 255);
    msaaOrderMap[1] = int(value != 255);

    // Release memory
    SAFE_RELEASE(effect);
    SAFE_DELETE(renderTargetMS);
    SAFE_DELETE(renderTarget);
    SAFE_RELEASE(stagingTexture);
    SAFE_DELETE(quad);
}
