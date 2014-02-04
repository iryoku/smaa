
// easiest way to generate different permutations using XNA content pipeline
#define SMAA_PRESET_HIGH 1

// these are needed to bring the SMAA shader through the XNA FX compiler
#define XNA_HAVE_NO_SRGBWRITEENABLE
#define XNA_HAVE_NO_ALPHATESTENABLE
#define XNA_HAVE_NO_SRGBTEXTURE
#define XNA_NO_STENCIL

// this is only needed if the input texture does not support linear
// filtering (i.e. floating-point format)
#define SMAA_DIRECTX9_LINEAR_BLEND 1

#include "SMAA.fxh"
