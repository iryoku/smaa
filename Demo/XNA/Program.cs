/**
 * This XNA4/C# port of SMAA is (c) 2012, Alexander Christoph Gessler
 * It is released as Open Source under the same conditions as SMAA itself.
 * 
 * Check out LICENSE.txt in the root folder of the repository or
 * Readme.txt in /Demo/XNA for more information.
*/

using System.Windows.Forms;

#if XBOX
#error demo app is windows only (smaa of course is not)
#endif

namespace SMAADemo
{
#if WINDOWS || XBOX
    static class Program
    {
        static void Main()
        {
            MessageBox.Show("Welcome to the SMAA demo app. The image is pre-rendered and comes " +
                "from the original SMAA demo app (it is a screenshot from a Unigine demo). "  +
                "Using a pre-rendered image is possible because SMAA is fully image-based " +
                "and takes no inputs except the rendered scene." +
                "\n\nPress [Space] to cycle between different SMAA modes, the current active " +
                "mode is shown in the window title");

            using (var game = new Demo())
            {
                game.Run();
            }
        }
    }
#endif
}

