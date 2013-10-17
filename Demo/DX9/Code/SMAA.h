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

#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>
#include <dxgi.h>


class SMAA {
    public:
        class ExternalStorage;

        enum Preset { PRESET_LOW, PRESET_MEDIUM, PRESET_HIGH, PRESET_ULTRA, PRESET_CUSTOM };
        enum Input { INPUT_LUMA, INPUT_COLOR, INPUT_DEPTH };

        /**
         * If you have one or two spare render targets of the same size as the
         * backbuffer, you may want to pass them in the 'storage' parameter.
         * You may pass one or the two, depending on what you have available.
         *
         * A RG buffer (at least) is expected for storing edges.
         * A RGBA buffer is expected for the blending weights.
         *
         * By default, two render targets will be created for storing
         * intermediate calculations.
         */
        SMAA(IDirect3DDevice9 *device, int width, int height, Preset preset,
             const ExternalStorage &storage=ExternalStorage());
        ~SMAA();

        /**
         * Processes input texture 'src', storing the antialiased image into
         * 'dst'. Note that 'src' and 'dst' should be associated to different
         * buffers.
         *
         * 'edges' should be the input for using for edge detection: either a
         * depth buffer or a non-sRGB color buffer. Input must be set 
         * accordingly.
         *
         * IMPORTANT: the stencil component of currently bound depth-stencil
         * buffer will be used to mask the zones to be processed. It is assumed
         * to be already cleared to zero when this function is called. It is 
         * not done here because it is usually cleared together with the depth.
         *
         * For performance reasons, the state is not restored before returning
         * from this function (the render target, the input layout, the 
         * depth-stencil and blend states...)
         */
        void go(IDirect3DTexture9 *edges,
                IDirect3DTexture9 *src, 
                IDirect3DSurface9 *dst,
                Input input);

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
         * This class allows to pass spare storage buffers to the SMAA class.
         */
        class ExternalStorage {
            public:
                ExternalStorage(IDirect3DTexture9 *edgeTex=nullptr,
                                IDirect3DSurface9 *edgeSurface=nullptr,
                                IDirect3DTexture9 *blendTex=nullptr,
                                IDirect3DSurface9 *blendSurface=nullptr)
                    : edgeTex(edgeTex),
                      edgeSurface(edgeSurface), 
                      blendTex(blendTex),
                      blendSurface(blendSurface) {}

            IDirect3DTexture9 *edgeTex, *blendTex;
            IDirect3DSurface9 *edgeSurface, *blendSurface;
        };

    private:
        void loadAreaTex();
        void loadSearchTex();
        void edgesDetectionPass(IDirect3DTexture9 *edges, Input input);
        void blendingWeightsCalculationPass();
        void neighborhoodBlendingPass(IDirect3DTexture9 *src, IDirect3DSurface9 *dst);
        void quad(int width, int height);

        IDirect3DDevice9 *device;
        ID3DXEffect *effect;
        IDirect3DVertexDeclaration9 *vertexDeclaration;

        IDirect3DTexture9 *edgeTex;
        IDirect3DSurface9 *edgeSurface;
        bool releaseEdgeResources;

        IDirect3DTexture9 *blendTex;
        IDirect3DSurface9 *blendSurface;
        bool releaseBlendResources;

        IDirect3DTexture9 *areaTex;
        IDirect3DTexture9 *searchTex;

        D3DXHANDLE thresholdHandle;
        D3DXHANDLE maxSearchStepsHandle, maxSearchStepsDiagHandle;
        D3DXHANDLE cornerRoundingHandle;
        D3DXHANDLE areaTexHandle, searchTexHandle;
        D3DXHANDLE colorTexHandle, depthTexHandle;
        D3DXHANDLE edgesTexHandle, blendTexHandle;
        D3DXHANDLE lumaEdgeDetectionHandle, colorEdgeDetectionHandle, depthEdgeDetectionHandle,
                   blendWeightCalculationHandle, neighborhoodBlendingHandle;
        
        int maxSearchSteps;
        int maxSearchStepsDiag;
        float cornerRounding;
        float threshold;
        int width, height;
};

#endif
