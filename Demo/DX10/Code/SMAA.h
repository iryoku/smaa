/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
 * Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
 * Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
 * Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)
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


#ifndef SMAA_H
#define SMAA_H

#include <dxgi.h>
#include <d3d10.h>
#include "RenderTarget.h"

/**
 * IMPORTANT NOTICE: please note that the documentation given in this file is
 * rather limited. We recommend first checking out SMAA.h in the root directory
 * of the source release (the shader header), then coming back here. This is
 * just a C++ interface to the shader.
 */


class SMAA {
    public:
        class ExternalStorage;

        enum Mode { MODE_SMAA_1X, MODE_SMAA_T2X, MODE_SMAA_S2X, MODE_SMAA_4X, MODE_SMAA_COUNT=MODE_SMAA_4X };
        enum Preset { PRESET_LOW, PRESET_MEDIUM, PRESET_HIGH, PRESET_ULTRA, PRESET_CUSTOM, PRESET_COUNT=PRESET_CUSTOM };
        enum Input { INPUT_LUMA, INPUT_COLOR, INPUT_DEPTH, INPUT_COUNT=INPUT_DEPTH };

        /**
         * By default, two render targets will be created for storing
         * intermediate calculations. If you have spare render targets,
         * search for @EXTERNAL_STORAGE.
         */
        SMAA(ID3D10Device *device, int width, int height, 
             Preset preset=PRESET_HIGH, bool predication=false, bool reprojection=false, const DXGI_ADAPTER_DESC *adapterDesc=NULL,
             const ExternalStorage &storage=ExternalStorage());
        ~SMAA();

        /**
         * Mandatory input textures varies depending on 'input':
         *    INPUT_LUMA:
         *    INPUT_COLOR:
         *        go(srcGammaSRV, srcSRV, nullptr,  depthSRV, dsv)
         *    INPUT_DEPTH:
         *        go(nullptr,     srcSRV, depthSRV, depthSRV, dsv)
         *
         * You can safely pass everything (do not use NULLs) if you want, the
         * extra paramters will be ignored accordingly. See descriptions below.
         *
         * Input texture 'depthSRV' will be used for predication if enabled. We
         * recommend using a light accumulation buffer or object ids, if
         * available; it'll probably yield better results.
         *
         * To ease implementation, everything is saved (blend state, viewport,
         * etc.), you may want to check Save*Scope in the implementation if you
         * don't need this.
         *
         * IMPORTANT: The stencil component of 'dsv' is used to mask zones to
         * be processed. It is assumed to be already cleared to zero when this
         * function is called. It is not done here because it is usually
         * cleared together with the depth.
         */
        void go(ID3D10ShaderResourceView *srcGammaSRV, // Non-SRGB version of the input color texture.
                ID3D10ShaderResourceView *srcSRV, // SRGB version of the input color texture.
                ID3D10ShaderResourceView *depthSRV, // Input depth texture.
                ID3D10ShaderResourceView *velocitySRV, // Input velocity texture, if reproject is going to be called later on, nullptr otherwise.
                ID3D10RenderTargetView *dstRTV, // Output render target.
                ID3D10DepthStencilView *dsv, // Depth-stencil buffer for optimizations.
                Input input, // Selects the input for edge detection.
                Mode mode=MODE_SMAA_1X, // Selects the SMAA mode.
                int pass=0); // Selects the S2x or 4x pass (either 0 or 1).

        /**
         * This function perform a temporal resolve of two buffers. They must
         * contain temporary jittered color subsamples.
         */
        void reproject(ID3D10ShaderResourceView *currentSRV,
                       ID3D10ShaderResourceView *previousSRV,
                       ID3D10ShaderResourceView *velocitySRV,
                       ID3D10RenderTargetView *dstRTV);

        /**
         * This function separates 2 subsamples in a 2x multisampled buffer
         * (srcSRV) into two different render targets.
         */
        void separate(ID3D10ShaderResourceView *srcSRV,
                      ID3D10RenderTargetView *dst1RTV,
                      ID3D10RenderTargetView *dst2RTV);

        /**
         * Reorders the subsample indices to match the standard
         * D3D1*_STANDARD_MULTISAMPLE_PATTERN arrangement.
         * See related SMAA::detectMSAAOrder.
         */
        int msaaReorder(int sample) const { return msaaOrderMap[sample]; }

        /**
         * Gets the render target size the object operates on.
         */
        int getWidth() const { return width; }
        int getHeight() const { return height; }

        /**
         * Threshold for the edge detection. Only has effect if PRESET_CUSTOM
         * is selected.
         */
        float getThreshold() const { return threshold; }
        void setThreshold(float threshold) { this->threshold = threshold; }

        /**
         * Maximum length to search for horizontal/vertical patterns. Each step
         * is two pixels wide. Only has effect if PRESET_CUSTOM is selected.
         */
        int getMaxSearchSteps() const { return maxSearchSteps; }
        void setMaxSearchSteps(int maxSearchSteps) { this->maxSearchSteps = maxSearchSteps; }

        /**
         * Maximum length to search for diagonal patterns. Only has effect if
         * PRESET_CUSTOM is selected.
         */
        int getMaxSearchStepsDiag() const { return maxSearchStepsDiag; }
        void setMaxSearchStepsDiag(int maxSearchStepsDiag) { this->maxSearchStepsDiag = maxSearchStepsDiag; }

        /**
         * Desired corner rounding, from 0.0 (no rounding) to 100.0 (full
         * rounding). Only has effect if PRESET_CUSTOM is selected.
         */
        float getCornerRounding() const { return cornerRounding; }
        void setCornerRounding(float cornerRounding) { this->cornerRounding = cornerRounding; }

        /**
         * These two are just for debugging purposes.
         */
        RenderTarget *getEdgesRenderTarget() { return edgesRT; }
        RenderTarget *getBlendRenderTarget() { return blendRT; }

        /**
         * Jitters the transformations matrix.
         */
        D3DXMATRIX JitteredMatrix(const D3DXMATRIX &worldViewProjection, Mode mode) const;

        /**
         * Increases the subpixel counter.
         */
        void nextFrame();
        int getFrameIndex() const { return frameIndex; }

        /**
         * @EXTERNAL_STORAGE
         *
         * If you have one or two spare render targets of the same size as the
         * backbuffer, you may want to pass them to SMAA::SMAA() using a
         * ExternalStorage object. You may pass one or the two, depending on
         * what you have available.
         *
         * A non-sRGB RG buffer (at least) is expected for storing edges.
         * A non-sRGB RGBA buffer is expected for the blending weights.
         */
        class ExternalStorage {
            public:
                ExternalStorage(ID3D10ShaderResourceView *edgesSRV=nullptr,
                                ID3D10RenderTargetView *edgesRTV=nullptr,
                                ID3D10ShaderResourceView *weightsSRV=nullptr,
                                ID3D10RenderTargetView *weightsRTV=nullptr)
                    : edgesSRV(edgesSRV),
                      edgesRTV(edgesRTV), 
                      weightsSRV(weightsSRV),
                      weightsRTV(weightsRTV) {}

            ID3D10ShaderResourceView *edgesSRV, *weightsSRV;
            ID3D10RenderTargetView *edgesRTV, *weightsRTV;
        };

    private:
        /**
         * Detects the sample order of MSAA 2x by rendering a quad that fills
         * the left half of a 1x1 MSAA 2x buffer. The sample #0 of this buffer
         * is then loaded and stored into a temporal 1x1 buffer. We transfer
         * this value to the CPU, and check its value to determine the actual
         * sample order. See related SMAA::msaaReorder.
         */
        void detectMSAAOrder();

        D3DXVECTOR2 getJitter(Mode mode) const;
        int getSubsampleIndex(Mode mode, int pass) const;

        void loadAreaTex();
        void loadSearchTex();
        void edgesDetectionPass(ID3D10DepthStencilView *dsv, Input input);
        void blendingWeightsCalculationPass(ID3D10DepthStencilView *dsv, Mode mode, int subsampleIndex);
        void neighborhoodBlendingPass(ID3D10RenderTargetView *dstRTV, ID3D10DepthStencilView *dsv);

        ID3D10Device *device;
        int width, height;
        Preset preset;
        ID3D10Effect *effect;
        FullscreenTriangle *triangle;

        RenderTarget *edgesRT;
        RenderTarget *blendRT;

        ID3D10Texture2D *areaTex;
        ID3D10ShaderResourceView *areaTexSRV;
        ID3D10Texture2D *searchTex;
        ID3D10ShaderResourceView *searchTexSRV;

        ID3D10EffectScalarVariable *thresholdVariable, *cornerRoundingVariable,
                                   *maxSearchStepsVariable, *maxSearchStepsDiagVariable,
                                   *blendFactorVariable;
        ID3D10EffectVectorVariable *subsampleIndicesVariable;
        ID3D10EffectShaderResourceVariable *areaTexVariable, *searchTexVariable,
                                           *colorTexVariable, *colorTexGammaVariable, *colorTexPrevVariable, *colorTexMSVariable,
                                           *depthTexVariable, *velocityTexVariable,
                                           *edgesTexVariable, *blendTexVariable;

        ID3D10EffectTechnique *edgeDetectionTechniques[3],
                              *blendingWeightCalculationTechnique,
                              *neighborhoodBlendingTechnique,
                              *resolveTechnique,
                              *separateTechnique;

        float threshold, cornerRounding;
        int maxSearchSteps, maxSearchStepsDiag;

        int frameIndex;
        int msaaOrderMap[2];
};

#endif
