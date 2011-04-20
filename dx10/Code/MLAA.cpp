/**
 * Copyright (C) 2010 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2010 Belen Masia (bmasia@unizar.es) 
 * Copyright (C) 2010 Jose I. Echevarria (joseignacioechevarria@gmail.com) 
 * Copyright (C) 2010 Fernando Navarro (fernandn@microsoft.com) 
 * Copyright (C) 2010 Diego Gutierrez (diegog@unizar.es)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the following statement:
 *
 *       "Uses Jimenez's MLAA. Copyright (C) 2010 by Jorge Jimenez, Belen Masia,
 *        Jose I. Echevarria, Fernando Navarro and Diego Gutierrez."
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
#include "MLAA.h"
using namespace std;


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


MLAA::MLAA(ID3D10Device *device, int width, int height, const ExternalStorage &storage)
        : device(device),
          maxSearchSteps(8),
          threshold(0.1f),
          backbufferRenderTarget(NULL),
          stopAtEdgeDetection(false) {
    HRESULT hr;
    
    // Setup the defines for compiling the effect.
    stringstream s;
    s << "float2(1.0 / " << width << ", 1.0 / " << height << ")";
    string pixelSizeText = s.str();

    s.str("");
    s << MAX_DISTANCE;
    string maxDistanceText = s.str();

    D3D10_SHADER_MACRO defines[3] = {
        {"PIXEL_SIZE", pixelSizeText.c_str()},
        {"MAX_DISTANCE", maxDistanceText.c_str()},
        {NULL, NULL}
    };

    /**
     * IMPORTANT! Here we load and compile the MLAA effect from a *RESOURCE*
     * (Yeah, we like all-in-one executables for demos =)
     * In case you want it to be loaded from other place change this line accordingly.
     */
    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"MLAA.fx", NULL, defines, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    // This is for rendering the typical fullscreen quad later on.
    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("NeighborhoodBlending")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);
    
    // If storage for the edges is not specified we will create it.
    if (storage.edgesRTV != NULL && storage.edgesSRV != NULL)
        edgeRenderTarget = new RenderTarget(device, storage.edgesRTV, storage.edgesSRV);
    else
        edgeRenderTarget = new RenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    
    // Same for blending weights.
    if (storage.weightsRTV != NULL && storage.weightsSRV != NULL)
        blendRenderTarget = new RenderTarget(device, storage.weightsRTV, storage.weightsSRV);
    else
        blendRenderTarget = new RenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    edgeRenderSRGBTarget = new RenderTarget(device, *edgeRenderTarget, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB); // This is just a view of the same resource.

    // Load the AreaMap pre-computed texture.
    wstringstream ss;
    ss << L"AreaMap" << MAX_DISTANCE << L".dds";

    D3DX10_IMAGE_LOAD_INFO info = D3DX10_IMAGE_LOAD_INFO();
    info.MipLevels = 1;
    info.Format = DXGI_FORMAT_R8G8_UNORM;
    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), ss.str().c_str(), &info, NULL, &areaMapView, NULL));

    // Create some handles for techniques and variables.
    thresholdVariable = effect->GetVariableByName("threshold")->AsScalar();
    maxSearchStepsVariable = effect->GetVariableByName("maxSearchSteps")->AsScalar();
    areaTexVariable = effect->GetVariableByName("areaTex")->AsShaderResource();
    colorTexVariable = effect->GetVariableByName("colorTex")->AsShaderResource();
    depthTexVariable = effect->GetVariableByName("depthTex")->AsShaderResource();
    edgesTexVariable = effect->GetVariableByName("edgesTex")->AsShaderResource();
    blendTexVariable = effect->GetVariableByName("blendTex")->AsShaderResource();
    colorEdgeDetectionTechnique = effect->GetTechniqueByName("ColorEdgeDetection");
    depthEdgeDetectionTechnique = effect->GetTechniqueByName("DepthEdgeDetection");
    blendWeightCalculationTechnique = effect->GetTechniqueByName("BlendingWeightCalculation");
    neighborhoodBlendingTechnique = effect->GetTechniqueByName("NeighborhoodBlending");
}


MLAA::~MLAA() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
    SAFE_DELETE(edgeRenderTarget);
    SAFE_DELETE(edgeRenderSRGBTarget);
    SAFE_DELETE(blendRenderTarget);
    SAFE_RELEASE(areaMapView);
    SAFE_DELETE(backbufferRenderTarget);
}


void MLAA::go(ID3D10ShaderResourceView *src,
              ID3D10RenderTargetView *dst, 
              ID3D10DepthStencilView *depthStencil, 
              ID3D10ShaderResourceView *depthResource) {
    // Save the state.
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    // Reset the render target.
    device->OMSetRenderTargets(0, NULL, NULL);

    // Setup the viewport and the vertex layout.
    edgeRenderTarget->setViewport();
    quad->setInputLayout();

    // Clear render targets.
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    device->ClearRenderTargetView(*edgeRenderTarget, clearColor);
    device->ClearRenderTargetView(*blendRenderTarget, clearColor);

    /**
     * We first detect edges in 'src' and store them into 'edgeRenderTarget'.
     */
    edgesDetectionPass(src, depthResource, depthStencil);

    /**
     * As we will overwrite edges below, if 'stopAtEdgeDetection' is true we will
     * stop at this point. This is useful for displaying the edge render
     * target for debugging purposes.
     */
    if (stopAtEdgeDetection)
        return;

    /**
     * Using the detected edges in 'edgeRenderTarget', we calculate the weights
     * and store them into 'blendRenderTarget'.
     */
    blendingWeightsCalculationPass(depthStencil);

    /**
     * We copy 'src' into 'edgeRenderSRGBTarget', to use it in the next pass 
     * as: 
     *    a) We cannot sample from 'src' at the same we write to 'dst', as
     *       they are usually the same resource (the backbuffer).
     *    b) We don't need the edges anymore.
     */
    copy(src);

    /**
     * Reading the copied pixel colors from 'edgeRenderSRGBTarget', we blend
     * neighborhood pixels using the weights stored in 'blendRenderTarget'.
     */
    neighborhoodBlendingPass(dst, depthStencil);
}


void MLAA::go(IDXGISwapChain *swapChain, ID3D10DepthStencilView *depthStencil, ID3D10ShaderResourceView *depthResource) {
    if (backbufferRenderTarget == NULL)
        backbufferRenderTarget = new BackbufferRenderTarget(device, swapChain);
    go(*backbufferRenderTarget, *backbufferRenderTarget, depthStencil, depthResource);
}


void MLAA::edgesDetectionPass(ID3D10ShaderResourceView *src, ID3D10ShaderResourceView *depthResource, ID3D10DepthStencilView *depthStencil) {
    HRESULT hr;

    // Setup variables.
    V(thresholdVariable->SetFloat(threshold));
    V(colorTexVariable->SetResource(src));
    V(depthTexVariable->AsShaderResource()->SetResource(depthResource));
    
    // Depending on the resources that are available, select the technique accordingly.
    if (depthResource != NULL) {
        V(depthEdgeDetectionTechnique->GetPassByIndex(0)->Apply(0));
    } else if (src != NULL) {
        V(colorEdgeDetectionTechnique->GetPassByIndex(0)->Apply(0));
    } else
        throw logic_error("unexpected error");

    // Do it!
    device->OMSetRenderTargets(1, *edgeRenderTarget, depthStencil);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void MLAA::blendingWeightsCalculationPass(ID3D10DepthStencilView *depthStencil) {
    HRESULT hr;

    // Setup the variables and the technique (yet again).
    V(maxSearchStepsVariable->SetInt(maxSearchSteps));
    V(edgesTexVariable->SetResource(*edgeRenderTarget));
    V(areaTexVariable->SetResource(areaMapView));
    V(blendWeightCalculationTechnique->GetPassByIndex(0)->Apply(0));

    // And here we go!
    device->OMSetRenderTargets(1, *blendRenderTarget, depthStencil);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void MLAA::neighborhoodBlendingPass(ID3D10RenderTargetView *dst, ID3D10DepthStencilView *depthStencil) {
    HRESULT hr;

    // Blah blah blah
    V(colorTexVariable->SetResource(*edgeRenderSRGBTarget));
    V(blendTexVariable->SetResource(*blendRenderTarget));
    V(neighborhoodBlendingTechnique->GetPassByIndex(0)->Apply(0));
    
    // Yeah! We will finally have the antialiased image :D
    device->OMSetRenderTargets(1, &dst, depthStencil);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void MLAA::copy(ID3D10ShaderResourceView *src) {
    ID3D10Texture2D *srcTexture2D;
    src->GetResource(reinterpret_cast<ID3D10Resource **>(&srcTexture2D));
    device->CopyResource(*edgeRenderSRGBTarget, srcTexture2D);
    srcTexture2D->Release();
}
