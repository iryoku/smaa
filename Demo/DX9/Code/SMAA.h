/**
 * Copyright (C) 2011 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2011 Belen Masia (bmasia@unizar.es) 
 * Copyright (C) 2011 Jose I. Echevarria (joseignacioechevarria@gmail.com) 
 * Copyright (C) 2011 Fernando Navarro (fernandn@microsoft.com) 
 * Copyright (C) 2011 Diego Gutierrez (diegog@unizar.es)
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the following disclaimer
 *       in the documentation and/or other materials provided with the 
 *       distribution:
 * 
 *      "Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria,
 *       Belen Masia, Fernando Navarro and Diego Gutierrez."
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


#ifndef SMAA_H
#define SMAA_H

#include <dxgi.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>


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
         * This class allows to pass spare storage buffers to the SMAA class.
         */
        class ExternalStorage {
            public:
                ExternalStorage(IDirect3DTexture9 *edgeTex=NULL,
                                IDirect3DSurface9 *edgeSurface=NULL,
                                IDirect3DTexture9 *blendTex=NULL,
                                IDirect3DSurface9 *blendSurface=NULL)
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

        D3DXHANDLE thresholdHandle, maxSearchStepsHandle;
        D3DXHANDLE areaTexHandle, searchTexHandle;
        D3DXHANDLE colorTexHandle, depthTexHandle;
        D3DXHANDLE edgesTexHandle, blendTexHandle;
        D3DXHANDLE lumaEdgeDetectionHandle, colorEdgeDetectionHandle, depthEdgeDetectionHandle,
                   blendWeightCalculationHandle, neighborhoodBlendingHandle;
        
        int maxSearchSteps;
        float threshold;
        int width, height;
};

#endif
