#!python3
#
# This texture allows to know how many pixels we must advance in the last step
# of our line search algorithm, with a single fetch.
#
# Requires:
#   - Python 3.3.2: http://www.python.org/
#   - Pillow 2.1.0: https://pypi.python.org/pypi/Pillow/2.1.0#downloads

from PIL import Image

# Interpolates between two values:
def lerp(v0, v1, p):
    return v0 + (v1 - v0) * p

# Calculates the bilinear fetch for a certain edge combination:
def bilinear(e):
    # e[0]       e[1]
    #
    #          x <-------- Sample position:    (-0.25,-0.125)
    # e[2]       e[3] <--- Current pixel [3]:  (  0.0, 0.0  )
    a = lerp(e[0], e[1], 1.0 - 0.25)
    b = lerp(e[2], e[3], 1.0 - 0.25)
    return lerp(a, b, 1.0 - 0.125)

# This dict returns which edges are active for a certain bilinear fetch:
# (it's the reverse lookup of the bilinear function)
edge = {
    bilinear([0, 0, 0, 0]): [0, 0, 0, 0],
    bilinear([0, 0, 0, 1]): [0, 0, 0, 1],
    bilinear([0, 0, 1, 0]): [0, 0, 1, 0],
    bilinear([0, 0, 1, 1]): [0, 0, 1, 1],

    bilinear([0, 1, 0, 0]): [0, 1, 0, 0],
    bilinear([0, 1, 0, 1]): [0, 1, 0, 1],
    bilinear([0, 1, 1, 0]): [0, 1, 1, 0],
    bilinear([0, 1, 1, 1]): [0, 1, 1, 1],

    bilinear([1, 0, 0, 0]): [1, 0, 0, 0],
    bilinear([1, 0, 0, 1]): [1, 0, 0, 1],
    bilinear([1, 0, 1, 0]): [1, 0, 1, 0],
    bilinear([1, 0, 1, 1]): [1, 0, 1, 1],

    bilinear([1, 1, 0, 0]): [1, 1, 0, 0],
    bilinear([1, 1, 0, 1]): [1, 1, 0, 1],
    bilinear([1, 1, 1, 0]): [1, 1, 1, 0],
    bilinear([1, 1, 1, 1]): [1, 1, 1, 1],
}

# Delta distance to add in the last step of searches to the left:
def deltaLeft(left, top):
    d = 0

    # If there is an edge, continue:
    if top[3] == 1:
        d += 1

    # If we previously found an edge, there is another edge and no crossing
    # edges, continue:
    if d == 1 and top[2] == 1 and left[1] != 1 and left[3] != 1:
        d += 1

    return d

# Delta distance to add in the last step of searches to the right:
def deltaRight(left, top):
    d = 0

    # If there is an edge, and no crossing edges, continue:
    if top[3] == 1 and left[1] != 1 and left[3] != 1:
        d += 1

    # If we previously found an edge, there is another edge and no crossing
    # edges, continue:
    if d == 1 and top[2] == 1 and left[0] != 1 and left[2] != 1:
        d += 1

    return d

# Prints the edges in a readable form:
def debug(dir, texcoord, val, left, top):
    print(dir, texcoord, val)
    print("|%s %s| |%s %s|" % (left[0], left[1], top[0], top[1]))
    print("|%s %s| |%s %s|" % (left[2], left[3], top[2], top[3]))
    print()

# Prints C++ code encoding a texture:
def cpp(image):
    n = 0
    print("static const unsigned char searchTexBytes[] = {")
    print("   ", end=" ")
    for y in range(image.size[1]):
        for x in range(image.size[0]):
            val = image.getpixel((x, y))[0]
            if n < 66 * 33 - 1: print("0x%02x," % val, end=" ")
            else: print("0x%02x" % val, end=" ")
            n += 1
            if n % 12 == 0: print("\n   ", end=" ")
    print()
    print("};")

# Calculate delta distances to the left:
image = Image.new("RGB", (66, 33))
for x in range(33):
    for y in range(33):
        texcoord = 0.03125 * x, 0.03125 * y
        if texcoord[0] in edge and texcoord[1] in edge:
            edges = edge[texcoord[0]], edge[texcoord[1]]
            val = 127 * deltaLeft(*edges) # Maximize dynamic range to help compression
            image.putpixel((x, y), (val, val, val))
            #debug("left: ", texcoord, val, *edges)

# Calculate delta distances to the right:
for x in range(33):
    for y in range(33):
        texcoord = 0.03125 * x, 0.03125 * y
        if texcoord[0] in edge and texcoord[1] in edge:
            edges = edge[texcoord[0]], edge[texcoord[1]]
            val = 127 * deltaRight(*edges) # Maximize dynamic range to help compression
            image.putpixel((33 + x, y), (val, val, val))
            #debug("right: ", texcoord, val, *edges)

# Crop it to power-of-two to make it BC4-friendly:
# (Cropped area and borders are black)
image = image.crop([0, 17, 64, 33])
image = image.transpose(Image.FLIP_TOP_BOTTOM)

# Save the texture:
image.save("SearchTex.tga")

# And print the C++ code:
cpp(image)
