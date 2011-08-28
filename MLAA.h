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


/**
 * Hi, welcome aboard!
 * 
 * Here you will find instructions to get the shader up and running as fast as
 * possible.
 *
 * The shader has three passes, chained together as follows:
 *
 *                           |input|------------------·
 *                              v                     |
 *                    [ MLAA*EdgeDetection ]          |
 *                              v                     |
 *                          |edgesTex|                |
 *                              v                     |
 *              [ MLAABlendingWeightCalculation ]     |
 *                              v                     |
 *                          |blendTex|                |
 *                              v                     |
 *                [ MLAANeighborhoodBlending ] <------·
 *                              v
 *                           |output|
 *
 * Note that each [pass] has its own vertex and pixel shader.
 *
 * You have three edge detection methods to choose from: luma, color or depth.
 * They represent different quality/performance and anti-aliasing/sharpness
 * tradeoffs, so our recommendation is for you to choose the one that suits
 * better your particular scenario:
 *
 * - Depth edge detection is usually the faster but it may miss some edges.
 * - Luma edge detection is usually more expensive than depth edge detection,
 *   but catches visible edges that depth edge detection can miss.
 * - Color edge detection is usually the most expensive one but catches
 *   chroma-only edges.
 *
 * Ok then, let's go!
 *
 * - The first step is to create two RGBA temporal framebuffers for holding 
 *   'edgesTex' and 'blendTex'. In DX10, you can use a RG framebuffer for the
 *   edges texture. 
 *
 * - Both temporal framebuffers 'edgesTex' and 'blendTex' must be cleared each
 *   frame. Do not forget to clear the alpha channel!
 *
 * - The next step is loading the two supporting precalculated textures,
 *   'areaTex' (RG) and 'searchTex' (R). They are needed for the 
 *   'MLAABlendingWeightCalculation' pass.
 *
 * - In DX9, all samplers must be set to linear filtering and clamp, with the
 *   exception of 'searchTex', that must be set to point filtering.
 *
 * - All texture reads and buffer writes must be non-sRGB, with the exception
 *   of the input read and the output write of input in 
 *   'MLAANeighborhoodBlending' (and only in this pass!). If sRGB reads in this
 *   last pass are not possible, the technique will work anyways, but will 
 *   perform antialiasing in gamma space. Note that for best results the input
 *   read for the edge detection should *NOT* be sRGB.
 *
 * - Before including MLAA.h you have to setup the framebuffer pixel size. For
 *   example:
 *       #define MLAA_PIXEL_SIZE float2(1.0 / 1280.0, 1.0 / 720.0)
 *
 * - Also, you will have to set MLAA_HLSL_3 or MLAA_HLSL_4 to 1 depending on
 *   the platform.
 *
 * - Then, you will have to setup the passes as indicated in the scheme above.
 *   You can take a look into MLAA.fx, to see how we did it for our demo.
 *   Checkout the function wrappers, you may want to copy-paste them!
 *
 * - It's recommended to validate the produced |edgesTex| and |blendTex|. It's
 *   advised to not continue with the implementation until both buffers are
 *   verified to produce identical results to our reference demo.
 *
 * - After you have get the last pass to work, it's time to optimize. You will
 *   have to initialize a stencil buffer in the first pass (discard is already
 *   in the code), then just mask execution by using it the second pass. The
 *   last one should be executed in all pixels.
 *
 * That is!
 */


/**
 * MLAA_THRESHOLD specifices the threshold or sensivity to edges.
 * Lowering this value you will be able to detect more edges at the expense of
 * performance. 
 * 0.1 is a reasonable value, and allows to catch most visible edges.
 * 0.05 is a rather overkill value, that allows to catch 'em all.
 */
#ifndef MLAA_THRESHOLD
#define MLAA_THRESHOLD 0.1
#endif

/**
 * MLAA_MAX_SEARCH_STEPS specifies the maximum steps performed in the line
 * searchs, at each side. In number of pixels, it's actually the double.
 * So the maximum line lengtly perfectly handled by, for example 16, is
 * 64 (by perfectly, we meant that longer lines won't look as good, but
 * still antialiased).
 */
#ifndef MLAA_MAX_SEARCH_STEPS
#define MLAA_MAX_SEARCH_STEPS 16
#endif

/**
 * Here we have an interesting define. In the last pass we make usage of 
 * bilinear filtering to avoid some lerps; however, bilinear filtering
 * in DX9, under DX9 hardware (but not in DX9 code running on DX10 hardware)
 * is done in gamma space, which gives inaccurate results. 
 *
 * So, if you are in DX9, under DX9 hardware, and do you want accurate linear
 * blending, you must set this flag to 1.
 *
 * It's ignored when using MLAA_HLSL_4, and of course, only has sense when
 * using sRGB read and writes on the last pass.
 */
#ifndef MLAA_DIRECTX9_LINEAR_BLEND
#define MLAA_DIRECTX9_LINEAR_BLEND 0
#endif


/**
 * This is actually not configurable, it's the size of the precomputed areatex.
 */
#ifndef MLAA_MAX_DISTANCE
#define MLAA_MAX_DISTANCE 15
#endif
#define MLAA_AREATEX_PIXEL_SIZE (1.0 / (MLAA_MAX_DISTANCE * 5))


/**
 * Porting functions.
 */
#if MLAA_HLSL_3 == 1
#define MLAATexture2D sampler2D
#define MLAASampleLevelZero(tex, coord) tex2Dlod(tex, float4(coord, 0.0, 0.0))
#define MLAASampleLevelZeroPoint(tex, coord) tex2Dlod(tex, float4(coord, 0.0, 0.0))
#define MLAASample(tex, coord) tex2D(tex, coord)
#define MLAASampleLevelZeroOffset(tex, coord, off) tex2Dlod(tex, float4(coord + off * MLAA_PIXEL_SIZE, 0.0, 0.0))
#define MLAASampleOffset(tex, coord, off) tex2D(tex, coord + off * MLAA_PIXEL_SIZE)
#elif MLAA_HLSL_4 == 1
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
#define MLAATexture2D Texture2D
#define MLAASampleLevelZero(tex, coord) tex.SampleLevel(LinearSampler, coord, 0)
#define MLAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(PointSampler, coord, 0)
#define MLAASample(tex, coord) MLAASampleLevelZero(tex, coord)
#define MLAASampleLevelZeroOffset(tex, coord, off) tex.SampleLevel(LinearSampler, coord, 0, off)
#define MLAASampleOffset(tex, coord, off) MLAASampleLevelZeroOffset(tex, coord, off)
#endif


/**
 * Typical Multiply-Add operation to ease translation to assembly code.
 */
float4 MLAAMad(float4 m, float4 a, float4 b) {
    return m * a + b;
}


/**
 * Edge Detection Vertex Shader
 */
void MLAAEdgeDetectionVS(float4 position,
                         out float4 svPosition,
                         inout float2 texcoord,
                         out float4 offset[2]) {
    svPosition = position;

    offset[0] = texcoord.xyxy + MLAA_PIXEL_SIZE.xyxy * float4(-1.0, 0.0, 0.0, -1.0);
    offset[1] = texcoord.xyxy + MLAA_PIXEL_SIZE.xyxy * float4( 1.0, 0.0, 0.0,  1.0);
}

/**
 * Blend Weight Calculation Vertex Shader
 */
void MLAABlendWeightCalculationVS(float4 position,
                                  out float4 svPosition,
                                  inout float2 texcoord,
                                  out float2 pixcoord,
                                  out float4 offset[3]) {
    svPosition = position;

    pixcoord = texcoord / MLAA_PIXEL_SIZE;

    // We will use these offsets for the searchs later on (see @PSEUDO_GATHER4):
    offset[0] = texcoord.xyxy + MLAA_PIXEL_SIZE.xyxy * float4(-0.25, -0.125,  1.25, -0.125);
    offset[1] = texcoord.xyxy + MLAA_PIXEL_SIZE.xyxy * float4(-0.125, -0.25, -0.125,  1.25);

    // And these for the searchs, they indicate the ends of the loops:
    offset[2] = float4(offset[0].xz, offset[1].yw) + float4(-2.0, 2.0, -2.0, 2.0) * MLAA_PIXEL_SIZE.xxyy * MLAA_MAX_SEARCH_STEPS;
}

/**
 * Neighborhood Blending Vertex Shader
 */
void MLAANeighborhoodBlendingVS(float4 position,
                                out float4 svPosition,
                                inout float2 texcoord,
                                out float4 offset[2]) {
    svPosition = position;

    offset[0] = texcoord.xyxy + MLAA_PIXEL_SIZE.xyxy * float4(-1.0, 0.0, 0.0, -1.0);
    offset[1] = texcoord.xyxy + MLAA_PIXEL_SIZE.xyxy * float4( 1.0, 0.0, 0.0,  1.0);
}


/**
 *  Luma Edge Detection
 */
float4 MLAALumaEdgeDetectionPS(float2 texcoord,
                               float4 offset[2],
                               MLAATexture2D colorGammaTex) {
    float3 weights = float3(0.2126,0.7152, 0.0722);

    /**
     * Luma calculation requires gamma-corrected colors, and thus 'colorGammaTex'
     * should be a non-sRGB texture.
     */
    float L = dot(MLAASample(colorGammaTex, texcoord).rgb, weights);
    float Lleft = dot(MLAASample(colorGammaTex, offset[0].xy).rgb, weights);
    float Ltop  = dot(MLAASample(colorGammaTex, offset[0].zw).rgb, weights);
    float Lright = dot(MLAASample(colorGammaTex, offset[1].xy).rgb, weights);
    float Lbottom  = dot(MLAASample(colorGammaTex, offset[1].zw).rgb, weights);

    // We do the usual threshold
    float4 delta = abs(L.xxxx - float4(Lleft, Ltop, Lright, Lbottom));
    float4 edges = step(MLAA_THRESHOLD.xxxx, delta);

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
 *  Color Edge Detection
 */
float4 MLAAColorEdgeDetectionPS(float2 texcoord,
                                float4 offset[2],
                                MLAATexture2D colorGammaTex) {
    float4 delta;

    /**
     * Just like the lumas edge detection, we prefer non-sRGB textures over 
     * here.
     */
    float3 C = MLAASample(colorGammaTex, texcoord).rgb;

    float3 Cleft = MLAASample(colorGammaTex, offset[0].xy).rgb;
    float3 t = abs(C - Cleft);
    delta.x = max(max(t.r, t.g), t.b);

    float3 Ctop  = MLAASample(colorGammaTex, offset[0].zw).rgb;
    t = abs(C - Ctop);
    delta.y = max(max(t.r, t.g), t.b);

    float3 Cright = MLAASample(colorGammaTex, offset[1].xy).rgb;
    t = abs(C - Cright);
    delta.z = max(max(t.r, t.g), t.b);

    float3 Cbottom  = MLAASample(colorGammaTex, offset[1].zw).rgb;
    t = abs(C - Cbottom);
    delta.w = max(max(t.r, t.g), t.b);

    // We do the usual threshold
    float4 edges = step(MLAA_THRESHOLD.xxxx, delta);

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
 * Depth Edge Detection
 */
float4 MLAADepthEdgeDetectionPS(float2 texcoord,
                                float4 offset[2],
                                MLAATexture2D depthTex) {
    float D = MLAASample(depthTex, texcoord).r;
    float Dleft = MLAASample(depthTex, offset[0].xy).r;
    float Dtop  = MLAASample(depthTex, offset[0].zw).r;
    float Dright = MLAASample(depthTex, offset[1].xy).r;
    float Dbottom  = MLAASample(depthTex, offset[1].zw).r;

    float4 delta = abs(D.xxxx - float4(Dleft, Dtop, Dright, Dbottom));
    float4 edges = step(MLAA_THRESHOLD.xxxx / 10.0, delta); // Dividing by 10 give us results similar to the color-based detection.

    if (dot(edges, 1.0) == 0.0)
        discard;

    return edges;
}


/**
 * This allows to determine how much length should we add in the last step
 * of the searchs. It takes the bilinearly interpolated edge (see 
 * @PSEUDO_GATHER4), and adds 0, 1 or 2, depending on which edges and
 * crossing edges are active.
 */
float MLAASearchLength(MLAATexture2D searchTex, float2 e, float bias, float scale) {
    // Not required if searchTex accesses are set to point:
    // float2 SEARCH_TEX_PIXEL_SIZE = 1.0 / float2(66.0, 33.0);
    // e = float2(bias, 0.0) + 0.5 * SEARCH_TEX_PIXEL_SIZE + e * float2(scale, 1.0) * float2(64.0, 32.0) * SEARCH_TEX_PIXEL_SIZE;
    e.r = bias + e.r * scale;
    return 255.0 * MLAASampleLevelZeroPoint(searchTex, e).r;
}


/**
 * Search functions for the 2nd pass.
 */
float MLAASearchXLeft(MLAATexture2D edgesTex, MLAATexture2D searchTex, float2 texcoord, float end) {
    /**
     * @PSEUDO_GATHER4
     * This texcoord has been offset by (-0.25, -0.125) in the vertex shader to
     * sample between edge, thus fetching four edges in a row.
     * Sampling with different offsets in each direction allows to disambiguate
     * which edges are active from the four fetched ones.
     */
    float2 e = float2(0.0, 1.0);
    while (texcoord.x > end && 
           e.g > 0.8281 && // Is there some edge not activated?
           e.r == 0.0) { // Or is there a crossing edge that breaks the line?
        e = MLAASampleLevelZero(edgesTex, texcoord).rg;
        texcoord -= float2(2.0, 0.0) * MLAA_PIXEL_SIZE;
    }

    // We correct the previous (-0.25, -0.125) offset we applied:
    texcoord.x += 0.25 * MLAA_PIXEL_SIZE.x;

    // The searchs are bias by 1, so adjust the coords accordingly:
    texcoord.x += MLAA_PIXEL_SIZE.x;

    // Disambiguate the length added by the last step:
    texcoord.x += 2.0 * MLAA_PIXEL_SIZE.x; // Undo last step
    texcoord.x -= MLAA_PIXEL_SIZE.x * MLAASearchLength(searchTex, e, 0.0, 0.5);

    return texcoord.x;
}

float MLAASearchXRight(MLAATexture2D edgesTex, MLAATexture2D searchTex, float2 texcoord, float end) {
    float2 e = float2(0.0, 1.0);
    while (texcoord.x < end && 
           e.g > 0.8281 && // Is there some edge not activated?
           e.r == 0.0) { // Or is there a crossing edge that breaks the line?
        e = MLAASampleLevelZero(edgesTex, texcoord).rg;
        texcoord += float2(2.0, 0.0) * MLAA_PIXEL_SIZE;
    }

    texcoord.x -= 0.25 * MLAA_PIXEL_SIZE.x;
    texcoord.x -= MLAA_PIXEL_SIZE.x;
    texcoord.x -= 2.0 * MLAA_PIXEL_SIZE.x;
    texcoord.x += MLAA_PIXEL_SIZE.x * MLAASearchLength(searchTex, e, 0.5, 0.5);
    return texcoord.x;
}

float MLAASearchYUp(MLAATexture2D edgesTex, MLAATexture2D searchTex, float2 texcoord, float end) {
    float2 e = float2(1.0, 0.0);
    while (texcoord.y > end && 
           e.r > 0.8281 && // Is there some edge not activated?
           e.g == 0.0) { // Or is there a crossing edge that breaks the line?
        e = MLAASampleLevelZero(edgesTex, texcoord).rg;
        texcoord -= float2(0.0, 2.0) * MLAA_PIXEL_SIZE;
    }

    texcoord.y += 0.25 * MLAA_PIXEL_SIZE.y;
    texcoord.y += MLAA_PIXEL_SIZE.y;
    texcoord.y += 2.0 * MLAA_PIXEL_SIZE.y;
    texcoord.y -= MLAA_PIXEL_SIZE.y * MLAASearchLength(searchTex, e.gr, 0.0, 0.5);
    return texcoord.y;
}

float MLAASearchYDown(MLAATexture2D edgesTex, MLAATexture2D searchTex, float2 texcoord, float end) {
    float2 e = float2(1.0, 0.0);
    while (texcoord.y < end && 
           e.r > 0.8281 && // Is there some edge not activated?
           e.g == 0.0) { // Or is there a crossing edge that breaks the line?
        e = MLAASampleLevelZero(edgesTex, texcoord).rg;
        texcoord += float2(0.0, 2.0) * MLAA_PIXEL_SIZE;
    }
    
    texcoord.y -= 0.25 * MLAA_PIXEL_SIZE.y;
    texcoord.y -= MLAA_PIXEL_SIZE.y;
    texcoord.y -= 2.0 * MLAA_PIXEL_SIZE.y;
    texcoord.y += MLAA_PIXEL_SIZE.y * MLAASearchLength(searchTex, e.gr, 0.5, 0.5);
    return texcoord.y;
}


/** 
 * Ok, we have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
float2 MLAAArea(MLAATexture2D areaTex, float2 distance, float e1, float e2) {
    // Rounding prevents precision errors of bilinear filtering:
    float2 texcoord = MLAA_MAX_DISTANCE * round(4.0 * float2(e1, e2)) + distance;
    
    // We do a scale and bias for mapping to texel space:
    texcoord = MLAA_AREATEX_PIXEL_SIZE * texcoord + (0.5 * MLAA_AREATEX_PIXEL_SIZE);

    // Do it!
    return MLAASampleLevelZero(areaTex, texcoord).rg;
}


/**
 * Blending Weight Calculation
 */
float4 MLAABlendingWeightCalculationPS(float2 texcoord,
                                       float2 pixcoord,
                                       float4 offset[3],
                                       MLAATexture2D edgesTex, 
                                       MLAATexture2D areaTex, 
                                       MLAATexture2D searchTex) {
    float4 weights = 0.0;

    float2 e = MLAASample(edgesTex, texcoord).rg;

    [branch]
    if (e.g) { // Edge at north
        float2 d;

        // Find the distance to the left:
        float2 coords;
        coords.x = MLAASearchXLeft(edgesTex, searchTex, offset[0].xy, offset[2].x);
        coords.y = offset[1].y; // offset[1].y = texcoord.y - 0.25 * MLAA_PIXEL_SIZE.y (@CROSSING_OFFSET)
        d.x = coords.x;

        // Now fetch the left crossing edges, two at a time using bilinear
        // filtering. Sampling at -0.25 (see @CROSSING_OFFSET) enables to
        // discern what value each edge has:
        float e1 = MLAASampleLevelZero(edgesTex, coords).r;

        // Find the distance to the right:
        coords.x = MLAASearchXRight(edgesTex, searchTex, offset[0].zw, offset[2].y);
        d.y = coords.x;

        // We want the distances to be in pixel units (doing this here allow to
        // better interleave arithmetic and memory accesses):
        d = d / MLAA_PIXEL_SIZE.x - pixcoord.x;

        // MLAAArea below needs a sqrt, as the areas texture is compressed 
        // quadratically:
        d = sqrt(abs(d));

        // Fetch the right crossing edges:
        float e2 = MLAASampleLevelZeroOffset(edgesTex, coords, int2(1, 0)).r;

        // Ok, we know how this pattern looks like, now it is time for getting
        // the actual area:
        weights.rg = MLAAArea(areaTex, d, e1, e2);
    }

    [branch]
    if (e.r) { // Edge at west
        float2 d;
        
        // Find the distance to the top:
        float2 coords;
        coords.y = MLAASearchYUp(edgesTex, searchTex, offset[1].xy, offset[2].z);
        coords.x = offset[0].x; // offset[1].x = texcoord.x - 0.25 * MLAA_PIXEL_SIZE.x;
        d.x = coords.y;

        // Fetch the top crossing edges:
        float e1 = MLAASampleLevelZero(edgesTex, coords).g;

        // Find the distance to the bottom:
        coords.y = MLAASearchYDown(edgesTex, searchTex, offset[1].zw, offset[2].w);
        d.y = coords.y;

        // We want the distances to be in pixel units:
        d = d / MLAA_PIXEL_SIZE.y - pixcoord.y;

        // MLAAArea below needs a sqrt, as the areas texture is compressed 
        // quadratically:
        d = sqrt(abs(d));

        // Fetch the bottom crossing edges:
        float e2 = MLAASampleLevelZeroOffset(edgesTex, coords, int2(0, 1)).g;

        // Get the area for this direction:
        weights.ba = MLAAArea(areaTex, d, e1, e2);
    }

    return weights;
}


/**
 * Neighborhood Blending
 */
float4 MLAANeighborhoodBlendingPS(float2 texcoord,
                                  float4 offset[2],
                                  MLAATexture2D colorTex,
                                  MLAATexture2D blendTex) {
    // Fetch the blending weights for current pixel:
    float4 topLeft = MLAASample(blendTex, texcoord);
    float bottom = MLAASample(blendTex, offset[1].zw).g;
    float right = MLAASample(blendTex, offset[1].xy).a;
    float4 a = float4(topLeft.r, bottom, topLeft.b, right);

    // Up to 4 lines can be crossing a pixel (one in each edge). So, we perform
    // a weighted average, where the weight of each line is 'a' cubed, which
    // favors blending and works well in practice.
    float4 w = a * a * a;

    // Is there any blending weight with a value greater than 0.0?
    float sum = dot(w, 1.0);
    if (sum < 1e-5)
        return MLAASample(colorTex, texcoord);

    float4 color = 0.0;

    // Add the contributions of the 4 possible lines that can cross this
    // pixel:
    #if MLAA_HLSL_4 == 1 || MLAA_DIRECTX9_LINEAR_BLEND == 0
        float4 coords = MLAAMad(float4( 0.0, -a.r, 0.0,  a.g), MLAA_PIXEL_SIZE.yyyy, texcoord.xyxy);
        color = MLAAMad(MLAASample(colorTex, coords.xy), w.r, color);
        color = MLAAMad(MLAASample(colorTex, coords.zw), w.g, color);

        coords = MLAAMad(float4(-a.b,  0.0, a.a,  0.0), MLAA_PIXEL_SIZE.xxxx, texcoord.xyxy);
        color = MLAAMad(MLAASample(colorTex, coords.xy), w.b, color);
        color = MLAAMad(MLAASample(colorTex, coords.zw), w.a, color);
    #else
        float4 C = MLAASample(colorTex, texcoord);
        float4 Cleft = MLAASample(colorTex, offset[0].xy);
        float4 Ctop = MLAASample(colorTex, offset[0].zw);
        float4 Cright = MLAASample(colorTex, offset[1].xy);
        float4 Cbottom = MLAASample(colorTex, offset[1].zw);
        color = MLAAMad(lerp(C, Ctop, a.r), w.r, color);
        color = MLAAMad(lerp(C, Cbottom, a.g), w.g, color);
        color = MLAAMad(lerp(C, Cleft, a.b), w.b, color);
        color = MLAAMad(lerp(C, Cright, a.a), w.a, color);
    #endif

    // Normalize the resulting color and we are finished!
    return color / sum;
}
