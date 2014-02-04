/**
 * This XNA4/C# port of SMAA is (c) 2012, Alexander Christoph Gessler
 * It is released as Open Source under the same conditions as SMAA itself.
 * 
 * Check out LICENSE.txt in the root folder of the repository or
 * Readme.txt in /Demo/XNA for more information.
*/

using System;
using System.Globalization;

namespace SMAADemo
{
    /** Default Logger implementation, prints to console */
    public class DefaultLogger : ILogger
    {
        public void Log(string text, int level = (int)LogLevel.INFO)
        {
            Console.WriteLine(level.ToString(CultureInfo.InvariantCulture) + ": " + text);
        }
    }
}
