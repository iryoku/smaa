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

#ifndef MLAA_H
#define MLAA_H

#include <dxgi.h>
#include <d3d10.h>
#include <d3dx10.h>
#include "RenderTarget.h"


class MLAA {
    public:
        /**
         * If you have a spare render target of the same size and format as the
         * backbuffer you may want to pass it as the 'scratch' parameter.
         * Otherwise, one will be created for the intemediate calculations.
         */
        MLAA(ID3D10Device *device, int width, int height, ID3D10ShaderResourceView *scratch=NULL);
        ~MLAA();

        /**
         * MLAA processing using color-based edge detection.
         * *************************************************
         *
         * 'src' should be a sRGB view of the src texture, for blending the
         * neighborhood of each pixel in linear space.
         *
         * The stencil component of 'depthStencil' is used to mask the zones to
         * be processed. It is assumed to be already cleared when this function
         * is called. It is not done here because it is usually cleared
         * together with the depth.
         */
        void go(ID3D10ShaderResourceView *src,
                ID3D10RenderTargetView *dst,
                ID3D10DepthStencilView *depthStencil) { go(src, dst, depthStencil, NULL); }

        /**
         * MLAA processing using depth-based edge detection.
         * *************************************************
         *
         * 'src' should be a sRGB view of the src texture, for blending the
         * neighborhood of each pixel in linear space.
         *
         * The stencil component of 'depthStencil' is used to mask the zones to
         * be processed. It is assumed to be already cleared when this function
         * is called. It is not done here because it is usually cleared
         * together with the depth.
         *
         * 'depthResource' should contain the linearized depth buffer to be used
         * for edge detection.
         */
        void go(ID3D10ShaderResourceView *src,
                ID3D10RenderTargetView *dst,
                ID3D10DepthStencilView *depthStencil,
                ID3D10ShaderResourceView *depthResource);

        /**
         * MLAA processing using color-based edge detection.
         * *************************************************
         *                                Backbuffer Version
         *
         * This is a convenience function, that uses the backbuffer as 'src'
         * and 'dst'.
         */
        void go(IDXGISwapChain *swapChain, ID3D10DepthStencilView *depthStencil) { go(swapChain, depthStencil, NULL); }

        /**
         * MLAA processing using depth-based edge detection.
         * *************************************************
         *                                Backbuffer Version
         *
         * This is a convenience function, that uses the backbuffer as 'src'
         * and 'dst'.
         */
        void go(IDXGISwapChain *swapChain, ID3D10DepthStencilView *depthStencil, ID3D10ShaderResourceView *depthResource);


        int getMaxSearchSteps() const { return maxSearchSteps; }
        void setMaxSearchSteps(int maxSearchSteps) { this->maxSearchSteps = maxSearchSteps; }

        float getThreshold() const { return threshold; }
        void setThreshold(float threshold) { this->threshold = threshold; }

        RenderTarget *getEdgeRenderTarget() { return edgeRenderTarget; }
        RenderTarget *getBlendRenderTarget() { return blendRenderTarget; }


    private:
        void copy(ID3D10ShaderResourceView *srgbSrc);
        void edgesDetectionPass(ID3D10ShaderResourceView *src, ID3D10ShaderResourceView *depth, ID3D10DepthStencilView *depthStencil);
        void blendingWeightsCalculationPass(ID3D10DepthStencilView *depthStencil);
        void neighborhoodBlendingPass(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, ID3D10DepthStencilView *depthStencil);

        ID3D10Device *device;
        ID3D10Effect *effect;
        Quad *quad;
        RenderTarget *edgeRenderTarget;
        RenderTarget *blendRenderTarget;
        ID3D10ShaderResourceView *areaMapView;
        
        RenderTarget *scratchRenderTarget;
        ID3D10ShaderResourceView *scratchView;

        BackbufferRenderTarget *backbufferRenderTarget;

        int maxSearchSteps;
        float threshold;
};

#endif
