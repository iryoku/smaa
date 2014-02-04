/**
 * This XNA4/C# port of SMAA is (c) 2012, Alexander Christoph Gessler
 * It is released as Open Source under the same conditions as SMAA itself.
 * 
 * Check out LICENSE.txt in the root folder of the repository or
 * Readme.txt in /Demo/XNA for more information.
*/

namespace SMAADemo
{
    public enum LogLevel
    {
        VERBOSE = 0, INFO = 5, WARN = 10, ERROR = 15
    }

    public interface ILogger
    {
        void Log(string text, int level = (int)LogLevel.INFO);
    }
}
