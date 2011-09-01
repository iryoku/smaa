README
======

In the following text you will learn what is included in the DX10 demo, and how to load custom batches of images without having to use the GUI. Also how we recommend you to explore the different options of the demo.

This demo includes example images allowing its instant use. If you want to play with your own images, you just need to add images to a folder named *Images*, which should be located in the same folder as the executable. Then, the application will load automatically all the *png* images in the folder. Alternatively You can load other image formats using the button found on the GUI.

Once inside the app, you can:

- Load a new image. Supported image formats for the color image are *bmp*, *jpg* and *png*. The program will try to load a depth buffer as follows: if *[image name].png* is the image loaded, and there exists a file named *[image name].dds*, it will be used as depth map (this is optional).

- Change between the images loaded. Alternatively, *A* and *D* keys can be used.

- Display the color image, the edges detected or the blending weights for the anti-aliasing pass.

- Change between edge detection modes. Depth mode is only available if a depth map has been provided.

- Switch anti-aliasing on/off in order to compare the results with the original input.

- Check the performance of the algorithm for the current image activating the Profile checkbox.

If a custom preset is selected, you can:

- Change the threshold used for the edge detection with the corresponding slider (it is interesting to see its effects while 'View edges' is selected).

- Change the maximum number of steps of the horizontal/vertical pattern searches. Longer lines will look better with higher number of steps.

- Change the maximum number of steps of the diagonal pattern searches. Longer diagonal lines will look better with higher number of steps.

All the changes done to the loaded images will not modify the original source files.

Enjoy!
