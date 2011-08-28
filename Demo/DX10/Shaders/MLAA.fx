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
#define MLAA_HLSL_4 1

// And include our header!
#include "MLAA.h"


/**
 * DepthStencilState's and company.
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

BlendState NoBlending {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
};


/**
 * Input textures.
 */
Texture2D colorTex;
Texture2D colorGammaTex;
Texture2D depthTex;
Texture2D edgesTex;
Texture2D blendTex;
Texture2D areaTex;
Texture2D searchTex;


/**
 * Function wrappers
 */
void DX10_MLAAEdgeDetectionVS(float4 position : POSITION,
                              out float4 svPosition : SV_POSITION,
                              inout float2 texcoord : TEXCOORD0,
                              out float4 offset[2] : TEXCOORD1) {
    MLAAEdgeDetectionVS(position, svPosition, texcoord, offset);
}

void DX10_MLAABlendWeightCalculationVS(float4 position : POSITION,
                                       out float4 svPosition : SV_POSITION,
                                       inout float2 texcoord : TEXCOORD0,
                                       out float2 pixcoord : TEXCOORD1,
                                       out float4 offset[3] : TEXCOORD2) {
    MLAABlendWeightCalculationVS(position, svPosition, texcoord, pixcoord, offset);
}

void DX10_MLAANeighborhoodBlendingVS(float4 position : POSITION,
                                     out float4 svPosition : SV_POSITION,
                                     inout float2 texcoord : TEXCOORD0,
                                     out float4 offset[2] : TEXCOORD1) {
    MLAANeighborhoodBlendingVS(position, svPosition, texcoord, offset);
}


float4 DX10_MLAALumaEdgeDetectionPS(float4 position : SV_POSITION,
                                    float2 texcoord : TEXCOORD0,
                                    float4 offset[2] : TEXCOORD1,
                                    uniform MLAATexture2D colorGammaTex) : SV_TARGET {
    return MLAALumaEdgeDetectionPS(texcoord, offset, colorGammaTex);
}

float4 DX10_MLAAColorEdgeDetectionPS(float4 position : SV_POSITION,
                                     float2 texcoord : TEXCOORD0,
                                     float4 offset[2] : TEXCOORD1,
                                     uniform MLAATexture2D colorGammaTex) : SV_TARGET {
    return MLAAColorEdgeDetectionPS(texcoord, offset, colorGammaTex);
}

float4 DX10_MLAADepthEdgeDetectionPS(float4 position : SV_POSITION,
                                     float2 texcoord : TEXCOORD0,
                                     float4 offset[2] : TEXCOORD1,
                                     uniform MLAATexture2D depthTex) : SV_TARGET {
    return MLAADepthEdgeDetectionPS(texcoord, offset, depthTex);
}

float4 DX10_MLAABlendingWeightCalculationPS(float4 position : SV_POSITION,
                                            float2 texcoord : TEXCOORD0,
                                            float2 pixcoord : TEXCOORD1,
                                            float4 offset[3] : TEXCOORD2,
                                            uniform MLAATexture2D edgesTex, 
                                            uniform MLAATexture2D areaTex, 
                                            uniform MLAATexture2D searchTex) : SV_TARGET {
    return MLAABlendingWeightCalculationPS(texcoord, pixcoord, offset, edgesTex, areaTex, searchTex);
}

float4 DX10_MLAANeighborhoodBlendingPS(float4 position : SV_POSITION,
                                       float2 texcoord : TEXCOORD0,
                                       float4 offset[2] : TEXCOORD1,
                                       uniform MLAATexture2D colorTex,
                                       uniform MLAATexture2D blendTex) : SV_TARGET {
    return MLAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);
}


/**
 * Time for some techniques!
 */
technique10 LumaEdgeDetection {
    pass LumaEdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, DX10_MLAAEdgeDetectionVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, DX10_MLAALumaEdgeDetectionPS(colorGammaTex)));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 ColorEdgeDetection {
    pass ColorEdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, DX10_MLAAEdgeDetectionVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, DX10_MLAAColorEdgeDetectionPS(colorGammaTex)));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 DepthEdgeDetection {
    pass DepthEdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, DX10_MLAAEdgeDetectionVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, DX10_MLAADepthEdgeDetectionPS(depthTex)));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 BlendingWeightCalculation {
    pass BlendingWeightCalculation {
        SetVertexShader(CompileShader(vs_4_0, DX10_MLAABlendWeightCalculationVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, DX10_MLAABlendingWeightCalculationPS(edgesTex, areaTex, searchTex)));

        SetDepthStencilState(DisableDepthUseStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 NeighborhoodBlending {
    pass NeighborhoodBlending {
        SetVertexShader(CompileShader(vs_4_0, DX10_MLAANeighborhoodBlendingVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, DX10_MLAANeighborhoodBlendingPS(colorTex, blendTex)));

        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
