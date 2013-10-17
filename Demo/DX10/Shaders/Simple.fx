/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
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

// For reprojection:
matrix currWorldViewProj;
matrix prevWorldViewProj;


// For shading:
bool shading;
matrix world;
float3 eyePositionW;

Texture2D diffuseTex;
Texture2D normalTex;
TextureCube envTex;


// Samplers:
SamplerState LinearSampler {
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState AnisotropicSampler {
    Filter = ANISOTROPIC;
    AddressU = Wrap;
    AddressV = Wrap;
    MaxAnisotropy = 16;
};


struct SimpleV2P {
    // For reprojection:
    float4 svPosition : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 currPosition : TEXCOORD1;
    float3 prevPosition : TEXCOORD2;

    // For shading:
    centroid float3 normalW : TEXCOORD3;
    centroid float3 tangentW : TEXCOORD4;
    centroid float3 positionW : TEXCOORD5;
};


SimpleV2P SimpleVS(float4 position : POSITION,
                   float3 normal : NORMAL,
                   float2 texcoord : TEXCOORD,
                   float3 tangent : TANGENT) {
    SimpleV2P output;

    // Transform to homogeneous projection space:
    output.svPosition = mul(position, currWorldViewProj);
    output.currPosition = output.svPosition.xyw;
    output.prevPosition = mul(position, prevWorldViewProj).xyw;

    // Positions in projection space are in [-1, 1] range, while texture
    // coordinates are in [0, 1] range. So, we divide by 2 to get velocities in
    // the scale (and flip the y axis):
    output.currPosition.xy *= float2(0.5, -0.5);
    output.prevPosition.xy *= float2(0.5, -0.5);

    // Output texture coordinates:
    output.texcoord = texcoord;

    // Build the vectors required for shading:
    output.normalW = mul(normal, (float3x3) world);
    output.tangentW = mul(tangent, (float3x3) world);
    output.positionW = mul(position, world).xyz;

    return output;
}


float3 Shade(SimpleV2P input) {
    // Normalize the input:
    input.normalW = normalize(input.normalW);
    input.tangentW = normalize(input.tangentW);

    // Calculate eye and light vectors:
    float3 eyeW = normalize(eyePositionW - input.positionW);
    float3 lightW = float3(0.0, 1.0, -1.0);

    // Fetch and unpack the normal:
    float3 normalT = normalTex.Sample(AnisotropicSampler, input.texcoord).rgb;
    normalT.xy = -1.0 + 2.0 * normalT.gr;
    normalT.z = sqrt(1.0 - normalT.x * normalT.x - normalT.y * normalT.y);
    normalT = normalize(normalT);

    // Transform the normal to world space:
    float3 bitangentW = cross(input.normalW, input.tangentW);
    float3x3 tbn = transpose(float3x3(input.tangentW, bitangentW, input.normalW));
    float3 normalW = mul(tbn, normalT);

    // Fetch the albedo color:
    float4 albedo = diffuseTex.Sample(AnisotropicSampler, input.texcoord);

    // Calculate environment reflections:
    // (we'll try to do our best with a single specular map)
    float3 reflectionW = normalize(reflect(-eyeW, normalW));
    float intensity = 2.0 * albedo.a * albedo.a;
    float enviroment = intensity * dot(envTex.Sample(LinearSampler, reflectionW).rgb, 1.0 / 3.0);

    // Set the ambient color:
    float ambient = 0.025;

    // Calculate the diffuse component:
    float diffuse = max(dot(normalW, lightW), 0.0);

    // Accumulate them:
    float3 color = (albedo.rgb + enviroment) * (ambient + diffuse);

    // Calculate and accumulate the specular component:
    reflectionW = normalize(reflect(-lightW, normalW));
    color += 7.0 * albedo.a * pow(max(dot(reflectionW, eyeW), 0.0), 45.0);

    // Return the shaded pixel:
    return color;
}


float4 SimplePS(SimpleV2P input,
                out float2 velocity : SV_TARGET1) : SV_TARGET0 {
    // Convert to non-homogeneous points by dividing by w:
    input.currPosition.xy /= input.currPosition.z; // w is stored in z
    input.prevPosition.xy /= input.prevPosition.z;

    // Calculate velocity in non-homogeneous projection space:
    velocity = input.currPosition.xy - input.prevPosition.xy;

    // Shade the pixel:
    float3 color = shading? Shade(input) : 0.5;

    return float4(color, 1.0);
}


DepthStencilState EnableDepthDisableStencil {
    DepthEnable = TRUE;
    StencilEnable = FALSE;
};

BlendState NoBlending {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
};


technique10 Simple {
    pass Simple {
        SetVertexShader(CompileShader(vs_4_0, SimpleVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, SimplePS()));

        SetDepthStencilState(EnableDepthDisableStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
