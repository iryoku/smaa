README
======

Hi there! Thanks for downloading our demo!

In the following text you will learn what is included in the demo, and how to 
load custom batches of images without having to use the GUI. Also how we
recommend you to explore the different options of the demo.

This demo includes example images allowing its instant use. If you want to play
with your own images, you just need to add images to a folder named "images",
which should be located in the same folder as the executable. Then, the
application will load automatically all the *png* images in the folder. You can
load other image formats using the button found on the GUI. The recommended
resolution for the images is 1280x720.

By default, the demo uses color for the edge detection pass. However, when a
depth map is provided, it is automatically loaded and used for detecting edges.
The depth map must have the same name as the image but with dds extension (i.e.
image.png and image.dds).

Once inside the app, you can:

- Load a new image. The rules are the same as for automatic loading: [image 
  name].png is the image to be anti-aliased, and [image name].dds its 
  corresponding depth map (optional). Supported image formats for the color
  image are bmp, jpg and png.

- Change between edge detection modes. Depth mode is only available if a depth
  map has been provided.

- Display the color image, the edges detected or the blending weights for the
  anti-aliasing pass.

- Change between the images loaded. Alternatively, A and D keys can be used.

- Switch anti-aliasing on/off in order to compare the results with the
  original input.

- Check the performance of the algorithm for the current image activating the
  Profile checkbox.

- Change the threshold used for the edge detection with the corresponding
  slider (it is interesting to see its effects while 'View edges' is
  selected).

- Change the 'Max search steps'. A comprehensive low-level description of this
  parameter can be found in the article. In practice, it makes large quasi-
  horizontal and quasi-vertical aliased edges an lines look smoother.

All the changes done to the loaded images will not modify the original source
files.

Enjoy!
