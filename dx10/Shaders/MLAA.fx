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
#define MAX_DISTANCE 32
#endif


/**
 * Input vars and textures.
 */

cbuffer UpdatedOncePerFrame {
    /**
     * IMPORTANT: For maximum performance, 'maxSearchSteps' should be defined
     *            as a macro.
     */
    int maxSearchSteps;
    float threshold;
}

Texture2D colorTex;
Texture2D colorGammaTex;
Texture2D<float> depthTex;
Texture2D edgesTex;
Texture2D blendTex;
Texture2D<float2> areaTex;
Texture2D<float> searchLengthTex;


/**
 * Some sampler states ahead.
 */

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


/**
 * Typical Multiply-Add operation to ease translation to assembly code.
 */

float4 mad(float4 m, float4 a, float4 b) {
    return m * a + b;
}


/** 
 * Ok, we have the distance and both crossing edges, can you please return 
 * the float2 blending weights?
 */

float2 Area(float2 distance, float e1, float e2) {
     // * By dividing by areaSize - 1.0 below we are implicitly offsetting to
     //   always fall inside of a pixel
     // * Rounding prevents bilinear access precision problems
    float areaSize = MAX_DISTANCE * 5;
    float2 pixcoord = MAX_DISTANCE * round(4.0 * float2(e1, e2)) + distance;
    float2 texcoord = pixcoord / (areaSize - 1.0);
    return areaTex.SampleLevel(PointSampler, texcoord, 0).rg;
}


/**
 *  ~ D U M M Y   V E R T E X   S H A D E R ~
 */

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


/**
 *  1 S T   P A S S   ~   C O L O R   V E R S I O N
 */

float4 ColorEdgeDetectionPS(float4 position : SV_POSITION,
                            float2 texcoord : TEXCOORD0) : SV_TARGET {
    float3 weights = float3(0.2126,0.7152, 0.0722);

    /**
     * Luma calculation requires gamma-corrected colors, and thus 'colorGammaTex' should
     * be a non-sRGB texture.
     */
    float L = dot(colorGammaTex.SampleLevel(PointSampler, texcoord, 0).rgb, weights);
    float Lleft = dot(colorGammaTex.SampleLevel(PointSampler, texcoord, 0, -int2(1, 0)).rgb, weights);
    float Ltop  = dot(colorGammaTex.SampleLevel(PointSampler, texcoord, 0, -int2(0, 1)).rgb, weights);
    float Lright = dot(colorGammaTex.SampleLevel(PointSampler, texcoord, 0, int2(1, 0)).rgb, weights);
    float Lbottom  = dot(colorGammaTex.SampleLevel(PointSampler, texcoord, 0, int2(0, 1)).rgb, weights);

    // We do the usual threshold
    float4 delta = abs(L.xxxx - float4(Lleft, Ltop, Lright, Lbottom));
    float4 edges = step(threshold.xxxx, delta);

    // Then discard if there is no edge
    if (dot(edges, 1.0) == 0.0)
        discard;

    /**
     * Each edge with a delta in luma of less than 50% of the maximum luma
     * surrounding this pixel is discarded. This allows to eliminate spurious
     * crossing edges, and is based on the fact that, if there is too much
     * contrast in a direction, that will hide contrast in the other
     * neighbors.
     * This is done after the discard intentionally as this situation doesn't
     * happen too frequently (but it's important to do as it prevents some 
     * edges from going undetected).
     */
    float maxDelta = max(max(max(delta.x, delta.y), delta.z), delta.w);
    edges *= step(0.5 * maxDelta, delta);

    return edges;
}


/**
 *  1 S T   P A S S   ~   D E P T H   V E R S I O N
 */

float4 DepthEdgeDetectionPS(float4 position : SV_POSITION,
                            float2 texcoord : TEXCOORD0) : SV_TARGET {
    float D = depthTex.SampleLevel(PointSampler, texcoord, 0);
    float Dleft = depthTex.SampleLevel(PointSampler, texcoord, 0, -int2(1, 0));
    float Dtop  = depthTex.SampleLevel(PointSampler, texcoord, 0, -int2(0, 1));
    float Dright = depthTex.SampleLevel(PointSampler, texcoord, 0, int2(1, 0));
    float Dbottom  = depthTex.SampleLevel(PointSampler, texcoord, 0, int2(0, 1));

    float4 delta = abs(D.xxxx - float4(Dleft, Dtop, Dright, Dbottom));
    float4 edges = step(threshold.xxxx / 10.0, delta); // Dividing by 10 give us results similar to the color-based detection.

    if (dot(edges, 1.0) == 0.0)
        discard;

    return edges;
}


/**
 * This allows to determine how much length should add the last step of the
 * searchs. It takes the bilinearly interpolated value and returns 0, 1 or 2,
 * depending on which edges and crossing edges are active. 
 * "e.g" contains the bilinearly interpolated value for the edges and "e.r" for
 * the crossing edges.
 */

float DetermineLength(float2 e) {
	return 255.0 * searchLengthTex.SampleLevel(PointSampler, e, 0);
}


/**
 * Search functions for the 2nd pass.
 */

float SearchXLeft(float2 texcoord) {
    float2 e = edgesTex.SampleLevel(LinearSampler, texcoord - float2(0.0, 0.5) * PIXEL_SIZE, 0).rg;
    if (e.r > 0.25)
        return 0.0;

    // We offset by (1.25, 0.125) to sample between edgels, thus fetching four
    // in a row.
    // Sampling with different offsets in each direction allows to disambiguate
    // which edges are active from the four fetched ones.
    texcoord -= float2(1.25, 0.125) * PIXEL_SIZE;

    for (int i = 0; i < maxSearchSteps; i++) {
        e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).rg;

        // Is there some edge non activated? (e.g < 0.8281)
        // Or is there a crossing edge that breaks the line? (e.r > 0.0)
        // We refer you to the paper to discover where this magic number comes
        // from.
        [flatten] if (e.g < 0.8281 || e.r > 0.0) break;

        texcoord -= float2(2.0, 0.0) * PIXEL_SIZE;
    }

    // When we exit the loop without founding the end, we want to return
    // -2 * maxSearchSteps
    return max(-2.0 * i - DetermineLength(e), -2.0 * maxSearchSteps);
}

float SearchXRight(float2 texcoord) {
    float2 e = edgesTex.SampleLevel(LinearSampler, texcoord - float2(0.0, 0.5) * PIXEL_SIZE, 0).bg;
    if (e.r > 0.25)
        return 0.0;

    texcoord += float2(1.25, -0.125) * PIXEL_SIZE;
    for (int i = 0; i < maxSearchSteps; i++) {
        e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).bg;
        [flatten] if (e.g < 0.8281 || e.r > 0.0) break;
        texcoord += float2(2.0, 0.0) * PIXEL_SIZE;
    }

    return min(2.0 * i + DetermineLength(e), 2.0 * maxSearchSteps);
}

float SearchYUp(float2 texcoord) {
    float2 e = edgesTex.SampleLevel(LinearSampler, texcoord - float2(0.5, 0.0) * PIXEL_SIZE, 0).rg;
    if (e.g > 0.25)
        return 0.0;

    texcoord -= float2(0.125, 1.25) * PIXEL_SIZE;
    for (int i = 0; i < maxSearchSteps; i++) {
        e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).rg;
        [flatten] if (e.r < 0.8281 || e.g > 0.0) break;
        texcoord -= float2(0.0, 2.0) * PIXEL_SIZE;
    }

    return max(-2.0 * i - DetermineLength(e.gr), -2.0 * maxSearchSteps);
}

float SearchYDown(float2 texcoord) {
    float2 e = edgesTex.SampleLevel(LinearSampler, texcoord - float2(0.5, 0.0) * PIXEL_SIZE, 0).ra;
    if (e.g > 0.25)
        return 0.0;

    texcoord += float2(-0.125, 1.25) * PIXEL_SIZE;
    for (int i = 0; i < maxSearchSteps; i++) {
        e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).ra;
        [flatten] if (e.r < 0.8281 || e.g > 0.0) break;
        texcoord += float2(0.0, 2.0) * PIXEL_SIZE;
    }

    return min(2.0 * i + DetermineLength(e.gr), 2.0 * maxSearchSteps);
}


/**
 *  S E C O N D   P A S S
 */

float4 BlendingWeightCalculationPS(float4 position : SV_POSITION,
                                   float2 texcoord : TEXCOORD0) : SV_TARGET {
    float4 weights = 0.0;

    float2 e = edgesTex.SampleLevel(PointSampler, texcoord, 0).rg;

    [branch]
    if (e.g) { // Edge at north

        // Search distances to the left and to the right:
        float2 d = float2(SearchXLeft(texcoord), SearchXRight(texcoord));

        // Now fetch the crossing edges. Instead of sampling between edgels, we
        // sample at -0.25, to be able to discern what value each edgel has:
        float4 coords = mad(float4(d.x, -0.25, d.y + 1.0, -0.25),
                            PIXEL_SIZE.xyxy, texcoord.xyxy);
        float e1 = edgesTex.SampleLevel(LinearSampler, coords.xy, 0).r;
        float e2 = edgesTex.SampleLevel(LinearSampler, coords.zw, 0).r;

        // Ok, we know how this pattern looks like, now it is time for getting
        // the actual area:
        weights.rg = Area(abs(d), e1, e2);
    }

    [branch]
    if (e.r) { // Edge at west

        // Search distances to the top and to the bottom:
        float2 d = float2(SearchYUp(texcoord), SearchYDown(texcoord));
        
        // Now fetch the crossing edges (yet again):
        float4 coords = mad(float4(-0.25, d.x, -0.25, d.y + 1.0),
                            PIXEL_SIZE.xyxy, texcoord.xyxy);
        float e1 = edgesTex.SampleLevel(LinearSampler, coords.xy, 0).g;
        float e2 = edgesTex.SampleLevel(LinearSampler, coords.zw, 0).g;

        // Get the area for this direction:
        weights.ba = Area(abs(d), e1, e2);
    }

    return saturate(weights);
}


/**
 *  T H I R D   P A S S
 */

float4 NeighborhoodBlendingPS(float4 position : SV_POSITION,
                              float2 texcoord : TEXCOORD0) : SV_TARGET {
    // Fetch the blending weights for current pixel:
    float4 topLeft = blendTex.SampleLevel(PointSampler, texcoord, 0);
    float bottom = blendTex.SampleLevel(PointSampler, texcoord, 0, int2(0, 1)).g;
    float right = blendTex.SampleLevel(PointSampler, texcoord, 0, int2(1, 0)).a;
    float4 a = float4(topLeft.r, bottom, topLeft.b, right);

    // Up to 4 lines can be crossing a pixel (one in each edge). So, we perform
    // a weighted average, where the weight of each line is 'a' cubed, which
    // favors blending and works well in practice.
    float4 w = a * a * a;

    // Is there any blending weight with a value greater than 0.0?
    float sum = dot(w, 1.0);
    if (sum < 1e-5)
        discard;

    float4 color = 0.0;

    // Add the contributions of the 4 possible lines that can cross this
    // pixel:
    float4 coords = mad(float4( 0.0, -a.r, 0.0,  a.g), PIXEL_SIZE.yyyy, texcoord.xyxy);
    color = mad(colorTex.SampleLevel(LinearSampler, coords.xy, 0), w.r, color);
    color = mad(colorTex.SampleLevel(LinearSampler, coords.zw, 0), w.g, color);

    coords = mad(float4(-a.b,  0.0, a.a,  0.0), PIXEL_SIZE.xxxx, texcoord.xyxy);
    color = mad(colorTex.SampleLevel(LinearSampler, coords.xy, 0), w.b, color);
    color = mad(colorTex.SampleLevel(LinearSampler, coords.zw, 0), w.a, color);

    // Normalize the resulting color and we are finished!
    return color / sum;
}


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
 * Time for some techniques!
 */

technique10 ColorEdgeDetection {
    pass ColorEdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, ColorEdgeDetectionPS()));

        SetDepthStencilState(DisableDepthReplaceStencil, 1);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 DepthEdgeDetection {
    pass DepthEdgeDetection {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, DepthEdgeDetectionPS()));

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
