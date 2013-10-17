/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
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


#include <string>
#include <d3d9.h>
#include "Copy.h"
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
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = nullptr; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p) = nullptr; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif
#pragma endregion


ID3D10Device *Copy::device;
ID3D10Effect *Copy::effect;
Quad *Copy::quad;


void Copy::init(ID3D10Device *device) {
    Copy::device = device;

    string s = "Texture2D tex;"
               "SamplerState PointSampler { Filter = MIN_MAG_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };"
               "float4 VS(float4 pos : POSITION,    inout float2 coord : TEXCOORD0) : SV_POSITION { return pos; }"
               "float4 PS(float4 pos : SV_POSITION,       float2 coord : TEXCOORD0) : SV_TARGET   { return tex.Sample(PointSampler, coord); }"
               "DepthStencilState DisableDepthStencil { DepthEnable = FALSE; StencilEnable = FALSE; };"
               "BlendState NoBlending { AlphaToCoverageEnable = FALSE; BlendEnable[0] = FALSE; };"
               "technique10 Copy { pass Copy {"
               "SetVertexShader(CompileShader(vs_4_0, VS())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, PS()));"
               "SetDepthStencilState(DisableDepthStencil, 0);"
               "SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);"
               "}}";

    HRESULT hr;
    V(D3DX10CreateEffectFromMemory(s.c_str(), s.length(), nullptr, nullptr, nullptr, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, nullptr, nullptr, &effect, nullptr, nullptr));

    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("Copy")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);
}


void Copy::release() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
}


void Copy::go(ID3D10ShaderResourceView *srcSRV, ID3D10RenderTargetView *dstRTV, D3D10_VIEWPORT *viewport) {
    PerfEventScope perfEvent(L"Copy");

    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    D3D10_VIEWPORT dstViewport = Utils::viewportFromView(dstRTV);
    device->RSSetViewports(1, viewport != nullptr? viewport : &dstViewport);

    quad->setInputLayout();
    
    HRESULT hr;
    V(effect->GetVariableByName("tex")->AsShaderResource()->SetResource(srcSRV));
    V(effect->GetTechniqueByName("Copy")->GetPassByIndex(0)->Apply(0));
    device->OMSetRenderTargets(1, &dstRTV, nullptr);
    quad->draw();
    device->OMSetRenderTargets(0, nullptr, nullptr);
}
