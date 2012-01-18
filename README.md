SMAA: Subpixel Morphological Antialiasing
=========================================

SMAA is a very efficient GPU-based MLAA implementation, capable of handling subpixel features seamlessly, and featuring an advanced pattern detection & handling mechanism.

Checkout the [technical paper](http://www.iryoku.com/smaa/) for more info!


Thanks To
---------

**Stephen Hill** ‒ for its invaluable support.

**Alex Fry** ‒ for its priceless help with the devkit.

**Naty Hoffman** ‒ for helping us to touch base with the game developer community.

**Jean-Francois St-Amour** ‒ for providing us great images for testing.

**Johan Andersson** ‒ for providing the fantastic BF3 image and clearing important questions.

**Andrej Dudenhenfer** ‒ for creating the SMAA injector.

**Dmitriy Jdone** ‒ for porting the code to GLSL.

**Weibo Xie** ‒ for the suggested optimizations.

**Alexander Reshetov** ‒ for creating MLAA, and opening our mind.

**Everyone on the [SIGGRAPH course](http://iryoku.com/aacourse/)** ‒ for the incredible inspiration.


Usage
-----

See [SMAA.h](https://github.com/iryoku/smaa/blob/master/SMAA.h) for integration info. You'll also need some precomputed textures, which can be found as C++ headers ([Textures/AreaTex.h](https://github.com/iryoku/smaa/blob/master/Textures/AreaTex.h) and [Textures/SearchTex.h](https://github.com/iryoku/smaa/blob/master/Textures/SearchTex.h)), or as regular DDS files (see [Textures](https://github.com/iryoku/smaa/blob/master/Textures) directory). The directories [DX9](https://github.com/iryoku/smaa/blob/master/Demo/DX9) and [DX10](https://github.com/iryoku/smaa/blob/master/Demo/DX10) contain integration examples for DirectX 9 and 10 respectively.


Bug Tracker
-----------

Found a bug? Please create an issue here on GitHub!

https://github.com/iryoku/smaa/issues


Authors
-------

**Jorge Jimenez** http://www.iryoku.com/

**Jose I. Echevarria** http://cheveone.blogspot.com/

**Tiago Sousa** https://twitter.com/#!/CRYTEK_TIAGO

**Belen Masia**

**Fernando Navarro**

**Diego Gutierrez** http://giga.cps.unizar.es/~diegog/


Copyright and License
---------------------

Copyright &copy; 2011 Jorge Jimenez (jorge@iryoku.com)

Copyright &copy; 2011 Belen Masia (bmasia@unizar.es) 

Copyright &copy; 2011 Jose I. Echevarria (joseignacioechevarria@gmail.com) 

Copyright &copy; 2011 Fernando Navarro (fernandn@microsoft.com) 

Copyright &copy; 2011 Diego Gutierrez (diegog@unizar.es)

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
