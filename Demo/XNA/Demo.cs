/**
 * This XNA4/C# port of SMAA is (c) 2012, Alexander Christoph Gessler
 * It is released as Open Source under the same conditions as SMAA itself.
 * 
 * Check out LICENSE.txt in the root folder of the repository or
 * Readme.txt in /Demo/XNA for more information.
*/

using System.Diagnostics;

using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;

namespace SMAADemo
{
   
    public class Demo : Game
    {
        private GraphicsDeviceManager gd;
        SpriteBatch spriteBatch;
        SMAA smaa;
        Texture2D texture;
        RenderTarget2D rt;

        int mode;
        const int MAX_MODES = 4;
        bool wasDown;

        private const string baseTitle = "Subpixel Morphological Antialiasing (SMAA) XNA Demo - [Space] to cycle modes - ";

        public Demo()
        {
            gd = new GraphicsDeviceManager(this)
                {PreferredBackBufferWidth = 1280, PreferredBackBufferHeight = 720};

            Content.RootDirectory = "";

            Window.Title = baseTitle + "No SMAA";
        }


        protected override void LoadContent()
        {
            spriteBatch = new SpriteBatch(GraphicsDevice);

            smaa = new SMAA(GraphicsDevice, 1280, 720, SMAA.Preset.ULTRA,
                Content, null, null, "shaders/SMAA_", "textures/");

            texture = Content.Load<Texture2D>("textures/Unigine02");

            rt = new RenderTarget2D(GraphicsDevice, 1280, 720);
        }

      
        protected override void UnloadContent()
        {
            smaa.Dispose();
        }

       
        protected override void Update(GameTime gameTime)
        {
            // Allows the game to exit
            if (GamePad.GetState(PlayerIndex.One).Buttons.Back == ButtonState.Pressed)
                Exit();

            KeyboardState ks = Keyboard.GetState();
            if (ks.IsKeyUp(Keys.Space))
            {
                if (wasDown)
                {
                    mode = (mode + 1) % MAX_MODES;
                    wasDown = false;

                    switch (mode)
                    {
                        case 0:
                            Window.Title = baseTitle + "No SMAA";
                            break;

                        case 1:
                            Window.Title = baseTitle + "Quality Ultra, Edge Detection";
                            break;

                        case 2:
                            Window.Title = baseTitle + "Quality Ultra, Luma Detection";
                            break;

                        case 3:
                            Window.Title = baseTitle + "Quality Ultra, Color Detection";
                            break;

                        default:
                            Debug.Assert(false);
                            return;
                    }
                }
            }
            else
            {
                wasDown = true;
            }

            base.Update(gameTime);
        }

      
        protected override void Draw(GameTime gameTime)
        {
            GraphicsDevice.Clear(Color.CornflowerBlue);

            if (mode > 0)
            {
                GraphicsDevice.SetRenderTarget(rt);
            }

            spriteBatch.Begin();
            spriteBatch.Draw(texture, new Rectangle(0, 0, 1280, 720), Color.White);
            spriteBatch.End();

            if (mode > 0)
            {
                SMAA.Input sinput; 
                switch (mode)
                {
                    case 1:
                        sinput = SMAA.Input.DEPTH;
                        break;

                    case 2:
                        sinput = SMAA.Input.LUMA;
                        break;

                    case 3:
                        sinput = SMAA.Input.COLOR;
                        break;

                    default:
                        Debug.Assert(false);
                        return;
                }

                smaa.Go(rt, rt, null, sinput);
            }

            base.Draw(gameTime);
        }
    }
}
