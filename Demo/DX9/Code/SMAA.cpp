/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
 * Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
 * Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
 * Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. As clarification, there
 * is no requirement that the copyright notice and permission be included in
 * binary distributions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <sstream>
#include <vector>
#include "AreaTex.h"
#include "SearchTex.h"
#include "SMAA.h"
using namespace std;


// This define is for using the precomputed textures DDS files instead of the
// headers:
#define SMAA_USE_DDS_PRECOMPUTED_TEXTURES 1


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
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = nullptr; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p) = nullptr; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif
#pragma endregion

#pragma region This stuff is for loading headers from resources
class ID3D10IncludeResource : public ID3DXInclude {
    public:
        STDMETHOD(Open)(THIS_ D3DXINCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID *ppData, UINT *pBytes)  {
            wstringstream s;
            s << pFileName;
            HRSRC src = FindResource(GetModuleHandle(nullptr), s.str().c_str(), RT_RCDATA);
            HGLOBAL res = LoadResource(GetModuleHandle(nullptr), src);

            *pBytes = SizeofResource(GetModuleHandle(nullptr), src);
            *ppData = (LPCVOID) LockResource(res);

            return S_OK;
        }

        STDMETHOD(Close)(THIS_ LPCVOID)  {
            return S_OK;
        }
};
#pragma endregion


SMAA::SMAA(IDirect3DDevice9 *device, int width, int height, Preset preset, const ExternalStorage &storage)
        : device(device),
          threshold(0.1f),
          cornerRounding(0.25),
          maxSearchSteps(16),
          maxSearchStepsDiag(8),
          width(width), height(height) {
    HRESULT hr;

    // Setup the defines for compiling the effect.
    vector<D3DXMACRO> defines;
    stringstream s;

    // Setup pixel size macro
    s << "float4(1.0 / " << width << ", 1.0 / " << height << ", " << width << ", " << height << ")";
    string pixelSizeText = s.str();
    D3DXMACRO renderTargetMetricsMacro = { "SMAA_RT_METRICS", pixelSizeText.c_str() };
    defines.push_back(renderTargetMetricsMacro);

    // Setup preset macro
    D3DXMACRO presetMacros[] = {
        { "SMAA_PRESET_LOW", nullptr },
        { "SMAA_PRESET_MEDIUM", nullptr },
        { "SMAA_PRESET_HIGH", nullptr },
        { "SMAA_PRESET_ULTRA", nullptr },
        { "SMAA_PRESET_CUSTOM", nullptr }
    };
    defines.push_back(presetMacros[int(preset)]);

    D3DXMACRO null = { nullptr, nullptr };
    defines.push_back(null);

    // Setup the flags for the effect.
    DWORD flags = D3DXFX_NOT_CLONEABLE;
    #ifdef D3DXFX_LARGEADDRESS_HANDLE
    flags |= D3DXFX_LARGEADDRESSAWARE;
    #endif

    /**
     * IMPORTANT! Here we load and compile the SMAA effect from a *RESOURCE*
     * (Yeah, we like all-in-one executables for demos =)
     * In case you want it to be loaded from other place change this line accordingly.
     */
    ID3D10IncludeResource includeResource;
    V(D3DXCreateEffectFromResource(device, nullptr, L"SMAA.fx", &defines.front(), &includeResource, flags, nullptr, &effect, nullptr));

    // Vertex declaration for rendering the typical fullscreen quad later on.
    const D3DVERTEXELEMENT9 vertexElements[3] = {
        { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
        D3DDECL_END()
    };
    V(device->CreateVertexDeclaration(vertexElements , &vertexDeclaration));

    // If storage for the edges is not specified we will create it.
    if (storage.edgeTex != nullptr && storage.edgeSurface != nullptr) {
        edgeTex = storage.edgeTex;
        edgeSurface = storage.edgeSurface;
        releaseEdgeResources = false;
    } else {
        V(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &edgeTex, nullptr));
        V(edgeTex->GetSurfaceLevel(0, &edgeSurface));
        releaseEdgeResources = true;
    }

    // Same for blending weights.
    if (storage.blendTex != nullptr && storage.blendSurface != nullptr) {
        blendTex = storage.blendTex;
        blendSurface = storage.blendSurface;
        releaseBlendResources = false;
    } else {
        V(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &blendTex, nullptr));
        V(blendTex->GetSurfaceLevel(0, &blendSurface));
        releaseBlendResources = true;
    }

    // Load the precomputed textures.
    loadAreaTex();
    loadSearchTex();

    // Create some handles for techniques and variables.
    thresholdHandle = effect->GetParameterByName(NULL, "threshld");
    maxSearchStepsHandle = effect->GetParameterByName(NULL, "maxSearchSteps");
    maxSearchStepsDiagHandle = effect->GetParameterByName(NULL, "maxSearchStepsDiag");
    cornerRoundingHandle = effect->GetParameterByName(NULL, "cornerRounding");
    areaTexHandle = effect->GetParameterByName(NULL, "areaTex2D");
    searchTexHandle = effect->GetParameterByName(NULL, "searchTex2D");
    colorTexHandle = effect->GetParameterByName(NULL, "colorTex2D");
    depthTexHandle = effect->GetParameterByName(NULL, "depthTex2D");
    edgesTexHandle = effect->GetParameterByName(NULL, "edgesTex2D");
    blendTexHandle = effect->GetParameterByName(NULL, "blendTex2D");
    lumaEdgeDetectionHandle = effect->GetTechniqueByName("LumaEdgeDetection");
    colorEdgeDetectionHandle = effect->GetTechniqueByName("ColorEdgeDetection");
    depthEdgeDetectionHandle = effect->GetTechniqueByName("DepthEdgeDetection");
    blendWeightCalculationHandle = effect->GetTechniqueByName("BlendWeightCalculation");
    neighborhoodBlendingHandle = effect->GetTechniqueByName("NeighborhoodBlending");
}


SMAA::~SMAA() {
    SAFE_RELEASE(effect);
    SAFE_RELEASE(vertexDeclaration);

    if (releaseEdgeResources) { // We will be releasing these things *only* if we created them.
        SAFE_RELEASE(edgeTex);
        SAFE_RELEASE(edgeSurface);
    }

    if (releaseBlendResources) { // Same applies over here.
        SAFE_RELEASE(blendTex);
        SAFE_RELEASE(blendSurface);
    }

    SAFE_RELEASE(areaTex);
    SAFE_RELEASE(searchTex);
}


void SMAA::go(IDirect3DTexture9 *edges,
              IDirect3DTexture9 *src, 
              IDirect3DSurface9 *dst,
              Input input) {
    HRESULT hr;

    // Setup the layout for our fullscreen quad.
    V(device->SetVertexDeclaration(vertexDeclaration));

    // And here we go!
    edgesDetectionPass(edges, input); 
    blendingWeightsCalculationPass();
    neighborhoodBlendingPass(src, dst);
}


void SMAA::loadAreaTex() {
    #if SMAA_USE_DDS_PRECOMPUTED_TEXTURES
    HRESULT hr;
    D3DXIMAGE_INFO info;
    V(D3DXGetImageInfoFromResource(GetModuleHandle(nullptr), L"AreaTexDX9.dds", &info));
    V(D3DXCreateTextureFromResourceEx(device, GetModuleHandle(nullptr), L"AreaTexDX9.dds", info.Width, info.Height, 1, 0, D3DFMT_A8L8, D3DPOOL_DEFAULT, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, &info, nullptr, &areaTex));
    #else
    HRESULT hr;
    V(device->CreateTexture(AREATEX_WIDTH, AREATEX_HEIGHT, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8L8, D3DPOOL_DEFAULT, &areaTex, nullptr));
    D3DLOCKED_RECT rect;
    V(areaTex->LockRect(0, &rect, nullptr, D3DLOCK_DISCARD));
    for (int i = 0; i < AREATEX_HEIGHT; i++)
        CopyMemory(((char *) rect.pBits) + i * rect.Pitch, areaTexBytes + i * AREATEX_PITCH, AREATEX_PITCH);
    V(areaTex->UnlockRect(0));
    #endif
}


void SMAA::loadSearchTex() {
    #if SMAA_USE_DDS_PRECOMPUTED_TEXTURES
    HRESULT hr;
    D3DXIMAGE_INFO info;
    V(D3DXGetImageInfoFromResource(GetModuleHandle(nullptr), L"SearchTex.dds", &info));
    V(D3DXCreateTextureFromResourceEx(device, GetModuleHandle(nullptr), L"SearchTex.dds", info.Width, info.Height, 1, 0, D3DFMT_L8, D3DPOOL_DEFAULT, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, &info, nullptr, &searchTex));
    #else
    HRESULT hr;
    V(device->CreateTexture(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &searchTex, nullptr));
    D3DLOCKED_RECT rect;
    V(searchTex->LockRect(0, &rect, nullptr, D3DLOCK_DISCARD));
    for (int i = 0; i < SEARCHTEX_HEIGHT; i++)
        CopyMemory(((char *) rect.pBits) + i * rect.Pitch, searchTexBytes + i * SEARCHTEX_PITCH, SEARCHTEX_PITCH);
    V(searchTex->UnlockRect(0));
    #endif
}


void SMAA::edgesDetectionPass(IDirect3DTexture9 *edges, Input input) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"SMAA: 1st pass");
    HRESULT hr;

    // Set the render target and clear both the color and the stencil buffers.
    V(device->SetRenderTarget(0, edgeSurface));
    V(device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0));

    // Setup variables.
    V(effect->SetFloat(thresholdHandle, threshold));
    V(effect->SetFloat(maxSearchStepsHandle, float(maxSearchSteps)));
    V(effect->SetFloat(maxSearchStepsDiagHandle, float(maxSearchStepsDiag)));
    V(effect->SetFloat(cornerRoundingHandle, cornerRounding));

    // Select the technique accordingly.
    switch (input) {
        case INPUT_LUMA:
            V(effect->SetTexture(colorTexHandle, edges));
            V(effect->SetTechnique(lumaEdgeDetectionHandle));
            break;
        case INPUT_COLOR:
            V(effect->SetTexture(colorTexHandle, edges));
            V(effect->SetTechnique(colorEdgeDetectionHandle));
            break;
        case INPUT_DEPTH:
            V(effect->SetTexture(depthTexHandle, edges));
            V(effect->SetTechnique(depthEdgeDetectionHandle));
            break;
        default:
            throw logic_error("unexpected error");
    }

    // Do it!
    UINT passes;
    V(effect->Begin(&passes, 0));
    V(effect->BeginPass(0));
    quad(width, height);
    V(effect->EndPass());
    V(effect->End());

    D3DPERF_EndEvent();
}


void SMAA::blendingWeightsCalculationPass() {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"SMAA: 2nd pass");
    HRESULT hr;

    // Set the render target and clear it.
    V(device->SetRenderTarget(0, blendSurface));
    V(device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0));

    // Setup the variables and the technique (yet again).
    V(effect->SetTexture(edgesTexHandle, edgeTex));
    V(effect->SetTexture(areaTexHandle, areaTex));
    V(effect->SetTexture(searchTexHandle, searchTex));
    V(effect->SetTechnique(blendWeightCalculationHandle));

    // And here we go!
    UINT passes;
    V(effect->Begin(&passes, 0));
    V(effect->BeginPass(0));
    quad(width, height);
    V(effect->EndPass());
    V(effect->End());

    D3DPERF_EndEvent();
}


void SMAA::neighborhoodBlendingPass(IDirect3DTexture9 *src, IDirect3DSurface9 *dst) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"SMAA: 3rd pass");
    HRESULT hr;

    // Blah blah blah
    V(device->SetRenderTarget(0, dst));
    V(effect->SetTexture(colorTexHandle, src));
    V(effect->SetTexture(blendTexHandle, blendTex));
    V(effect->SetTechnique(neighborhoodBlendingHandle));

    // Yeah! We will finally have the antialiased image :D
    UINT passes;
    V(effect->Begin(&passes, 0));
    V(effect->BeginPass(0));
    quad(width, height);
    V(effect->EndPass());
    V(effect->End());

    D3DPERF_EndEvent();
}


void SMAA::quad(int width, int height) {
    // Typical aligned fullscreen quad.
    HRESULT hr;
    D3DXVECTOR2 pixelSize = D3DXVECTOR2(1.0f / float(width), 1.0f / float(height));
    float quad[4][5] = {
        { -1.0f - pixelSize.x,  1.0f + pixelSize.y, 0.5f, 0.0f, 0.0f },
        {  1.0f - pixelSize.x,  1.0f + pixelSize.y, 0.5f, 1.0f, 0.0f },
        { -1.0f - pixelSize.x, -1.0f + pixelSize.y, 0.5f, 0.0f, 1.0f },
        {  1.0f - pixelSize.x, -1.0f + pixelSize.y, 0.5f, 1.0f, 1.0f }
    };
    V(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])));
}

