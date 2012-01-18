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


#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include <vector>
#include <dxgi.h>
#include <d3d10.h>
#include <d3dx10.h>
#include <dxerr.h>


class NoMSAA : public DXGI_SAMPLE_DESC {
    public:
        inline NoMSAA() { 
            Count = 1;
            Quality = 0;
        }
};


class RenderTarget {
    public:
        RenderTarget(ID3D10Device *device, int width, int height,
            DXGI_FORMAT format,
            const DXGI_SAMPLE_DESC &sampleDesc=NoMSAA(),
            bool typeless=true);

        /**
         * These two are just convenience constructors to build from existing
         * resources.
         */
        RenderTarget(ID3D10Device *device, ID3D10Texture2D *texture2D, DXGI_FORMAT format);
        RenderTarget(ID3D10Device *device,
            ID3D10RenderTargetView *renderTargetView,
            ID3D10ShaderResourceView *shaderResourceView);

        ~RenderTarget();

        operator ID3D10Texture2D * () const { return texture2D; }
        operator ID3D10RenderTargetView * () const { return renderTargetView; }
        operator ID3D10RenderTargetView *const * () const { return &renderTargetView; }
        operator ID3D10ShaderResourceView * () const { return shaderResourceView; }

        int getWidth() const { return width; }
        int getHeight() const { return height; }

        void setViewport(float minDepth=0.0f, float maxDepth=1.0f) const;

        static DXGI_FORMAT makeTypeless(DXGI_FORMAT format);

    private:
        void createViews(ID3D10Device *device, D3D10_TEXTURE2D_DESC desc, DXGI_FORMAT format);

        ID3D10Device *device;
        int width, height;
        ID3D10Texture2D *texture2D;
        ID3D10RenderTargetView *renderTargetView;
        ID3D10ShaderResourceView *shaderResourceView;
};


class DepthStencil {
    public:
        DepthStencil(ID3D10Device *device, int width, int height,
            DXGI_FORMAT texture2DFormat = DXGI_FORMAT_R32_TYPELESS, 
            DXGI_FORMAT depthStencilViewFormat = DXGI_FORMAT_D32_FLOAT, 
            DXGI_FORMAT shaderResourceViewFormat = DXGI_FORMAT_R32_FLOAT,
            const DXGI_SAMPLE_DESC &sampleDesc=NoMSAA());
        ~DepthStencil();

        operator ID3D10Texture2D * const () { return texture2D; }
        operator ID3D10DepthStencilView * const () { return depthStencilView; }
        operator ID3D10ShaderResourceView * const () { return shaderResourceView; }

        int getWidth() const { return width; }
        int getHeight() const { return height; }

        void setViewport(float minDepth=0.0f, float maxDepth=1.0f) const;

    private:
        ID3D10Device *device;
        int width, height;
        ID3D10Texture2D *texture2D;
        ID3D10DepthStencilView *depthStencilView;
        ID3D10ShaderResourceView *shaderResourceView;
};


class BackbufferRenderTarget {
    public:
        BackbufferRenderTarget(ID3D10Device *device, IDXGISwapChain *swapChain);
        ~BackbufferRenderTarget();

        operator ID3D10Texture2D * () const { return texture2D; }
        operator ID3D10RenderTargetView * () const { return renderTargetView; }
        operator ID3D10RenderTargetView *const * () const { return &renderTargetView; }
        operator ID3D10ShaderResourceView * () const { return shaderResourceView; }

        int getWidth() const { return width; }
        int getHeight() const { return height; }

    private:
        int width, height;
        ID3D10Texture2D *texture2D;
        ID3D10RenderTargetView *renderTargetView;
        ID3D10ShaderResourceView *shaderResourceView;
};


class Quad {
    public:
        Quad(ID3D10Device *device, const D3D10_PASS_DESC &desc);
        ~Quad();
        void setInputLayout() { device->IASetInputLayout(vertexLayout); }
        void draw();

    private:
        ID3D10Device *device;
        ID3D10Buffer *buffer;
        ID3D10InputLayout *vertexLayout;
};


class SaveViewportsScope {
    public: 
        SaveViewportsScope(ID3D10Device *device);
        ~SaveViewportsScope();

    private:
        ID3D10Device *device;
        UINT numViewports;
        std::vector<D3D10_VIEWPORT> viewports;
};


class SaveRenderTargetsScope {
    public: 
        SaveRenderTargetsScope(ID3D10Device *device);
        ~SaveRenderTargetsScope();

    private:
        ID3D10Device *device;
        ID3D10RenderTargetView *renderTargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
        ID3D10DepthStencilView *depthStencil;
};


class SaveInputLayoutScope {
    public: 
        SaveInputLayoutScope(ID3D10Device *device);
        ~SaveInputLayoutScope();

    private:
        ID3D10Device *device;
        ID3D10InputLayout *inputLayout;
};


class SaveBlendStateScope {
    public:
        SaveBlendStateScope(ID3D10Device *device);
        ~SaveBlendStateScope();

    private:
        ID3D10Device *device;
        ID3D10BlendState *blendState;
        FLOAT blendFactor[4];
        UINT sampleMask;
};


class SaveDepthStencilScope {
    public:
        SaveDepthStencilScope(ID3D10Device *device);
        ~SaveDepthStencilScope();

    private:
        ID3D10Device *device;
        ID3D10DepthStencilState *depthStencilState;
        UINT stencilRef;
};


class Utils {
    public:
        static ID3D10Texture2D *createStagingTexture(ID3D10Device *device, ID3D10Texture2D *texture);
        static D3D10_VIEWPORT viewportFromView(ID3D10View *view);
        static D3D10_VIEWPORT viewportFromTexture2D(ID3D10Texture2D *texture2D);
};

#endif
