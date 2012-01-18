/**
 * Copyright (C) 2010 Jorge Jimenez (jorge@iryoku.com). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
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


#include <DXUT.h>
#include <string>
#include <d3d9.h>
#include "Copy.h"
using namespace std;


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
    V(D3DX10CreateEffectFromMemory(s.c_str(), s.length(), NULL, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("Copy")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);
}


void Copy::release() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
}


void Copy::go(ID3D10ShaderResourceView *srcSRV, ID3D10RenderTargetView *dstRTV, D3D10_VIEWPORT *viewport) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"Copy");

    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    D3D10_VIEWPORT dstViewport = Utils::viewportFromView(dstRTV);
    device->RSSetViewports(1, viewport != NULL? viewport : &dstViewport);

    quad->setInputLayout();
    
    HRESULT hr;
    V(effect->GetVariableByName("tex")->AsShaderResource()->SetResource(srcSRV));
    V(effect->GetTechniqueByName("Copy")->GetPassByIndex(0)->Apply(0));
    device->OMSetRenderTargets(1, &dstRTV, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    D3DPERF_EndEvent();
}
