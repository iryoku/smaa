XNA 4.0 SMAA Port
==========================================

1. How to use it
2. Xbox 360 Performance
2. Known Issues
2. License



1. How to use it
==========================

Best thing to do is to checkout the demo app (SMAADemo.sln). SMAA.cs
contains the main SMAA code along with some documentation.

Since XNA requires all effects to be precompiled, there are separate
shaders for all SMAA quality stages in the shaders/ folder. To be 
able to switch quality at runtime, all shaders need to be added
to the content pipeline. Make sure to set the "Optimize" flag in
the settings of the effects to avoid breaking the 512 instruction 
limit for SM 3.0 shaders.

Additionally, two lookup textures are needed by SMAA:

- textures/AreaTexDX9.dds
- textures/SearchTex.dds

Both need to be configured as follows: no Mips, no Color Keying,
no Premultiplied Alpha, no DXT Compression.

That's it.


2. XBox 360 Performance
==========================

Since SMAA expects the scene as input, a full resolve pass from the
EDRAM to main memory is needed. This is typically a main bottleneck
in XBox 360 renderers, so better expect XBox 360 performance 
degradation to be worse than on PC (it is definitely the case
in my measurements but your mileage may, of course, vary).


3. Known Issues
==========================

- SMAA uses the stencil buffer to mask pixels that need no 
  anti-aliasing, which greatly improves performance (since typically
  most pixels aren't edge pixels). Unfortunately, in XNA stencil
  buffers are tightly bound to render targets, rendering this
  optimization very difficult (the reference implementation first
  renders into a render target, populates a stencil buffer. Then
  it sets this render target as texture for the second pass but
  continues to use the stencil buffer -- sigh)

  For this reason, the stencil buffer optimization is disabled
  in the current XNA version of the shader.

  The only way out seems to be to render the first pass into two 
  render targets, the second of of which has a stencil buffer 
  attached. I didn't test this and I suspect it will be slower
  than not using stencil at all (2 render targets have twice
  the resolve bandwidth cost).


- XNA is based on a sub DX9 feature level in which linear filtering
  on floating point textures is not supported, even if the hardware
  is capable of doit it (which is the case for most hardware in
  use today). For this reason, some of the input textures filters
  have been changed from LINEAR to POINT to support SMAA on 
  floating-point targets (such as from HDR rendering). Probably
  this should be made a preprocessor define instead since LINEAR
  would be fine for non-floating-point targets.

- This code has been used in multiple XBox Live Indie Games
  on the XBox 360, but is far away from being super well-tested. 
  Use at your own risk, see the license section for more information.


4. License
==========================


The C# port of SMAA is (c) 2012, Alexander Christoph Gessler
It is released as Open Source under the same conditions as SMAA itself.


SMAA is subject to the following copyright notice / license:


Copyright (C) 2011 Jorge Jimenez (jorge@iryoku.com)
Copyright (C) 2011 Belen Masia (bmasia@unizar.es)
Copyright (C) 2011 Jose I. Echevarria (joseignacioechevarria@gmail.com)
Copyright (C) 2011 Fernando Navarro (fernandn@microsoft.com)
Copyright (C) 2011 Diego Gutierrez (diegog@unizar.es)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the following disclaimer
      in the documentation and/or other materials provided with the
      distribution:

      "Uses SMAA. Copyright (C) 2011 by Jorge Jimenez, Jose I. Echevarria,
       Belen Masia, Fernando Navarro and Diego Gutierrez."

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are
those of the authors and should not be interpreted as representing official
policies, either expressed or implied, of the copyright holders.