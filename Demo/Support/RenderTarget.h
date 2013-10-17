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


#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include <vector>
#include <d3d10.h>
#include <d3dx10.h>
#include <d3d9.h>
#include <dxerr.h>
#include <dxgi.h>


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


class FullscreenTriangle {
    public:
        FullscreenTriangle(ID3D10Device *device, const D3D10_PASS_DESC &desc);
        ~FullscreenTriangle();
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


class PerfEventScope {
    public:
        PerfEventScope(const std::wstring &eventName) { D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), eventName.c_str()); }
        ~PerfEventScope() { D3DPERF_EndEvent(); }
};


class Utils {
    public:
        static ID3D10Texture2D *createStagingTexture(ID3D10Device *device, ID3D10Texture2D *texture);
        static D3D10_VIEWPORT viewportFromView(ID3D10View *view);
        static D3D10_VIEWPORT viewportFromTexture2D(ID3D10Texture2D *texture2D);
};

#endif
