
/** SMAA demo (see the disclaimer/original copyright notice below) ported to 
 *  C# / XNA 4.0 by Alexander C. Gessler (www.greentoken.de).
 *  
 *  This is more or less a one-by-one translation of SMAA.h/SMAA.cpp 
 *  from the D3D9 SMAA demo to C#. 
 */

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

using System.Diagnostics;
using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;

namespace SMAADemo
{
    public class SMAA {

        public enum Preset { LOW, MEDIUM, HIGH, ULTRA, CUSTOM };
        public enum Input { LUMA, COLOR, DEPTH };


        /**
        * Maximum length to search for patterns. Each step is two pixels wide.
        */
        int MaxSearchSteps
        {
            get {
                return maxSearchSteps;
            }

            set {
                maxSearchSteps = value;
            }
        }


        /**
         * Threshold for the edge detection.
         */
        float Threshold
        {
            get {
                return threshold;
            }

            set {
                threshold = value;
            }
        }


        /**
         * If you have one or two spare render targets of the same size as the
         * backbuffer, you may want to pass them in the 'storage' parameter.
         * You may pass one or the two, depending on what you have available.
         *
         * A RG buffer (at least) is expected for storing edges. Note that
         * this target _must_ have a D24S8 depth/stencil buffer.
         * A RGBA buffer is expected for the blending weights.
         *
         * By default, two render targets will be created for storing
         * intermediate calculations. 
         * 
         * AreaTexDX9.dds and SearchTex.dds from the SMAA distribution need
         * to be added to the content project and compiled as well (no mips,
         * no color key, no premultiplied alpha). `textureBaseName` will
         * be prepended to the search path for these textures in case
         * you don't want them in the root content folder.
         * 
         * `effectBaseName` specifies the first part of the effect to be loaded.
         * the chosen `preset` will be appended. 
         * 
         * NOTE: The caller is responsible for ensuring that the effect exists
         * and can be accessed by the given contentmanager. This is because compiling
         * shaders at runtime is virtually impossible / very difficult to achieve in
         * XNA and the easiest way is to use the content pipeline to generate
         * all shader permutations upfront.
         */
        public SMAA(GraphicsDevice _device, int _width, int _height, Preset _preset,
            ContentManager _content,
            RenderTarget2D rt_rg = null,
            RenderTarget2D rt_rgba = null,
            string effectBaseName = null /* default: "SMAA_" */,
            string textureBaseName = null /* default: "" */
            )
            : this(_device, _width, _height, _preset,
            new DefaultTextureProvider(_content, null, textureBaseName),
            new DefaultEffectProvider(_content, null, effectBaseName),
            rt_rg,
            rt_rgba
            )
        {
        }



        /**
         * If you have one or two spare render targets of the same size as the
         * backbuffer, you may want to pass them in the 'storage' parameter.
         * You may pass one or the two, depending on what you have available.
         *
         * A RG buffer (at least) is expected for storing edges. Note that
         * this target _must_ have a D24S8 depth/stencil buffer.
         * A RGBA buffer is expected for the blending weights.
         *
         * By default, two render targets will be created for storing
         * intermediate calculations. 
         * 
         * AreaTexDX9.dds and SearchTex.dds from the SMAA distribution need
         * to be added to the content project and compiled as well (no mips,
         * no color key, no premultiplied alpha). `textureBaseName` will
         * be prepended to the search path for these textures in case
         * you don't want them in the root content folder.
         * 
         * `effectBaseName` specifies the first part of the effect to be loaded.
         * the chosen `preset` will be appended. 
         * 
         * NOTE: The caller is responsible for ensuring that the effect exists
         * and can be accessed by the given content manager. This is because compiling
         * shaders at runtime is virtually impossible / very difficult to achieve in
         * XNA and the easiest way is to use the content pipeline to generate
         * all shader permutations upfront.
         */
        public SMAA(GraphicsDevice _device, int _width, int _height, Preset _preset,
            ITextureProvider texProvider,
            IEffectProvider effectProvider,
            RenderTarget2D rt_rg = null,
            RenderTarget2D rt_rgba = null
            )
        {
            Debug.Assert(_width > 0);
            Debug.Assert(_height > 0);
            Debug.Assert(effectProvider != null);
            Debug.Assert(texProvider != null);
            Debug.Assert(_device != null);

            effect = effectProvider.Get(_preset.ToString());
            
            device = _device;
            width = _width;
            height = _height;

            threshold = 0.05f;
            maxSearchSteps = 8;
   
            // If storage for the edges is not specified we will create it.
            if (rt_rg != null)
            {
                Debug.Assert(rt_rg.DepthStencilFormat == DepthFormat.Depth24Stencil8);

                edgeTex = rt_rg;
                releaseEdgeResources = false;
            } 
            else
            {
                edgeTex = new RenderTarget2D(device, width, height, false, 
                    SurfaceFormat.Color, 
                    DepthFormat.Depth24Stencil8);

                releaseEdgeResources = true;
            }


            // If storage for the blend weights is not specified we will create it.
            if (rt_rgba != null)
            {
                blendTex = rt_rgba;
                releaseBlendResources = false;
            }
            else
            {
                blendTex = new RenderTarget2D(device, width, height, false, 
                    SurfaceFormat.Color, 
                    DepthFormat.None);

                releaseBlendResources = true;
            }


            // Load the precomputed textures.
            areaTex = texProvider.Get("AreaTexDX9");
            searchTex = texProvider.Get("SearchTex");

            // Create some handles for techniques and variables.
            thresholdHandle = effect.Parameters["threshold"];
            maxSearchStepsHandle = effect.Parameters["maxSearchSteps"];
            areaTexHandle = effect.Parameters["areaTex2D"];
            searchTexHandle = effect.Parameters["searchTex2D"];
            colorTexHandle = effect.Parameters["colorTex2D"];
            depthTexHandle = effect.Parameters["depthTex2D"];
            edgesTexHandle = effect.Parameters["edgesTex2D"];
            blendTexHandle = effect.Parameters["blendTex2D"];
            pixelSizeHandle = effect.Parameters["SMAA_PIXEL_SIZE"];
            lumaEdgeDetectionHandle = effect.Techniques["LumaEdgeDetection"];
            colorEdgeDetectionHandle = effect.Techniques["ColorEdgeDetection"];
            depthEdgeDetectionHandle = effect.Techniques["DepthEdgeDetection"];
            blendWeightCalculationHandle = effect.Techniques["BlendWeightCalculation"];
            neighborhoodBlendingHandle = effect.Techniques["NeighborhoodBlending"];
        }


        /** Dispose all VRAM-consuming resources */
        public void Dispose()
        {
            if (releaseBlendResources)
            {
                blendTex.Dispose();
                blendTex = null;
            }

            if (releaseEdgeResources)
            {
                edgeTex.Dispose();
                edgeTex = null;
            }

            effect.Dispose();
            effect = null;
        }


        /**
         * Processes input texture 'src', storing the anti aliased image into
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
        public void Go(Texture2D edges,
                Texture2D src,
                RenderTarget2D dst,
                Input input)
        {
            pixelSizeHandle.SetValue(Vector2.One / new Vector2(width, height));

            edgesDetectionPass(edges, input);
            blendingWeightsCalculationPass();
            neighborhoodBlendingPass(src, dst);
        }

       
        
        private void edgesDetectionPass(Texture2D edges, Input input)
        {
            // Set the render target and clear both the color and the stencil buffers.
            device.SetRenderTarget(edgeTex);
            device.Clear(ClearOptions.Stencil | ClearOptions.Target, new Color(0,0,0,0), 1.0f, 0);

            // Setup variables.
            thresholdHandle.SetValue(threshold);
            maxSearchStepsHandle.SetValue((float)maxSearchSteps);

            // Select the technique accordingly.
            switch (input) {
                case Input.LUMA:
                    colorTexHandle.SetValue(edges);
                    effect.CurrentTechnique = lumaEdgeDetectionHandle;
                    break;
                case Input.COLOR:
                    colorTexHandle.SetValue(edges);
                    effect.CurrentTechnique = colorEdgeDetectionHandle;
                    break;
                case Input.DEPTH:
                    depthTexHandle.SetValue(edges);
                    effect.CurrentTechnique = depthEdgeDetectionHandle;
                    break;
                default:
                    Debug.Assert(false);
                    break;
            }

            // Do it!
            effect.CurrentTechnique.Passes[0].Apply();

            quad();
        }
        
        
        private void blendingWeightsCalculationPass()
        {
            // Set the render target and clear it.
            device.SetRenderTarget(blendTex);
            device.Clear(new Color(0, 0, 0, 0));

            edgesTexHandle.SetValue(edgeTex);
            areaTexHandle.SetValue(areaTex);
            searchTexHandle.SetValue(searchTex);
            effect.CurrentTechnique = blendWeightCalculationHandle;

            // And here we go!
            effect.CurrentTechnique.Passes[0].Apply();

            quad();
        }
        
        
        private void neighborhoodBlendingPass(Texture2D src, RenderTarget2D dst)
        {
            device.SetRenderTarget(dst);
            colorTexHandle.SetValue(src);
            blendTexHandle.SetValue(blendTex);
            effect.CurrentTechnique = neighborhoodBlendingHandle;

            // Yeah! We will finally have the anti aliased image :D
            effect.CurrentTechnique.Passes[0].Apply();

            quad();
        }
        
        
        private void quad()
        {
            // Typical aligned full screen quad.
            var pixelSize = Vector2.One / new Vector2(width, height);
            var quad = new[] {
                new VertexPositionTexture( new Vector3(-1.0f - pixelSize.X,  1.0f + pixelSize.Y, 0.5f), new Vector2(0.0f, 0.0f)),
                new VertexPositionTexture( new Vector3(1.0f - pixelSize.X,  1.0f + pixelSize.Y, 0.5f), new Vector2(1.0f, 0.0f)),
                new VertexPositionTexture( new Vector3(-1.0f - pixelSize.X, -1.0f + pixelSize.Y, 0.5f), new Vector2(0.0f, 1.0f)),
                new VertexPositionTexture( new Vector3(1.0f - pixelSize.X, -1.0f + pixelSize.Y, 0.5f), new Vector2(1.0f, 1.0f))
            };

            device.DrawUserPrimitives(PrimitiveType.TriangleStrip, quad, 0, 2, VertexPositionTexture.VertexDeclaration);
        }


        private readonly GraphicsDevice device;
        private Effect effect;

        public RenderTarget2D edgeTex;
        public RenderTarget2D blendTex;
        private readonly bool releaseEdgeResources;
        private readonly bool releaseBlendResources;

        private readonly Texture2D areaTex;
        private readonly Texture2D searchTex;


        private readonly EffectParameter thresholdHandle;
        private readonly EffectParameter maxSearchStepsHandle;
        private readonly EffectParameter areaTexHandle;
        private readonly EffectParameter searchTexHandle;
        private readonly EffectParameter colorTexHandle;
        private readonly EffectParameter depthTexHandle;
        private readonly EffectParameter edgesTexHandle;
        private readonly EffectParameter blendTexHandle;

        private readonly EffectTechnique lumaEdgeDetectionHandle;
        private readonly EffectTechnique colorEdgeDetectionHandle;
        private readonly EffectTechnique depthEdgeDetectionHandle;
        private readonly EffectTechnique blendWeightCalculationHandle;
        private readonly EffectTechnique neighborhoodBlendingHandle;
        private readonly EffectParameter pixelSizeHandle;

        private int maxSearchSteps;
        private float threshold;
        private readonly int width;
        private readonly int height;
    }
}
