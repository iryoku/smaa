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
 *      "Uses Jimenez's MLAA. Copyright (C) 2011 by Jorge Jimenez, Belen Masia,
 *       Jose I. Echevarria, Fernando Navarro and Diego Gutierrez."
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


// Some configuration variables:
float threshold;
float maxSearchSteps;

/**
 * Setup mandatory defines. Use a real macro here for maximum performance!
 */
#ifndef MLAA_PIXEL_SIZE // It's actually set on runtime, this is for compilation time syntax checking.
#define MLAA_PIXEL_SIZE float2(1.0 / 1280.0, 1.0 / 720.0)
#endif

/**
 * Setup optional defines.
 */
#define MLAA_THRESHOLD threshold
#define MLAA_MAX_SEARCH_STEPS maxSearchSteps

// Set the HLSL version:
#define MLAA_HLSL_3 1

// And include our header!
#include "MLAA.h"


/**
 * Input vars and textures.
 */

texture2D colorTex2D;
texture2D depthTex2D;
texture2D edgesTex2D;
texture2D blendTex2D;
texture2D areaTex2D;
texture2D searchTex2D;


/**
 * DX9 samplers.
 */
sampler2D colorTex {
    Texture = <colorTex2D>;
    AddressU  = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = true;
};

sampler2D colorTexG {
    Texture = <colorTex2D>;
    AddressU  = Clamp; AddressV = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

sampler2D depthTex {
    Texture = <depthTex2D>;
    AddressU  = Clamp; AddressV = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

sampler2D edgesTex {
    Texture = <edgesTex2D>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

sampler2D blendTex {
    Texture = <blendTex2D>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

sampler2D areaTex {
    Texture = <areaTex2D>;
    AddressU = Clamp; AddressV = Clamp; AddressW = Clamp;
    MipFilter = Linear; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

sampler2D searchTex {
    Texture = <searchTex2D>;
    AddressU = Clamp; AddressV = Clamp; AddressW = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = false;
};


/**
 * Function wrappers
 */
void DX9_MLAAEdgeDetectionVS(inout float4 position : POSITION,
                             inout float2 texcoord : TEXCOORD0,
                             out float4 offset[2] : TEXCOORD1) {
    MLAAEdgeDetectionVS(position, position, texcoord, offset);
}

void DX9_MLAABlendWeightCalculationVS(inout float4 position : POSITION,
                                      inout float2 texcoord : TEXCOORD0,
                                      out float2 pixcoord : TEXCOORD1,
                                      out float4 offset[3] : TEXCOORD2) {
    MLAABlendWeightCalculationVS(position, position, texcoord, pixcoord, offset);
}

void DX9_MLAANeighborhoodBlendingVS(inout float4 position : POSITION,
                                    inout float2 texcoord : TEXCOORD0,
                                    out float4 offset[2] : TEXCOORD1) {
    MLAANeighborhoodBlendingVS(position, position, texcoord, offset);
}


float4 DX9_MLAALumaEdgeDetectionPS(float4 position : SV_POSITION,
                                   float2 texcoord : TEXCOORD0,
                                   float4 offset[2] : TEXCOORD1,
                                   uniform MLAATexture2D colorGammaTex) : COLOR {
    return MLAALumaEdgeDetectionPS(texcoord, offset, colorGammaTex);
}

float4 DX9_MLAAColorEdgeDetectionPS(float4 position : SV_POSITION,
                                    float2 texcoord : TEXCOORD0,
                                    float4 offset[2] : TEXCOORD1,
                                    uniform MLAATexture2D colorGammaTex) : COLOR {
    return MLAAColorEdgeDetectionPS(texcoord, offset, colorGammaTex);
}

float4 DX9_MLAADepthEdgeDetectionPS(float4 position : SV_POSITION,
                                    float2 texcoord : TEXCOORD0,
                                    float4 offset[2] : TEXCOORD1,
                                    uniform MLAATexture2D depthTex) : COLOR {
    return MLAADepthEdgeDetectionPS(texcoord, offset, depthTex);
}

float4 DX9_MLAABlendingWeightCalculationPS(float4 position : SV_POSITION,
                                           float2 texcoord : TEXCOORD0,
                                           float2 pixcoord : TEXCOORD1,
                                           float4 offset[3] : TEXCOORD2,
                                           uniform MLAATexture2D edgesTex, 
                                           uniform MLAATexture2D areaTex, 
                                           uniform MLAATexture2D searchTex) : COLOR {
    return MLAABlendingWeightCalculationPS(texcoord, pixcoord, offset, edgesTex, areaTex, searchTex);
}

float4 DX9_MLAANeighborhoodBlendingPS(float4 position : SV_POSITION,
                                      float2 texcoord : TEXCOORD0,
                                      float4 offset[2] : TEXCOORD1,
                                      uniform MLAATexture2D colorTex,
                                      uniform MLAATexture2D blendTex) : COLOR {
    return MLAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}


/**
 * Time for some techniques!
 */
technique LumaEdgeDetection {
    pass LumaEdgeDetection {
        VertexShader = compile vs_3_0 DX9_MLAAEdgeDetectionVS();
        PixelShader = compile ps_3_0 DX9_MLAALumaEdgeDetectionPS(colorTexG);
        ZEnable = false;        
        SRGBWriteEnable = false;
        AlphaBlendEnable = false;

        // We will be creating the stencil buffer for later usage.
        StencilEnable = true;
        StencilPass = REPLACE;
        StencilRef = 1;
    }
}

technique ColorEdgeDetection {
    pass ColorEdgeDetection {
        VertexShader = compile vs_3_0 DX9_MLAAEdgeDetectionVS();
        PixelShader = compile ps_3_0 DX9_MLAAColorEdgeDetectionPS(colorTexG);
        ZEnable = false;        
        SRGBWriteEnable = false;
        AlphaBlendEnable = false;

        // We will be creating the stencil buffer for later usage.
        StencilEnable = true;
        StencilPass = REPLACE;
        StencilRef = 1;
    }
}

technique DepthEdgeDetection {
    pass DepthEdgeDetection {
        VertexShader = compile vs_3_0 DX9_MLAAEdgeDetectionVS();
        PixelShader = compile ps_3_0 DX9_MLAADepthEdgeDetectionPS(depthTex);
        ZEnable = false;        
        SRGBWriteEnable = false;
        AlphaBlendEnable = false;

        // We will be creating the stencil buffer for later usage.
        StencilEnable = true;
        StencilPass = REPLACE;
        StencilRef = 1;
    }
}

technique BlendWeightCalculation {
    pass BlendWeightCalculation {
        VertexShader = compile vs_3_0 DX9_MLAABlendWeightCalculationVS();
        PixelShader = compile ps_3_0 DX9_MLAABlendingWeightCalculationPS(edgesTex, areaTex, searchTex);
        ZEnable = false;
        SRGBWriteEnable = false;
        AlphaBlendEnable = false;

        // Here we want to process only marked pixels.
        StencilEnable = true;
        StencilPass = KEEP;
        StencilFunc = EQUAL;
        StencilRef = 1;
    }
}

technique NeighborhoodBlending {
    pass NeighborhoodBlending {
        VertexShader = compile vs_3_0 DX9_MLAANeighborhoodBlendingVS();
        PixelShader = compile ps_3_0 DX9_MLAANeighborhoodBlendingPS(colorTex, blendTex);
        ZEnable = false;
        SRGBWriteEnable = true;
        AlphaBlendEnable = false;

        // Here we want to process all the pixels.
        StencilEnable = false;
    }
}
