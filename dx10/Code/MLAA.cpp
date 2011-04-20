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
#define V(x)           { hr = (x); if( FAILED(hr) ) { DXTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return DXTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#else
#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif    
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=NULL; } }
#endif    
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif
#pragma endregion


MLAA::MLAA(ID3D10Device *device, int width, int height, ID3D10ShaderResourceView *scratch)
        : device(device),
          maxSearchSteps(8),
          threshold(0.1f),
          backbufferRenderTarget(NULL) {
    HRESULT hr;

    stringstream s;
    s << "float2(1.0 / " << width << ", 1.0 / " << height << ")";
    string value = s.str();

    D3D10_SHADER_MACRO defines[2] = {
        {"PIXEL_SIZE", value.c_str()},
        {NULL, NULL}
    };

    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"MLAA.fx", NULL, defines, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("NeighborhoodBlending")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);

    edgeRenderTarget = new RenderTarget(device, width, height, DXGI_FORMAT_R8G8_UNORM);
    blendRenderTarget = new RenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (scratch == NULL) {
        scratchRenderTarget = new RenderTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        scratchView = *scratchRenderTarget;
    } else {
        scratchRenderTarget = NULL;
        scratchView = scratch;
    }

    D3DX10_IMAGE_LOAD_INFO info = D3DX10_IMAGE_LOAD_INFO();
    info.MipLevels = 1;
    info.Format = DXGI_FORMAT_R8G8_UNORM;
    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"AreaMap32.dds", &info, NULL, &areaMapView, NULL));
}


MLAA::~MLAA() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
    SAFE_DELETE(edgeRenderTarget);
    SAFE_DELETE(blendRenderTarget);
    SAFE_DELETE(scratchRenderTarget);
    SAFE_RELEASE(areaMapView);
    SAFE_DELETE(backbufferRenderTarget);
}


void MLAA::go(ID3D10ShaderResourceView *src,
              ID3D10RenderTargetView *dst, 
              ID3D10DepthStencilView *depthStencil, 
              ID3D10ShaderResourceView *depthResource) {

    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    HRESULT hr;
    V(effect->GetVariableByName("areaTex")->AsShaderResource()->SetResource(areaMapView));
    device->OMSetRenderTargets(0, NULL, NULL);

    scratchRenderTarget->setViewport();
    quad->setInputLayout();

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    device->ClearRenderTargetView(*edgeRenderTarget, clearColor);
    device->ClearRenderTargetView(*blendRenderTarget, clearColor);

    /**
     * Here it is the meat of the technique =)
     */
    copy(src);
    edgesDetectionPass(src, depthResource, depthStencil);
    blendingWeightsCalculationPass(depthStencil);
    neighborhoodBlendingPass(scratchView, dst, depthStencil);
}


void MLAA::go(IDXGISwapChain *swapChain, ID3D10DepthStencilView *depthStencil, ID3D10ShaderResourceView *depthResource) {
    if (backbufferRenderTarget == NULL) {
        backbufferRenderTarget = new BackbufferRenderTarget(device, swapChain);
    }
    go(*backbufferRenderTarget, *backbufferRenderTarget, depthStencil, depthResource);
}


void MLAA::copy(ID3D10ShaderResourceView *src) {
    ID3D10Texture2D *srcTexture2D;
    src->GetResource(reinterpret_cast<ID3D10Resource **>(&srcTexture2D));

    ID3D10Texture2D *scratchTexture2D;
    scratchView->GetResource(reinterpret_cast<ID3D10Resource **>(&scratchTexture2D));

    device->CopyResource(scratchTexture2D, srcTexture2D);

    srcTexture2D->Release();
    scratchTexture2D->Release();
}


void MLAA::edgesDetectionPass(ID3D10ShaderResourceView *src, ID3D10ShaderResourceView *depthResource, ID3D10DepthStencilView *depthStencil) {
    HRESULT hr;

    V(effect->GetVariableByName("threshold")->AsScalar()->SetFloat(threshold));
    V(effect->GetVariableByName("colorTex")->AsShaderResource()->SetResource(src));
    V(effect->GetVariableByName("depthTex")->AsShaderResource()->SetResource(depthResource));

    if (depthResource != NULL) {
        V(effect->GetTechniqueByName("EdgeDetectionDepth")->GetPassByIndex(0)->Apply(0));
    } else if (src != NULL) {
        V(effect->GetTechniqueByName("EdgeDetection")->GetPassByIndex(0)->Apply(0));
    } else {
        throw logic_error("unexpected error");
    }

    device->OMSetRenderTargets(1, *edgeRenderTarget, depthStencil);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void MLAA::blendingWeightsCalculationPass(ID3D10DepthStencilView *depthStencil) {
    HRESULT hr;
    V(effect->GetVariableByName("edgesTex")->AsShaderResource()->SetResource(*edgeRenderTarget));
    V(effect->GetTechniqueByName("BlendingWeightCalculation")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, *blendRenderTarget, depthStencil);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void MLAA::neighborhoodBlendingPass(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, ID3D10DepthStencilView *depthStencil) {
    HRESULT hr;
    V(effect->GetVariableByName("maxSearchSteps")->AsScalar()->SetInt(maxSearchSteps));
    V(effect->GetVariableByName("colorTex")->AsShaderResource()->SetResource(src));
    V(effect->GetVariableByName("blendTex")->AsShaderResource()->SetResource(*blendRenderTarget));
    V(effect->GetTechniqueByName("NeighborhoodBlending")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, depthStencil);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}
