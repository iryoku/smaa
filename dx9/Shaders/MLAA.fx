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


// Just for checking syntax at compile time
#if !defined(PIXEL_SIZE)
#define PIXEL_SIZE float2(1.0 / 1280.0, 1.0 / 720.0)
#define MAX_SEARCH_STEPS 8    
#define MAX_DISTANCE 32
#endif

/**
 * Here we have an interesting define. In the last pass we make usage of 
 * bilinear filtering to avoid some lerps; however, bilinear filtering
 * in DX9, under DX9 hardware (but not in DX9 code running on DX10 hardware)
 * is done in gamma space, which gives sustantially worser results. So, this
 * flag allows to avoid the bilinear filter trick, changing it with some 
 * software lerps.
 *
 * So, to summarize, it is safe to use the bilinear filter trick when you are
 * using DX10 hardware on DX9. However, for the best results when using DX9
 * hardware, it is recommended comment this line.
 */

#define BILINEAR_FILTER_TRICK


/**
 * Input vars and textures.
 */

float threshold;
texture2D colorTex;
texture2D depthTex;
texture2D edgesTex;
texture2D blendTex;
texture2D areaTex;
texture2D searchLengthTex;


/**
 * DX9 samplers hell following this.
 */

sampler2D colorMap {
    Texture = <colorTex>;
    AddressU  = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = true;
};

sampler2D colorMapL {
    Texture = <colorTex>;
    AddressU  = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = true;
};

sampler2D colorMapG {
    Texture = <colorTex>;
    AddressU  = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = false;
};

sampler2D depthMap {
    Texture = <depthTex>;
    AddressU  = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = false;
};

sampler2D edgesMap {
    Texture = <edgesTex>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = false;
};

sampler2D edgesMapL {
    Texture = <edgesTex>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Linear; MagFilter = Linear;
    SRGBTexture = false;
};

sampler2D blendMap {
    Texture = <blendTex>;
    AddressU = Clamp; AddressV = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = false;
};

sampler2D areaMap {
    Texture = <areaTex>;
    AddressU = Clamp; AddressV = Clamp; AddressW = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = false;
};

sampler2D searchLengthMap {
    Texture = <searchLengthTex>;
    AddressU = Clamp; AddressV = Clamp; AddressW = Clamp;
    MipFilter = Point; MinFilter = Point; MagFilter = Point;
    SRGBTexture = false;
};


/**
 * Typical Multiply-Add operation to ease translation to assembly code.
 */

float4 mad(float4 m, float4 a, float4 b) {
    #if defined(XBOX)
    float4 result;
    asm {
        mad result, m, a, b
    };
    return result;
    #else
    return m * a + b;
    #endif
}


/**
 * This one just returns the first level of a mip map chain, which allow us to
 * avoid the nasty ddx/ddy warnings, even improving the performance a little 
 * bit.
 */

float4 tex2Dlevel0(sampler2D map, float2 texcoord) {
    return tex2Dlod(map, float4(texcoord, 0.0, 0.0));
}


/** 
 * Ok, we have the distance and both crossing edges, can you please return 
 * the float2 blending weights?
 */

float2 Area(float2 distance, float e1, float e2) {
     // * By dividing by areaSize - 1.0 below we are implicitly offsetting to
     //   always fall inside of a pixel
     // * Rounding prevents bilinear access precision problems
    float areaSize = MAX_DISTANCE * 5.0;
    float2 pixcoord = MAX_DISTANCE * round(4.0 * float2(e1, e2)) + distance;
    float2 texcoord = pixcoord / (areaSize - 1.0);
    return tex2Dlevel0(areaMap, texcoord).ra;
}


/**
 *  V E R T E X   S H A D E R S
 */

void PassThroughVS(inout float4 position : POSITION0,
                   inout float2 texcoord : TEXCOORD0) {
}

void OffsetVS(inout float4 position : POSITION0,
              inout float2 texcoord : TEXCOORD0,
              out float4 offset[2] : TEXCOORD1) {
    offset[0] = texcoord.xyxy + PIXEL_SIZE.xyxy * float4(-1.0, 0.0, 0.0, -1.0);
    offset[1] = texcoord.xyxy + PIXEL_SIZE.xyxy * float4( 1.0, 0.0, 0.0,  1.0);
}


/**
 *  1 S T   P A S S   ~   C O L O R   V E R S I O N
 */

float4 ColorEdgeDetectionPS(float2 texcoord : TEXCOORD0,
                            float4 offset[2]: TEXCOORD1) : COLOR0 {
    float3 weights = float3(0.2126,0.7152, 0.0722); // These ones are from the ITU-R Recommendation BT. 709

    /**
     * Luma calculation requires gamma-corrected colors:
     */
    float L = dot(tex2D(colorMapG, texcoord).rgb, weights);
    float Lleft = dot(tex2D(colorMapG, offset[0].xy).rgb, weights);
    float Ltop = dot(tex2D(colorMapG, offset[0].zw).rgb, weights);  
    float Lright = dot(tex2D(colorMapG, offset[1].xy).rgb, weights);
    float Lbottom = dot(tex2D(colorMapG, offset[1].zw).rgb, weights);

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

float4 DepthEdgeDetectionPS(float2 texcoord : TEXCOORD0,
                            float4 offset[2]: TEXCOORD1) : COLOR0 {
    float D = tex2D(depthMap, texcoord).r;
    float Dleft = tex2D(depthMap, offset[0].xy).r;
    float Dtop  = tex2D(depthMap, offset[0].zw).r;
    float Dright = tex2D(depthMap, offset[1].xy).r;
    float Dbottom = tex2D(depthMap, offset[1].zw).r;

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
    return 255.0 * tex2Dlevel0(searchLengthMap, e).r;
}


/**
 * Search functions for the 2nd pass.
 */

float SearchXLeft(float2 texcoord) {
    float2 e = tex2Dlevel0(edgesMapL, texcoord - float2(0.0, 0.5) * PIXEL_SIZE).rg;
    if (e.r > 0.25)
        return 0.0;

    // We offset by (1.25, 0.125) to sample between edgels, thus fetching four
    // in a row.
    // Sampling with different offsets in each direction allows to disambiguate
    // which edges are active from the four fetched ones.
    texcoord -= float2(1.25, 0.125) * PIXEL_SIZE;

    for (int i = 0; i < MAX_SEARCH_STEPS; i++) {
        e = tex2Dlevel0(edgesMapL, texcoord).rg;

        // Is there some edge non activated? (e.g < 0.8281)
        // Or is there a crossing edge that breaks the line? (e.r > 0.0)
        // We refer you to the paper to discover where this magic number comes
        // from.
        [flatten] if (e.g < 0.8281 || e.r > 0.0) break;

        texcoord -= float2(2.0, 0.0) * PIXEL_SIZE;
    }

    // When we exit the loop without founding the end, we want to return
    // -2 * MAX_SEARCH_STEPS
    return max(-2.0 * i - DetermineLength(e), -2.0 * MAX_SEARCH_STEPS);
}


float SearchXRight(float2 texcoord) {
    float2 e = tex2Dlevel0(edgesMapL, texcoord - float2(0.0, 0.5) * PIXEL_SIZE).bg;
    if (e.r > 0.25)
        return 0.0;

    texcoord += float2(1.25, -0.125) * PIXEL_SIZE;
    for (int i = 0; i < MAX_SEARCH_STEPS; i++) {
        e = tex2Dlevel0(edgesMapL, texcoord).bg;
        [flatten] if (e.g < 0.8281 || e.r > 0.0) break;
        texcoord += float2(2.0, 0.0) * PIXEL_SIZE;
    }

    return min(2.0 * i + DetermineLength(e), 2.0 * MAX_SEARCH_STEPS);
}


float SearchYUp(float2 texcoord) {
    float2 e = tex2Dlevel0(edgesMapL, texcoord - float2(0.5, 0.0) * PIXEL_SIZE).rg;
    if (e.g > 0.25)
        return 0.0;

    texcoord -= float2(0.125, 1.25) * PIXEL_SIZE;
    for (int i = 0; i < MAX_SEARCH_STEPS; i++) {
        e = tex2Dlevel0(edgesMapL, texcoord).rg;
        [flatten] if (e.r < 0.8281 || e.g > 0.0) break;
        texcoord -= float2(0.0, 2.0) * PIXEL_SIZE;
    }

    return max(-2.0 * i - DetermineLength(e.gr), -2.0 * MAX_SEARCH_STEPS);
}


float SearchYDown(float2 texcoord) {
    float2 e = tex2Dlevel0(edgesMapL, texcoord - float2(0.5, 0.0) * PIXEL_SIZE).ra;
    if (e.g > 0.25)
        return 0.0;

    texcoord += float2(-0.125, 1.25) * PIXEL_SIZE;
    for (int i = 0; i < MAX_SEARCH_STEPS; i++) {
        e = tex2Dlevel0(edgesMapL, texcoord).ra;
        [flatten] if (e.r < 0.8281 || e.g > 0.0) break;
        texcoord += float2(0.0, 2.0) * PIXEL_SIZE;
    }

    return min(2.0 * i + DetermineLength(e.gr), 2.0 * MAX_SEARCH_STEPS);
}


/**
 *  2 N D   P A S S
 */

float4 BlendWeightCalculationPS(float2 texcoord : TEXCOORD0) : COLOR0 {
    float4 areas = 0.0;

    float2 e = tex2D(edgesMap, texcoord).rg;

    [branch]
    if (e.g) { // Edge at north

        // Search distances to the left and to the right:
        float2 d = float2(SearchXLeft(texcoord), SearchXRight(texcoord));

        // Now fetch the crossing edges. Instead of sampling between edgels, we
        // sample at -0.25, to be able to discern what value each edgel has:
        float4 coords = mad(float4(d.x, -0.25, d.y + 1.0, -0.25),
                            PIXEL_SIZE.xyxy, texcoord.xyxy);
        float e1 = tex2Dlevel0(edgesMapL, coords.xy).r;
        float e2 = tex2Dlevel0(edgesMapL, coords.zw).r;

        // Ok, we know how this pattern looks like, now it is time for getting
        // the actual area:
        areas.rg = Area(abs(d), e1, e2);
    }

    [branch]
    if (e.r) { // Edge at west

        // Search distances to the top and to the bottom:
        float2 d = float2(SearchYUp(texcoord), SearchYDown(texcoord));

        // Now fetch the crossing edges (yet again):
        float4 coords = mad(float4(-0.25, d.x, -0.25, d.y + 1.0),
                            PIXEL_SIZE.xyxy, texcoord.xyxy);
        float e1 = tex2Dlevel0(edgesMapL, coords.xy).g;
        float e2 = tex2Dlevel0(edgesMapL, coords.zw).g;

        // Get the area for this direction:
        areas.ba = Area(abs(d), e1, e2);
    }

    return areas;
}


/**
 *  3 R D   P A S S
 */

float4 NeighborhoodBlendingPS(float2 texcoord : TEXCOORD0,
                              float4 offset[2]: TEXCOORD1) : COLOR0 {
    // Fetch the blending weights for current pixel:
    float4 topLeft = tex2D(blendMap, texcoord);
    float bottom = tex2D(blendMap, offset[1].zw).g;
    float right = tex2D(blendMap, offset[1].xy).a;
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

    // Add the contributions of the 4 possible lines that can cross this pixel:
    #ifdef BILINEAR_FILTER_TRICK
        float4 coords = mad(float4( 0.0, -a.r, 0.0,  a.g), PIXEL_SIZE.yyyy, texcoord.xyxy);
        color = mad(tex2D(colorMapL, coords.xy), w.r, color);
        color = mad(tex2D(colorMapL, coords.zw), w.g, color);

        coords = mad(float4(-a.b,  0.0, a.a,  0.0), PIXEL_SIZE.xxxx, texcoord.xyxy);
        color = mad(tex2D(colorMapL, coords.xy), w.b, color);
        color = mad(tex2D(colorMapL, coords.zw), w.a, color);
    #else
        float4 C = tex2D(colorMap, texcoord);
        float4 Cleft = tex2D(colorMap, offset[0].xy);
        float4 Ctop = tex2D(colorMap, offset[0].zw);
        float4 Cright = tex2D(colorMap, offset[1].xy);
        float4 Cbottom = tex2D(colorMap, offset[1].zw);
        color = mad(lerp(C, Ctop, a.r), w.r, color);
        color = mad(lerp(C, Cbottom, a.g), w.g, color);
        color = mad(lerp(C, Cleft, a.b), w.b, color);
        color = mad(lerp(C, Cright, a.a), w.a, color);
    #endif

    // Normalize the resulting color and we are finished!
    return color / sum; 
}


/**
 * Time for some techniques!
 */

technique ColorEdgeDetection {
    pass ColorEdgeDetection {
        VertexShader = compile vs_3_0 OffsetVS();
        PixelShader = compile ps_3_0 ColorEdgeDetectionPS();
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
        VertexShader = compile vs_3_0 OffsetVS();
        PixelShader = compile ps_3_0 DepthEdgeDetectionPS();
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
        VertexShader = compile vs_3_0 PassThroughVS();
        PixelShader = compile ps_3_0 BlendWeightCalculationPS();
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
        VertexShader = compile vs_3_0 OffsetVS();
        PixelShader = compile ps_3_0 NeighborhoodBlendingPS();
        ZEnable = false;
        SRGBWriteEnable = true;
        AlphaBlendEnable = false;

        // Here we want to process only marked pixels.
        StencilEnable = true;
        StencilPass = KEEP;
        StencilFunc = EQUAL;
        StencilRef = 1;
    }
}
