/**
 * This XNA4/C# port of SMAA is (c) 2012, Alexander Christoph Gessler
 * It is released as Open Source under the same conditions as SMAA itself.
 * 
 * Check out LICENSE.txt in the root folder of the repository or
 * Readme.txt in /Demo/XNA for more information.
*/

using Microsoft.Xna.Framework.Graphics;

namespace SMAADemo
{
    public interface ITextureProvider
    {
        Texture2D Get(string name);
        void Prefetch(string name);
    }
}

