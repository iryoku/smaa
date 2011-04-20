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
#include "Copy.h"
using namespace std;


ID3D10Device *Copy::device;
ID3D10Effect *Copy::effect;
Quad *Copy::quad;


void Copy::init(ID3D10Device *device) {
    Copy::device = device;

    string s = "Texture2D tex;"
               "SamplerState PointSampler { Filter = MIN_MAG_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };"
               "struct PassV2P { float4 pos : SV_POSITION; float2 coord : TEXCOORD0; };"
               "PassV2P VS(float4 pos: POSITION, float2 coord: TEXCOORD0) { PassV2P o; o.pos = pos; o.coord = coord; return o; }"
               "float4 PS(PassV2P i) : SV_TARGET { return tex.Sample(PointSampler, i.coord); }"
               "DepthStencilState DisableDepthStencil { DepthEnable = FALSE; StencilEnable = FALSE; };"
               "technique10 Copy { pass Copy {"
               "SetVertexShader(CompileShader(vs_4_0, VS())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, PS()));"
               "SetDepthStencilState(DisableDepthStencil, 0);"
               "}}";

    HRESULT hr;
    V(D3DX10CreateEffectFromMemory(s.c_str(), s.length(), NULL, NULL, NULL, "fx_4_0", D3DXFX_NOT_CLONEABLE, 0, device, NULL, NULL, &effect, NULL, NULL));

    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("Copy")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);
}


void Copy::release() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
}


void Copy::go(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, D3D10_VIEWPORT *viewport) {
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    D3D10_VIEWPORT dstViewport = Utils::viewportFromView(dst);
    device->RSSetViewports(1, viewport != NULL? viewport : &dstViewport);

    quad->setInputLayout();
    
    HRESULT hr;
    V(effect->GetVariableByName("tex")->AsShaderResource()->SetResource(src));
    V(effect->GetTechniqueByName("Copy")->GetPassByIndex(0)->Apply(0));
    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}
