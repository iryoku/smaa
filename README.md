SMAA: Subpixel Morphological Antialiasing
=========================================

SMAA is a very efficient GPU-based MLAA implementation (DX9, DX10, DX11 and OpenGL), capable of handling subpixel features seamlessly, and featuring an improved and advanced pattern detection & handling mechanism.

The technique focuses on handling each pattern in a very specific way (via look-up-tables), in order to minimize false positives in the pattern detection. **Ultimately, this prevents antialiasing features that are not produced by jaggies, like texture details**. Furthermore, this conservative morphological approach, together with correct subsample area estimation, allows to accurately combine MLAA with multi/supersampling techniques. Finally, the technique has been specifically designed to clone (to a reasonable extent) multisampling reference results.

This code is licensed under the MIT license, with a clarification to avoid copyright notices on binary releases (see [below](#copyright-and-license)).

Checkout the [paper](http://www.iryoku.com/smaa/) for more info!


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

See [SMAA.hlsl](https://github.com/iryoku/smaa/blob/master/SMAA.hlsl) for integration info (despite the extension, note that it's OpenGL compatible).

You'll also need some precomputed textures, which can be found as C++ headers ([Textures/AreaTex.h](https://github.com/iryoku/smaa/blob/master/Textures/AreaTex.h) and [Textures/SearchTex.h](https://github.com/iryoku/smaa/blob/master/Textures/SearchTex.h)), or as regular DDS files (see [Textures](https://github.com/iryoku/smaa/blob/master/Textures) directory). If you want to see where they came from, you can check out the [Scripts](https://github.com/iryoku/smaa/blob/master/Scripts) directory.

The directories [DX9](https://github.com/iryoku/smaa/blob/master/Demo/DX9) and [DX10](https://github.com/iryoku/smaa/blob/master/Demo/DX10) contain integration examples for DirectX 9 and 10 respectively.


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

Copyright &copy; 2013 Jorge Jimenez (jorge@iryoku.com)

Copyright &copy; 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)

Copyright &copy; 2013 Belen Masia (bmasia@unizar.es)

Copyright &copy; 2013 Fernando Navarro (fernandn@microsoft.com)

Copyright &copy; 2013 Diego Gutierrez (diegog@unizar.es)

Permission is hereby granted, free of charge, to any person obtaining a copy
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software. As clarification, there is no
requirement that the copyright notice and permission be included in binary
distributions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
