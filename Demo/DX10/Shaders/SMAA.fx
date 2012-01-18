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


/**
 * Setup mandatory defines. Use a real macro here for maximum performance!
 */
#ifndef SMAA_PIXEL_SIZE // It's actually set on runtime, this is for compilation time syntax checking.
#define SMAA_PIXEL_SIZE float2(1.0 / 1280.0, 1.0 / 720.0)
#endif

/**
 * This is only required for temporal modes (SMAA T2x).
 */
int4 subsampleIndices;

/**
 * This is required for blending the results of previous subsample with the
 * output render target; it's used in SMAA S2x and 4x, for other modes just use
 * 1.0 (no blending).
 */
float blendFactor = 1.0;

/**
 * This can be ignored; its purpose is to support interactive custom parameter
 * tweaking.
 */
float threshld;
float maxSearchSteps;
float maxSearchStepsDiag;
float cornerRounding;

#define SMAA_PRESET_CUSTOM
#ifdef SMAA_PRESET_CUSTOM
#define SMAA_THRESHOLD threshld
#define SMAA_MAX_SEARCH_STEPS maxSearchSteps
#define SMAA_MAX_SEARCH_STEPS_DIAG maxSearchStepsDiag
#define SMAA_CORNER_ROUNDING cornerRounding
#define SMAA_FORCE_DIAGONAL_DETECTION 1
#define SMAA_FORCE_CORNER_DETECTION 1
#endif

// Set the HLSL version:
#ifndef SMAA_HLSL_4_1
#define SMAA_HLSL_4 1
#endif

// And include our header!
#include "SMAA.h"

// Set pixel shader version accordingly:
#if SMAA_HLSL_4_1 == 1
#define PS_VERSION ps_4_1
#else
#define PS_VERSION ps_4_0
#endif


/**
 * DepthStencilState's and company
 */
DepthStencilState DisableDepthStencil {
    DepthEnable = FALSE;
    StencilEnable = FALSE;
};

DepthStencilState DisableDepthReplaceStencil {
    DepthEnable = FALSE;
    StencilEnable = TRUE;
    FrontFaceStencilPass = REPLACE;
};

DepthStencilState DisableDepthUseStencil {
    DepthEnable = FALSE;
    StencilEnable = TRUE;
    FrontFaceStencilFunc = EQUAL;
};

BlendState Blend {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = TRUE;
    SrcBlend = BLEND_FACTOR;
    DestBlend = INV_BLEND_FACTOR;
    BlendOp = ADD;
};

BlendState NoBlending {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
};


/**
 * Input textures
 */
Texture2D colorTex;
Texture2D colorTexGamma;
Texture2D colorTexPrev;
Texture2DMS<float4, 2> colorMSTex;
Texture2D depthTex;
Texture2D velocityTex;

/**
 * Temporal textures
 */
Texture2D edgesTex;
Texture2D blendTex;

/**
 * Pre-computed area and search textures
 */
Texture2D areaTex;
Texture2D searchTex;


/**
 * Function wrappers
 */
void DX10_SMAAEdgeDetectionVS(float4 position : POSITION,
                              out float4 svPosition : SV_POSITION,
                              inout float2 texcoord : TEXCOORD0,
                              out float4 offset[3] : TEXCOORD1) {
    SMAAEdgeDetectionVS(position, svPosition, texcoord, offset);
}

void DX10_SMAABlendingWeightCalculationVS(float4 position : POSITION,
                                          out float4 svPosition : SV_POSITION,
                                          inout float2 texcoord : TEXCOORD0,
                                          out float2 pixcoord : TEXCOORD1,
                                          out float4 offset[3] : TEXCOORD2) {
    SMAABlendingWeightCalculationVS(position, svPosition, texcoord, pixcoord, offset);
}

void DX10_SMAANeighborhoodBlendingVS(float4 position : POSITION,
                                     out float4 svPosition : SV_POSITION,
                                     inout float2 texcoord : TEXCOORD0,
                                     out float4 offset[2] : TEXCOORD1) {
    SMAANeighborhoodBlendingVS(position, svPosition, texcoord, offset);
}

void DX10_SMAAResolveVS(float4 position : POSITION,
                        out float4 svPosition : SV_POSITION,
                        inout float2 texcoord : TEXCOORD0) {
    SMAAResolveVS(position, svPosition, texcoord);
}

void DX10_SMAASeparateVS(float4 position : POSITION,
                         out float4 svPosition : SV_POSITION,
                         inout float2 texcoord : TEXCOORD0) {
    SMAASeparateVS(position, svPosition, texcoord);
}

float4 DX10_SMAALumaEdgeDetectionPS(float4 position : SV_POSITION,
                                    float2 texcoord : TEXCOORD0,
                                    float4 offset[3] : TEXCOORD1,
                                    uniform SMAATexture2D colorTexGamma) : SV_TARGET {
    #if SMAA_PREDICATION == 1
    return SMAALumaEdgeDetectionPS(texcoord, offset, colorTexGamma, depthTex);
    #else
    return SMAALumaEdgeDetectionPS(texcoord, offset, colorTexGamma);
    #endif
}

float4 DX10_SMAAColorEdgeDetectionPS(float4 position : SV_POSITION,
                                     float2 texcoord : TEXCOORD0,
                                     float4 offset[3] : TEXCOORD1,
                                     uniform SMAATexture2D colorTexGamma) : SV_TARGET {
    #if SMAA_PREDICATION == 1
    return SMAAColorEdgeDetectionPS(texcoord, offset, colorTexGamma, depthTex);
    #else
    return SMAAColorEdgeDetectionPS(texcoord, offset, colorTexGamma);
    #endif
}

float4 DX10_SMAADepthEdgeDetectionPS(float4 position : SV_POSITION,
                                     float2 texcoord : TEXCOORD0,
                                     float4 offset[3] : TEXCOORD1,
                                     uniform SMAATexture2D depthTex) : SV_TARGET {
    return SMAADepthEdgeDetectionPS(texcoord, offset, depthTex);
}

float4 DX10_SMAABlendingWeightCalculationPS(float4 position : SV_POSITION,
                                            float2 texcoord : TEXCOORD0,
                                            float2 pixcoord : TEXCOORD1,
                                            float4 offset[3] : TEXCOORD2,
                                            uniform SMAATexture2D edgesTex, 
                                            uniform SMAATexture2D areaTex, 
                                            uniform SMAATexture2D searchTex) : SV_TARGET {
    return SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset, edgesTex, areaTex, searchTex, subsampleIndices);
}

float4 DX10_SMAANeighborhoodBlendingPS(float4 position : SV_POSITION,
                                       float2 texcoord : TEXCOORD0,
                                       float4 offset[2] : TEXCOORD1,
                                       uniform SMAATexture2D colorTex,
                                       uniform SMAATexture2D blendTex) : SV_TARGET {
    return SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}

float4 DX10_SMAAResolvePS(float4 position : SV_POSITION,
                          float2 texcoord : TEXCOORD0,
                          uniform SMAATexture2D colorTex,
                          uniform SMAATexture2D colorTexPrev,
                          uniform SMAATexture2D velocityTex) : SV_TARGET {
    #if SMAA_REPROJECTION == 1
    return SMAAResolvePS(texcoord, colorTex, colorTexPrev, velocityTex);
    #else
    return SMAAResolvePS(texcoord, colorTex, colorTexPrev);
    #endif
}

void DX10_SMAASeparatePS(float4 position : SV_POSITION,
                         float2 texcoord : TEXCOORD0,
                         out float4 target0 : SV_TARGET0,
                         out float4 target1 : SV_TARGET1,
                         uniform SMAATexture2DMS2 colorMSTex) {
    SMAASeparatePS(position, texcoord, target0, target1, colorMSTex);
}

/**
 * Edge detection techniques
 */
technique10 LumaEdgeDetection {
    pass LumaEdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, DX10_SMAAEdgeDetectionVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(PS_VERSION, DX10_SMAALumaEdgeDetectionPS(colorTexGamma)));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 ColorEdgeDetection {
    pass ColorEdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, DX10_SMAAEdgeDetectionVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(PS_VERSION, DX10_SMAAColorEdgeDetectionPS(colorTexGamma)));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 DepthEdgeDetection {
    pass DepthEdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, DX10_SMAAEdgeDetectionVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(PS_VERSION, DX10_SMAADepthEdgeDetectionPS(depthTex)));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

/**
 * Blending weight calculation technique
 */
technique10 BlendingWeightCalculation {
    pass BlendingWeightCalculation {
        SetVertexShader(CompileShader(vs_4_0, DX10_SMAABlendingWeightCalculationVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(PS_VERSION, DX10_SMAABlendingWeightCalculationPS(edgesTex, areaTex, searchTex)));

        SetDepthStencilState(DisableDepthUseStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

/**
 * Neighborhood blending technique
 */
technique10 NeighborhoodBlending {
    pass NeighborhoodBlending {
        SetVertexShader(CompileShader(vs_4_0, DX10_SMAANeighborhoodBlendingVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(PS_VERSION, DX10_SMAANeighborhoodBlendingPS(colorTex, blendTex)));

        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(Blend, float4(blendFactor, blendFactor, blendFactor, blendFactor), 0xFFFFFFFF);
        // For SMAA 1x, just use NoBlending!
    }
}

/**
 * Temporal resolve technique
 */
technique10 Resolve {
    pass Resolve {
        SetVertexShader(CompileShader(vs_4_0, DX10_SMAAResolveVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(PS_VERSION, DX10_SMAAResolvePS(colorTex, colorTexPrev, velocityTex)));

        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

/**
 * 2x multisampled buffer conversion into two regular buffers
 */
technique10 Separate {
    pass Separate {
        SetVertexShader(CompileShader(vs_4_0, DX10_SMAASeparateVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(PS_VERSION, DX10_SMAASeparatePS(colorMSTex)));

        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
