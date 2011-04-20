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

// For checking syntax at compile time
#if !defined(PIXEL_SIZE)
#define PIXEL_SIZE float2(1.0 / 1280.0, 1.0 / 720.0)
#endif

#define NUM_DISTANCES 32
#define AREA_SIZE (NUM_DISTANCES * 5)

cbuffer UpdatedOncePerFrame {
    // For maximum performance maxDistance should be defined as a macro
    int maxSearchSteps;
    float threshold;
}

Texture2D colorTex;
Texture2D<float> depthTex;
Texture2D edgesTex;
Texture2D blendTex;
Texture2D areaTex;


SamplerState LinearSampler {
    Filter = MIN_MAG_LINEAR_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState PointSampler {
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};


float4 mad(float4 m, float4 a, float4 b) {
    return m * a + b;
}


float2 Area(float2 distance, float e1, float e2) {
     // * By dividing by AREA_SIZE - 1.0 below we are implicitely offsetting to
     //   always fall inside of a pixel
     // * Rounding prevents bilinear access precision problems
    float2 pixcoord = NUM_DISTANCES * round(4.0 * float2(e1, e2)) + distance;
    float2 texcoord = pixcoord / (AREA_SIZE - 1.0);
    return areaTex.SampleLevel(PointSampler, texcoord, 0).rg;
}


struct PassV2P {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PassV2P PassVS(float4 position : POSITION,
               float2 texcoord : TEXCOORD0) {
    PassV2P output;
    output.position = position;
    output.texcoord = texcoord;
    return output;
}


float4 EdgeDetectionPS(float4 position : SV_POSITION,
                       float2 texcoord : TEXCOORD0) : SV_TARGET {
    float3 weights = float3(0.2126,0.7152, 0.0722);

    float L = dot(colorTex.SampleLevel(PointSampler, texcoord, 0).rgb, weights);
    float Lleft = dot(colorTex.SampleLevel(PointSampler, texcoord, 0, -int2(1, 0)).rgb, weights);
    float Ltop  = dot(colorTex.SampleLevel(PointSampler, texcoord, 0, -int2(0, 1)).rgb, weights);
    float Lright = dot(colorTex.SampleLevel(PointSampler, texcoord, 0, int2(1, 0)).rgb, weights);
    float Lbottom  = dot(colorTex.SampleLevel(PointSampler, texcoord, 0, int2(0, 1)).rgb, weights);

    /**
     * We detect edges in gamma 1.0/2.0 space. Gamma space boosts the contrast
     * of the blacks, where the human vision system is more sensitive to small
     * gradations of intensity.
     */
    float4 delta = abs(sqrt(L).xxxx - sqrt(float4(Lleft, Ltop, Lright, Lbottom)));
    float4 edges = step(threshold.xxxx, delta);

    if (dot(edges, 1.0) == 0.0) {
        discard;
    }

    return edges;
}


float4 EdgeDetectionDepthPS(float4 position : SV_POSITION,
                            float2 texcoord : TEXCOORD0) : SV_TARGET {
    float D = depthTex.SampleLevel(PointSampler, texcoord, 0);
    float Dleft = depthTex.SampleLevel(PointSampler, texcoord, 0, -int2(1, 0));
    float Dtop  = depthTex.SampleLevel(PointSampler, texcoord, 0, -int2(0, 1));
    float Dright = depthTex.SampleLevel(PointSampler, texcoord, 0, int2(1, 0));
    float Dbottom  = depthTex.SampleLevel(PointSampler, texcoord, 0, int2(0, 1));

    float4 delta = abs(D.xxxx - float4(Dleft, Dtop, Dright, Dbottom));
    // Dividing by 10 give us results similar to the color-based detection
    float4 edges = step(threshold.xxxx / 10.0, delta);

    if (dot(edges, 1.0) == 0.0) {
        discard;
    }

    return edges;
}


float SearchXLeft(float2 texcoord) {
    texcoord -= float2(1.5, 0.0) * PIXEL_SIZE;
    float e = 0.0;
    // We offset by 0.5 to sample between edgels, thus fetching two in a row
    for (int i = 0; i < maxSearchSteps; i++) {
        e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).g;
        // We compare with 0.9 to prevent bilinear access precision problems
        [flatten] if (e < 0.9) break;
        texcoord -= float2(2.0, 0.0) * PIXEL_SIZE;
    }
    // When we exit the loop without founding the end, we want to return
    // -2 * maxSearchSteps
    return max(-2.0 * i - 2.0 * e, -2.0 * maxSearchSteps);
}

float SearchXRight(float2 texcoord) {
    texcoord += float2(1.5, 0.0) * PIXEL_SIZE;
    float e = 0.0;
    for (int i = 0; i < maxSearchSteps; i++) {
        e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).g;
        [flatten] if (e < 0.9) break;
        texcoord += float2(2.0, 0.0) * PIXEL_SIZE;
    }
    return min(2.0 * i + 2.0 * e, 2.0 * maxSearchSteps);
}

float SearchYUp(float2 texcoord) {
    texcoord -= float2(0.0, 1.5) * PIXEL_SIZE;
    float e = 0.0;
    for (int i = 0; i < maxSearchSteps; i++) {
        e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).r;
        [flatten] if (e < 0.9) break;
        texcoord -= float2(0.0, 2.0) * PIXEL_SIZE;
    }
    return max(-2.0 * i - 2.0 * e, -2.0 * maxSearchSteps);
}

float SearchYDown(float2 texcoord) {
    texcoord += float2(0.0, 1.5) * PIXEL_SIZE;
    float e = 0.0;
    for (int i = 0; i < maxSearchSteps; i++) {
        e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).r;
        [flatten] if (e < 0.9) break;
        texcoord += float2(0.0, 2.0) * PIXEL_SIZE;
    }
    return min(2.0 * i + 2.0 * e, 2.0 * maxSearchSteps);
}

float4 BlendingWeightCalculationPS(float4 position : SV_POSITION,
                                   float2 texcoord : TEXCOORD0) : SV_TARGET {
    float4 weights = 0.0;

    float2 e = edgesTex.SampleLevel(PointSampler, texcoord, 0).rg;

    [branch]
    if (e.g) { // Edge at north
        float2 d = float2(SearchXLeft(texcoord), SearchXRight(texcoord));
        
        // Instead of sampling between edgels, we sample at -0.25,
        // to be able to discern what value each edgel has.
        float4 coords = mad(float4(d.x, -0.25, d.y + 1.0, -0.25),
                            PIXEL_SIZE.xyxy, texcoord.xyxy);
        float e1 = edgesTex.SampleLevel(LinearSampler, coords.xy, 0).r;
        float e2 = edgesTex.SampleLevel(LinearSampler, coords.zw, 0).r;
        weights.rg = Area(abs(d), e1, e2);
    }

    [branch]
    if (e.r) { // Edge at west
        float2 d = float2(SearchYUp(texcoord), SearchYDown(texcoord));

        float4 coords = mad(float4(-0.25, d.x, -0.25, d.y + 1.0),
                            PIXEL_SIZE.xyxy, texcoord.xyxy);
        float e1 = edgesTex.SampleLevel(LinearSampler, coords.xy, 0).g;
        float e2 = edgesTex.SampleLevel(LinearSampler, coords.zw, 0).g;
        weights.ba = Area(abs(d), e1, e2);
    }

    return weights;
}


float4 NeighborhoodBlendingPS(float4 position : SV_POSITION,
                              float2 texcoord : TEXCOORD0) : SV_TARGET {
    float4 topLeft = blendTex.SampleLevel(PointSampler, texcoord, 0);
    float right = blendTex.SampleLevel(PointSampler, texcoord, 0, int2(0, 1)).g;
    float bottom = blendTex.SampleLevel(PointSampler, texcoord, 0, int2(1, 0)).a;
    float4 a = float4(topLeft.r, right, topLeft.b, bottom);

    float sum = dot(a, 1.0);

    [branch]
    if (sum > 0.0) {
        float4 o = a * PIXEL_SIZE.yyxx;
        float4 color = 0.0;
        color = mad(colorTex.SampleLevel(LinearSampler, texcoord + float2( 0.0, -o.r), 0), a.r, color);
        color = mad(colorTex.SampleLevel(LinearSampler, texcoord + float2( 0.0,  o.g), 0), a.g, color);
        color = mad(colorTex.SampleLevel(LinearSampler, texcoord + float2(-o.b,  0.0), 0), a.b, color);
        color = mad(colorTex.SampleLevel(LinearSampler, texcoord + float2( o.a,  0.0), 0), a.a, color);
        return color / sum;
    } else {
        return colorTex.SampleLevel(LinearSampler, texcoord, 0);
    }
}


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


technique10 EdgeDetection {
    pass EdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, EdgeDetectionPS()));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 EdgeDetectionDepth {
    pass EdgeDetectionDepth {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, EdgeDetectionDepthPS()));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 BlendingWeightCalculation {
    pass BlendingWeightCalculation {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, BlendingWeightCalculationPS()));

        SetDepthStencilState(DisableDepthUseStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 NeighborhoodBlending {
    pass NeighborhoodBlending {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, NeighborhoodBlendingPS()));

        SetDepthStencilState(DisableDepthUseStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
