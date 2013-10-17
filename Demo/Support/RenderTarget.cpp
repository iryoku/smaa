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


#include <stdexcept>
#include "RenderTarget.h"
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


RenderTarget::RenderTarget(ID3D10Device *device, int width, int height, DXGI_FORMAT format, const DXGI_SAMPLE_DESC &sampleDesc, bool typeless)
        : device(device), width(width), height(height) {
    HRESULT hr;

    D3D10_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = typeless? makeTypeless(format) : format;
    desc.SampleDesc = sampleDesc;
    desc.Usage = D3D10_USAGE_DEFAULT;
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    V(device->CreateTexture2D(&desc, nullptr, &texture2D));
    
    createViews(device, desc, format);
}


RenderTarget::RenderTarget(ID3D10Device *device, ID3D10Texture2D *texture2D, DXGI_FORMAT format)
        : device(device), texture2D(texture2D) {
    D3D10_TEXTURE2D_DESC desc;
    texture2D->GetDesc(&desc);
    width = desc.Width;
    height = desc.Height;

    // Increase the count as it is shared between two RenderTargets.
    texture2D->AddRef();

    createViews(device, desc, format);
}


RenderTarget::RenderTarget(ID3D10Device *device, ID3D10RenderTargetView *renderTargetView, ID3D10ShaderResourceView *shaderResourceView) 
        : device(device), renderTargetView(renderTargetView), shaderResourceView(shaderResourceView) {
    ID3D10Texture2D *t;
    renderTargetView->GetResource(reinterpret_cast<ID3D10Resource **>(&texture2D));
    shaderResourceView->GetResource(reinterpret_cast<ID3D10Resource **>(&t));
    if (t != texture2D) {
        throw logic_error("'renderTargetView' and 'shaderResourceView' ID3D10Texture2Ds should match");
    }
    SAFE_RELEASE(t);

    D3D10_TEXTURE2D_DESC desc;
    texture2D->GetDesc(&desc);
    width = desc.Width;
    height = desc.Height;

    // We are going to safe release later, so increase references.
    renderTargetView->AddRef();
    shaderResourceView->AddRef();
}


void RenderTarget::createViews(ID3D10Device *device, D3D10_TEXTURE2D_DESC desc, DXGI_FORMAT format) {
    HRESULT hr;

    D3D10_RENDER_TARGET_VIEW_DESC rtdesc;
    rtdesc.Format = format;
    if (desc.SampleDesc.Count == 1) {
        rtdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    } else {
        rtdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
    }
    rtdesc.Texture2D.MipSlice = 0;
    V(device->CreateRenderTargetView(texture2D, &rtdesc, &renderTargetView));

    D3D10_SHADER_RESOURCE_VIEW_DESC srdesc;
    srdesc.Format = format;
    if (desc.SampleDesc.Count == 1) {
        srdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    } else {
        srdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
    }
    srdesc.Texture2D.MostDetailedMip = 0;
    srdesc.Texture2D.MipLevels = 1;
    V(device->CreateShaderResourceView(texture2D, &srdesc, &shaderResourceView));
}


RenderTarget::~RenderTarget() {
    SAFE_RELEASE(texture2D);
    SAFE_RELEASE(renderTargetView);
    SAFE_RELEASE(shaderResourceView);
}


void RenderTarget::setViewport(float minDepth, float maxDepth) const {
    D3D10_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = minDepth;
    viewport.MaxDepth = maxDepth;
    device->RSSetViewports(1, &viewport);
}


DXGI_FORMAT RenderTarget::makeTypeless(DXGI_FORMAT format) {
    switch(format) {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
            return DXGI_FORMAT_R8G8B8A8_TYPELESS;

        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC1_UNORM:
            return DXGI_FORMAT_BC1_TYPELESS;
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC2_UNORM:
            return DXGI_FORMAT_BC2_TYPELESS;
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
            return DXGI_FORMAT_BC3_TYPELESS;
    };

    return format;
}


DepthStencil::DepthStencil(ID3D10Device *device, int width, int height, DXGI_FORMAT texture2DFormat,
                           const DXGI_FORMAT depthStencilViewFormat, DXGI_FORMAT shaderResourceViewFormat,
                           const DXGI_SAMPLE_DESC &sampleDesc)
        : device(device), width(width), height(height) {
    HRESULT hr;

    D3D10_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = texture2DFormat;
    desc.SampleDesc = sampleDesc;
    desc.Usage = D3D10_USAGE_DEFAULT;
    if (sampleDesc.Count == 1) {
        desc.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;
    } else {
        desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    }
    V(device->CreateTexture2D(&desc, nullptr, &texture2D));

    D3D10_DEPTH_STENCIL_VIEW_DESC dsdesc;
    dsdesc.Format = depthStencilViewFormat;
    if (sampleDesc.Count == 1) {
        dsdesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
    } else {
        dsdesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
    }
    dsdesc.Texture2D.MipSlice = 0;
    V(device->CreateDepthStencilView(texture2D, &dsdesc, &depthStencilView));

    if (sampleDesc.Count == 1) {
        D3D10_SHADER_RESOURCE_VIEW_DESC srdesc;
        srdesc.Format = shaderResourceViewFormat;
        srdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        srdesc.Texture2D.MostDetailedMip = 0;
        srdesc.Texture2D.MipLevels = 1;
        V(device->CreateShaderResourceView(texture2D, &srdesc, &shaderResourceView));
    } else {
        shaderResourceView = nullptr;
    }
}


DepthStencil::~DepthStencil() {
    SAFE_RELEASE(texture2D);
    SAFE_RELEASE(depthStencilView);
    SAFE_RELEASE(shaderResourceView);
}


void DepthStencil::setViewport(float minDepth, float maxDepth) const {
    D3D10_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = minDepth;
    viewport.MaxDepth = maxDepth;
    device->RSSetViewports(1, &viewport);
}


BackbufferRenderTarget::BackbufferRenderTarget(ID3D10Device *device, IDXGISwapChain *swapChain) {
    HRESULT hr;

    swapChain->GetBuffer(0, __uuidof(texture2D), reinterpret_cast<void**>(&texture2D));

    D3D10_TEXTURE2D_DESC desc;
    texture2D->GetDesc(&desc);

    D3D10_RENDER_TARGET_VIEW_DESC rtdesc;
    rtdesc.Format = desc.Format;
    rtdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    rtdesc.Texture2D.MipSlice = 0;
    V(device->CreateRenderTargetView(texture2D, &rtdesc, &renderTargetView));

    D3D10_SHADER_RESOURCE_VIEW_DESC srdesc;
    srdesc.Format = desc.Format;
    srdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    srdesc.TextureCube.MostDetailedMip = 0;
    srdesc.TextureCube.MipLevels = 1;
    V(device->CreateShaderResourceView(texture2D, &srdesc, &shaderResourceView));

    width = desc.Width;
    height = desc.Height;
}


BackbufferRenderTarget::~BackbufferRenderTarget() {
    SAFE_RELEASE(texture2D);
    SAFE_RELEASE(renderTargetView);
    SAFE_RELEASE(shaderResourceView);
}


struct Vertex {
    D3DXVECTOR3 position;
    D3DXVECTOR2 texcoord;
};


Quad::Quad(ID3D10Device *device, const D3D10_PASS_DESC &desc) 
        : device(device) {
    D3D10_BUFFER_DESC bufferDesc;
    D3D10_SUBRESOURCE_DATA data;
    Vertex vertices[4];

    vertices[0].position = D3DXVECTOR3(-1.0f, -1.0f, 1.0f);
    vertices[1].position = D3DXVECTOR3(-1.0f,  1.0f, 1.0f);
    vertices[2].position = D3DXVECTOR3( 1.0f, -1.0f, 1.0f);
    vertices[3].position = D3DXVECTOR3( 1.0f,  1.0f, 1.0f);

    vertices[0].texcoord = D3DXVECTOR2(0.0f, 1.0f);
    vertices[1].texcoord = D3DXVECTOR2(0.0f, 0.0f);
    vertices[2].texcoord = D3DXVECTOR2(1.0f, 1.0f);
    vertices[3].texcoord = D3DXVECTOR2(1.0f, 0.0f);

    bufferDesc.ByteWidth = sizeof(Vertex) * 4;
    bufferDesc.Usage = D3D10_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;

    data.pSysMem = vertices;
    data.SysMemPitch = 0;
    data.SysMemSlicePitch = 0;

    HRESULT hr;
    V(device->CreateBuffer(&bufferDesc, &data, &buffer));

    const D3D10_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof(layout) / sizeof(D3D10_INPUT_ELEMENT_DESC);

    V(device->CreateInputLayout(layout, numElements, desc.pIAInputSignature, desc.IAInputSignatureSize, &vertexLayout));
}


Quad::~Quad() {
    SAFE_RELEASE(buffer);
    SAFE_RELEASE(vertexLayout);
}


void Quad::draw() {
    const UINT offset = 0;
    const UINT stride = sizeof(Vertex);
    device->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
    device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    device->Draw(4, 0);
}


FullscreenTriangle::FullscreenTriangle(ID3D10Device *device, const D3D10_PASS_DESC &desc) 
        : device(device) {
    D3D10_BUFFER_DESC bufferDesc;
    D3D10_SUBRESOURCE_DATA data;
    Vertex vertices[3];

    vertices[0].position = D3DXVECTOR3(-1.0f, -1.0f, 1.0f);
    vertices[1].position = D3DXVECTOR3(-1.0f,  3.0f, 1.0f);
    vertices[2].position = D3DXVECTOR3( 3.0f, -1.0f, 1.0f);

    vertices[0].texcoord = D3DXVECTOR2(0.0f,  1.0f);
    vertices[1].texcoord = D3DXVECTOR2(0.0f, -1.0f);
    vertices[2].texcoord = D3DXVECTOR2(2.0f,  1.0f);

    bufferDesc.ByteWidth = sizeof(Vertex) * 3;
    bufferDesc.Usage = D3D10_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;

    data.pSysMem = vertices;
    data.SysMemPitch = 0;
    data.SysMemSlicePitch = 0;

    HRESULT hr;
    V(device->CreateBuffer(&bufferDesc, &data, &buffer));

    const D3D10_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof(layout) / sizeof(D3D10_INPUT_ELEMENT_DESC);

    V(device->CreateInputLayout(layout, numElements, desc.pIAInputSignature, desc.IAInputSignatureSize, &vertexLayout));
}


FullscreenTriangle::~FullscreenTriangle() {
    SAFE_RELEASE(buffer);
    SAFE_RELEASE(vertexLayout);
}


void FullscreenTriangle::draw() {
    const UINT offset = 0;
    const UINT stride = sizeof(Vertex);
    device->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
    device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    device->Draw(3, 0);
}


SaveViewportsScope::SaveViewportsScope(ID3D10Device *device) : device(device), numViewports(0) {
    device->RSGetViewports(&numViewports, nullptr);
    if (numViewports > 0) {
        viewports.resize(numViewports);
        device->RSGetViewports(&numViewports, &viewports.front());
    }
}


SaveViewportsScope::~SaveViewportsScope() {
    if (numViewports > 0) {
        device->RSSetViewports(numViewports, &viewports.front());
    }
}


SaveRenderTargetsScope::SaveRenderTargetsScope(ID3D10Device *device) : device(device) {
    device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargets, &depthStencil);
}


SaveRenderTargetsScope::~SaveRenderTargetsScope() {
    device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargets, depthStencil);
    for (int i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
        SAFE_RELEASE(renderTargets[i]);
    }
    SAFE_RELEASE(depthStencil);
}


SaveInputLayoutScope::SaveInputLayoutScope(ID3D10Device *device) : device(device) {
    device->IAGetInputLayout(&inputLayout);
}


SaveInputLayoutScope::~SaveInputLayoutScope() {
    device->IASetInputLayout(inputLayout);
    SAFE_RELEASE(inputLayout);
}


SaveBlendStateScope::SaveBlendStateScope(ID3D10Device *device) : device(device) {
    device->OMGetBlendState(&blendState, blendFactor, &sampleMask);
}


SaveBlendStateScope::~SaveBlendStateScope() {
    device->OMSetBlendState(blendState, blendFactor, sampleMask);
    SAFE_RELEASE(blendState);
}


SaveDepthStencilScope::SaveDepthStencilScope(ID3D10Device *device) : device(device) {
    device->OMGetDepthStencilState(&depthStencilState, &stencilRef);
}


SaveDepthStencilScope::~SaveDepthStencilScope() {
    device->OMSetDepthStencilState(depthStencilState, stencilRef);
    SAFE_RELEASE(depthStencilState);
}


ID3D10Texture2D *Utils::createStagingTexture(ID3D10Device *device, ID3D10Texture2D *texture) {
    HRESULT hr;

    D3D10_TEXTURE2D_DESC texdesc;
    texture->GetDesc(&texdesc);
    texdesc.Usage = D3D10_USAGE_STAGING;
    texdesc.BindFlags = 0;
    texdesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;

    ID3D10Texture2D *stagingTexture;
    V(device->CreateTexture2D(&texdesc, nullptr, &stagingTexture));
    return stagingTexture;
}


D3D10_VIEWPORT Utils::viewportFromView(ID3D10View *view) {
    ID3D10Texture2D *texture2D;
    view->GetResource(reinterpret_cast<ID3D10Resource **>(&texture2D));
    D3D10_VIEWPORT viewport = viewportFromTexture2D(texture2D);
    texture2D->Release();
    return viewport;
}

D3D10_VIEWPORT Utils::viewportFromTexture2D(ID3D10Texture2D *texture2D) {
    D3D10_TEXTURE2D_DESC desc;
    texture2D->GetDesc(&desc);

    D3D10_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = desc.Width;
    viewport.Height = desc.Height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    return viewport;
}
