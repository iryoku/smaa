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
#include "RenderTarget.h"


class MLAA {
    public:
        class ExternalStorage;

        /**
         * If you have one or two spare DXGI_FORMAT_R8G8B8A8_UNORM render
         * targets of the same size as the backbuffer, you may want to pass
         * them in the 'storage' parameter. You may pass one or the two,
         * depending on what you have available.
         *
         * By default, two render targets will be created for storing
         * intermediate calculations.
         */
        MLAA(ID3D10Device *device, int width, int height, const ExternalStorage &storage=ExternalStorage());
        ~MLAA();

        /**
         * MLAA processing using color-based edge detection
         * ************************************************
         *
         * 'src' should be a sRGB view of the src texture, for blending the
         * neighborhood of each pixel in linear space. Note that 'src' and
         * 'dst' can be associated to the same buffer.
         *
         * IMPORTANT: The stencil component of 'depthStencil' is used to mask
         * the zones to be processed. It is assumed to be already cleared to 
         * zero when this function is called. It is not done here because it is
         * usually cleared together with the depth.
         *
         * The viewport, the binded render target and the input layout is saved
         * and restored accordingly. However, we modify but not restore the
         * depth-stencil and blend states.
         */
        void go(ID3D10ShaderResourceView *src,
                ID3D10RenderTargetView *dst,
                ID3D10DepthStencilView *depthStencil) { go(src, dst, depthStencil, NULL); }

        /**
         * MLAA processing using depth-based edge detection
         * ************************************************
         *
         * Same as above, but in this case a depth-based edge detection will be
         * performed.
         *
         * 'depthResource' should contain the linearized depth buffer to be
         * used for edge detection. Some people seem to work better with
         * non-linearized buffers, so you may want to try that as well.
         */
        void go(ID3D10ShaderResourceView *src,
                ID3D10RenderTargetView *dst,
                ID3D10DepthStencilView *depthStencil,
                ID3D10ShaderResourceView *depthResource);

        /**
         * MLAA processing using color-based edge detection (Backbuffer Version)
         * *********************************************************************
         *
         * This is a convenience function, that uses the backbuffer as 'src'
         * and 'dst'.
         */
        void go(IDXGISwapChain *swapChain, ID3D10DepthStencilView *depthStencil) { go(swapChain, depthStencil, NULL); }

        /**
         * MLAA processing using depth-based edge detection (Backbuffer Version)
         * *********************************************************************
         *
         * This is a convenience function, that uses the backbuffer as 'src'
         * and 'dst'.
         */
        void go(IDXGISwapChain *swapChain, ID3D10DepthStencilView *depthStencil, ID3D10ShaderResourceView *depthResource);

        /**
         * Maximum length to search for patterns. Each step is two pixels wide.
         */
        int getMaxSearchSteps() const { return maxSearchSteps; }
        void setMaxSearchSteps(int maxSearchSteps) { this->maxSearchSteps = maxSearchSteps; }
        
        /**
         * Threshold for the edge detection.
         */
        float getThreshold() const { return threshold; }
        void setThreshold(float threshold) { this->threshold = threshold; }

        /**
         * These two are just for debugging purposes. See also
         * 'setStopAtEdgeDetection' below.
         */
        RenderTarget *getEdgeRenderTarget() { return edgeRenderTarget; }
        RenderTarget *getBlendRenderTarget() { return blendRenderTarget; }

        /**
         * This class allows to pass spare storage buffers to the MLAA class.
         */
        class ExternalStorage {
            public:
                ExternalStorage(ID3D10ShaderResourceView *edgesSRV=NULL,
                                ID3D10RenderTargetView *edgesRTV=NULL,
                                ID3D10ShaderResourceView *weightsSRV=NULL,
                                ID3D10RenderTargetView *weightsRTV=NULL)
                    : edgesSRV(edgesSRV),
                      edgesRTV(edgesRTV), 
                      weightsSRV(weightsSRV),
                      weightsRTV(weightsRTV) {}

            ID3D10ShaderResourceView *edgesSRV, *weightsSRV;
            ID3D10RenderTargetView *edgesRTV, *weightsRTV;
        };

        /**
         * 'stopAtEdgeDetection' should be 'true' if you are going to use
         * 'getEdgeRenderTarget' for debugging purposes, and 'false' otherwise.
         *  It defaults to 'false'. See 'MLAAQ::go' body for more details.
         */
        void setStopAtEdgeDetection(bool stopAtEdgeDetection) { this->stopAtEdgeDetection = stopAtEdgeDetection; };
        bool getStopAtEdgeDetection() const { return stopAtEdgeDetection; }

    private:
        void edgesDetectionPass(ID3D10ShaderResourceView *src, ID3D10ShaderResourceView *depth, ID3D10DepthStencilView *depthStencil);
        void blendingWeightsCalculationPass(ID3D10DepthStencilView *depthStencil);
        void neighborhoodBlendingPass(ID3D10RenderTargetView *dst, ID3D10DepthStencilView *depthStencil);
        void copy(ID3D10ShaderResourceView *src);

        static const int MAX_DISTANCE = 33; // We have precomputed textures for 9, 17, 33, 65 and 129.

        ID3D10Device *device;
        ID3D10Effect *effect;
        Quad *quad;

        RenderTarget *edgeRenderTarget;
        RenderTarget *edgeRenderSRGBTarget;
        RenderTarget *blendRenderTarget;
        BackbufferRenderTarget *backbufferRenderTarget;
        ID3D10ShaderResourceView *areaMapView;

        ID3D10EffectScalarVariable *thresholdVariable;
        ID3D10EffectScalarVariable *maxSearchStepsVariable;
        ID3D10EffectShaderResourceVariable *areaTexVariable,
                                           *colorTexVariable, *depthTexVariable,
                                           *edgesTexVariable, *blendTexVariable;
        
        ID3D10EffectTechnique *colorEdgeDetectionTechnique,
                              *depthEdgeDetectionTechnique,
                              *blendWeightCalculationTechnique,
                              *neighborhoodBlendingTechnique;

        int maxSearchSteps;
        float threshold;

        bool stopAtEdgeDetection;
};

#endif
