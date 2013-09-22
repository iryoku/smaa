#!python3
#
# This texture allows to obtain the area for a certain pattern and distances
# to the left and to right of the line.
#
# Requires:
#   - Python 3.3.2: http://www.python.org/
#   - Pillow 2.1.0: https://pypi.python.org/pypi/Pillow/2.1.0#downloads

from PIL import Image
from multiprocessing import *
from math import *
from tempfile import *
import operator

# Subsample offsets for orthogonal and diagonal areas:
SUBSAMPLE_OFFSETS_ORTHO = [ 0.0,   #0
                           -0.25,  #1
                            0.25,  #2
                           -0.125, #3
                            0.125, #4
                           -0.375, #5
                            0.375] #6
SUBSAMPLE_OFFSETS_DIAG  = [( 0.00,   0.00),  #0
                           ( 0.25,  -0.25),  #1
                           (-0.25,   0.25),  #2
                           ( 0.125, -0.125), #3
                           (-0.125,  0.125)] #4

# Texture sizes:
# (it's quite possible that this is not easily configurable)
SIZE_ORTHO = 16 # * 5 slots = 80
SIZE_DIAG  = 20 # * 4 slots = 80

# Number of samples for calculating areas in the diagonal textures:
# (diagonal areas are calculated using brute force sampling)
SAMPLES_DIAG = 30

# Maximum distance for smoothing u-shapes:
SMOOTH_MAX_DISTANCE = 32

#------------------------------------------------------------------------------
# Misc Functions

# Pixel layout for DirectX 9:
def la(v):
    return v[0], v[0], v[0], v[1]

# Pixel layout for DirectX 10:
def rgb(v):
    return v[0], v[1], 0, 0

# Coverts to 0..255 range:
def bytes(v):
    return tuple([int(255.0 * a) for a in v])

# Prints C++ code encoding a texture:
def cpp(image):
    n = 0
    last = 2 * (image.size[0] * image.size[1]) - 1

    print("static const unsigned char areaTexBytes[] = {")
    print("   ", end=" ")
    for y in range(image.size[1]):
        for x in range(image.size[0]):
            val = image.getpixel((x, y))

            if n < last: print("0x%02x," % val[0], end=" ")
            else: print("0x%02x" % val[0], end=" ")
            n += 1

            if n < last: print("0x%02x," % val[1], end=" ")
            else: print("0x%02x" % val[1], end=" ")
            n += 1

            if n % 12 == 0: print("\n   ", end=" ")
    print()
    print("};")

# A vector of two numbers:
class vec2(tuple):
    def __new__(self, v1, v2):
        return tuple.__new__(self, [v1, v2])
    def __add__(self, other):
        t1, t2 = map(operator.add, self, other)
        return self.__class__(t1, t2)
    def __sub__(self, other):
        t1, t2 = map(operator.sub, self, other)
        return self.__class__(t1, t2)
    def __mul__(self, other):
        t1, t2 = map(operator.mul, self, other) if isinstance(other, vec2) else (other * self[0], other * self[1])
        return self.__class__(t1, t2)
    def __truediv__(self, other):
        return self.__class__(self[0] / other, self[1] / other)
    def __ne__(self, other):
        return any([v1 != v2 for v1, v2 in zip(self, other)])
    def sqrt(self):
        return self.__class__(sqrt(self[0]), sqrt(self[1]))

# Linear interpolation:
def lerp(a, b, p):
    return a + (b - a) * p

# Saturates a value to [0..1] range:
def saturate(a):
    return min(max(a, 0.0), 1.0)

# Smoothing function for small u-patterns:
def smootharea(d, a1, a2):
    b1 = (a1 * 2.0).sqrt() * 0.5
    b2 = (a2 * 2.0).sqrt() * 0.5
    p = saturate(d / float(SMOOTH_MAX_DISTANCE))
    return lerp(b1, a1, p), lerp(b2, a2, p)

#------------------------------------------------------------------------------
# Mapping Functions (for placing each pattern subtexture into its place)

edgesortho = [ (0, 0), (3, 0), (0, 3), (3, 3), (1, 0), (4, 0), (1, 3), (4, 3),
               (0, 1), (3, 1), (0, 4), (3, 4), (1, 1), (4, 1), (1, 4), (4, 4) ]

edgesdiag  = [ (0, 0), (1, 0), (0, 2), (1, 2), (2, 0), (3, 0), (2, 2), (3, 2),
               (0, 1), (1, 1), (0, 3), (1, 3), (2, 1), (3, 1), (2, 3), (3, 3) ]

#------------------------------------------------------------------------------
# Horizontal/Vertical Areas

# Calculates the area for a given pattern and distances to the left and to the
# right, biased by an offset:
def areaortho(pattern, left, right, offset):

    # Calculates the area under the line p1->p2, for the pixel x..x+1:
    def area(p1, p2, x):
        d = p2[0] - p1[0], p2[1] - p1[1]
        x1 = float(x)
        x2 = x + 1.0
        y1 = p1[1] + d[1] * (x1 - p1[0]) / d[0]
        y2 = p1[1] + d[1] * (x2 - p1[0]) / d[0]

        inside = (x1 >= p1[0] and x1 < p2[0]) or (x2 > p1[0] and x2 <= p2[0])
        if inside:
            istrapezoid = (copysign(1.0, y1) == copysign(1.0, y2) or 
                           abs(y1) < 1e-4 or abs(y2) < 1e-4)
            if istrapezoid:
                a = (y1 + y2) / 2.0
                if a < 0.0:
                    return abs(a), 0.0
                else:
                    return 0.0, abs(a)
            else: # Then, we got two triangles:
                x = -p1[1] * d[0] / d[1] + p1[0]
                a1 = y1 *        modf(x)[0]  / 2.0 if x > p1[0] else 0.0
                a2 = y2 * (1.0 - modf(x)[0]) / 2.0 if x < p2[0] else 0.0
                a = a1 if abs(a1) > abs(a2) else -a2
                if a < 0.0:
                    return abs(a1), abs(a2)
                else:
                    return abs(a2), abs(a1)
        else:
            return 0.0, 0.0

    # o1           |
    #      .-------´
    # o2   |
    #
    #      <---d--->
    d = left + right + 1

    o1 = 0.5 + offset
    o2 = 0.5 + offset - 1.0

    if pattern == 0:
        #
        #    ------
        #   
        return 0.0, 0.0

    elif pattern == 1:
        #
        #   .------
        #   |
        #
        # We only offset L patterns in the crossing edge side, to make it
        # converge with the unfiltered pattern 0 (we don't want to filter the
        # pattern 0 to avoid artifacts).
        if left <= right:
            return area(([0.0, o2]), ([d / 2.0, 0.0]), left)
        else:
            return 0.0, 0.0
        
    elif pattern == 2:
        #
        #    ------.
        #          |
        if left >= right:
            return area(([d / 2.0, 0.0]), ([d, o2]), left)
        else:
            return 0.0, 0.0
        
    elif pattern == 3:
        #
        #   .------.
        #   |      |
        a1 = vec2(*area(([0.0, o2]), ([d / 2.0, 0.0]), left))
        a2 = vec2(*area(([d / 2.0, 0.0]), ([d, o2]), left))
        a1, a2 = smootharea(d, a1, a2)
        return a1[0] + a2[0], a1[1] + a2[1]

    elif pattern == 4:
        #   |
        #   `------
        #          
        if left <= right:
            return area(([0.0, o1]), ([d / 2.0, 0.0]), left)
        else:
            return 0.0, 0.0
    
    elif pattern == 5:
        #   |
        #   +------
        #   |      
        return 0.0, 0.0

    elif pattern == 6:
        #   |
        #   `------.
        #          |
        #
        # A problem of not offseting L patterns (see above), is that for certain
        # max search distances, the pixels in the center of a Z pattern will
        # detect the full Z pattern, while the pixels in the sides will detect a
        # L pattern. To avoid discontinuities, we blend the full offsetted Z
        # revectorization with partially offsetted L patterns.
        if abs(offset) > 0.0:
            a1 =  vec2(*area(([0.0, o1]), ([d, o2]), left))
            a2 =  vec2(*area(([0.0, o1]), ([d / 2.0, 0.0]), left))
            a2 += vec2(*area(([d / 2.0, 0.0]), ([d, o2]), left))
            return (a1 + a2) / 2.0
        else:
            return area(([0.0, o1]), ([d, o2]), left)

    elif pattern == 7:
        #   |
        #   +------.
        #   |      |
        return area(([0.0, o1]), ([d, o2]), left)

    elif pattern == 8:
        #          |
        #    ------´
        #   
        if left >= right:
            return area(([d / 2.0, 0.0]), ([d, o1]), left)
        else:
            return 0.0, 0.0

    elif pattern == 9:
        #          |
        #   .------´
        #   |
        if abs(offset) > 0.0:
            a1 = vec2(*area(([0.0, o2]), ([d, o1]), left))
            a2 = vec2(*area(([0.0, o2]), ([d / 2.0, 0.0]), left))
            a2 += vec2(*area(([d / 2.0, 0.0]), ([d, o1]), left))
            return (a1 + a2) / 2.0
        else:
            return area(([0.0, o2]), ([d, o1]), left)

    elif pattern == 10:
        #          |
        #    ------+
        #          |
        return 0.0, 0.0

    elif pattern == 11:
        #          |
        #   .------+
        #   |      |
        return area(([0.0, o2]), ([d, o1]), left)

    elif pattern == 12:
        #   |      |
        #   `------´
        #   
        a1 = vec2(*area(([0.0, o1]), ([d / 2.0, 0.0]), left))
        a2 = vec2(*area(([d / 2.0, 0.0]), ([d, o1]), left))
        a1, a2 = smootharea(d, a1, a2)
        return a1[0] + a2[0], a1[1] + a2[1]

    elif pattern == 13:
        #   |      |
        #   +------´
        #   |
        return area(([0.0, o2]), ([d, o1]), left)

    elif pattern == 14:
        #   |      |
        #   `------+
        #          |
        return area(([0.0, o1]), ([d, o2]), left)

    elif pattern == 15:
        #   |      |
        #   +------+
        #   |      |
        return 0.0, 0.0

#------------------------------------------------------------------------------
# Diagonal Areas

# Calculates the area for a given pattern and distances to the left and to the
# right, biased by an offset:
def areadiag(pattern, left, right, offset):
    # Calculates the area under the line p1->p2 for the pixel 'p' using brute
    # force sampling:
    # (quick and dirty solution, but it works)
    def area1(p1, p2, p):
        def inside(p):
            if p1 != p2:
                x, y = p
                xm, ym = (p1 + p2) / 2.0
                a = p2[1] - p1[1]
                b = p1[0] - p2[0]
                c = a * (x - xm) + b * (y - ym)
                return c > 0
            else:
                return True

        a = 0.0
        for x in range(SAMPLES_DIAG):
            for y in range(SAMPLES_DIAG):
                o = vec2(x, y) / float(SAMPLES_DIAG - 1)
                a += inside(p + o)
        return a / (SAMPLES_DIAG * SAMPLES_DIAG)

    # Calculates the area under the line p1->p2:
    # (includes the pixel and its opposite)
    def area(p1, p2, left, offset):
        e1, e2 = edgesdiag[pattern]
        p1 = p1 + vec2(*offset) if e1 > 0 else p1
        p2 = p2 + vec2(*offset) if e2 > 0 else p2
        a1 = area1(p1, p2, vec2(1.0, 0.0) + vec2(left, left))
        a2 = area1(p1, p2, vec2(1.0, 1.0) + vec2(left, left))
        return vec2(1.0 - a1, a2)

    d = left + right + 1

    # There is some Black Magic around diagonal area calculations. Unlike
    # orthogonal patterns, the 'null' pattern (one without crossing edges) must be
    # filtered, and the ends of both the 'null' and L patterns are not known: L
    # and U patterns have different endings, and we don't know what is the 
    # adjacent pattern. So, what we do is calculate a blend of both possibilites.
    #
    #         .-´
    #       .-´
    #     .-´
    #   .-´
    #   ´
    #
    if pattern == 0:
        a1 = area(vec2(1.0, 1.0), vec2(1.0, 1.0) + vec2(d, d), left, offset) # 1st possibility
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset) # 2nd possibility
        return (a1 + a2) / 2.0 # Blend them

    #
    #         .-´
    #       .-´
    #     .-´
    #   .-´
    #   |
    #   |
    elif pattern == 1:
        a1 = area(vec2(1.0, 0.0), vec2(0.0, 0.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0
        
    #
    #         .----
    #       .-´
    #     .-´
    #   .-´
    #   ´
    #
    elif pattern == 2:
        a1 = area(vec2(0.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #
    #         .----
    #       .-´
    #     .-´
    #   .-´
    #   |
    #   |
    elif pattern == 3:
        return area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)

    #
    #         .-´
    #       .-´
    #     .-´
    # ----´
    #
    #
    elif pattern == 4:
        a1 = area(vec2(1.0, 1.0), vec2(0.0, 0.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 1.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #
    #         .-´
    #       .-´
    #     .-´
    # --.-´
    #   |
    #   |
    elif pattern == 5:
        a1 = area(vec2(1.0, 1.0), vec2(0.0, 0.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #
    #         .----
    #       .-´
    #     .-´
    # ----´
    #
    #
    elif pattern == 6:
        return area(vec2(1.0, 1.0), vec2(1.0, 0.0) + vec2(d, d), left, offset) 

    #
    #         .----
    #       .-´
    #     .-´
    # --.-´
    #   |
    #   |
    elif pattern == 7:
        a1 = area(vec2(1.0, 1.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #         |
    #         |
    #       .-´
    #     .-´
    #   .-´
    #   ´
    #
    elif pattern == 8:
        a1 = area(vec2(0.0, 0.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #         |
    #         |
    #       .-´
    #     .-´
    #   .-´
    #   |
    #   |
    elif pattern == 9:
        return area(vec2(1.0, 0.0), vec2(1.0, 1.0) + vec2(d, d), left, offset) 

    #         |
    #         .----
    #       .-´
    #     .-´
    #   .-´
    #   ´
    #
    elif pattern == 10:
        a1 = area(vec2(0.0, 0.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #         |
    #         .----
    #       .-´
    #     .-´
    #   .-´
    #   |
    #   |
    elif pattern == 11:
        a1 = area(vec2(1.0, 0.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #         |
    #         |
    #       .-´
    #     .-´
    # ----´
    #
    #   
    elif pattern == 12:
        return area(vec2(1.0, 1.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)

    #         |
    #         |
    #       .-´
    #     .-´
    # --.-´
    #   |
    #   |
    elif pattern == 13:
        a1 = area(vec2(1.0, 1.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #         |
    #         .----
    #       .-´
    #     .-´
    # ----´
    #
    #
    elif pattern == 14:
        a1 = area(vec2(1.0, 1.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 1.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

    #         |
    #         .----
    #       .-´
    #     .-´
    # --.-´
    #   |
    #   |
    elif pattern == 15:
        a1 = area(vec2(1.0, 1.0), vec2(1.0, 1.0) + vec2(d, d), left, offset)
        a2 = area(vec2(1.0, 0.0), vec2(1.0, 0.0) + vec2(d, d), left, offset)
        return (a1 + a2) / 2.0

#------------------------------------------------------------------------------
# Main Functions

# Assembles 2D pattern subtextures into a 4D texture:
def assemble(tex4d, files, edges, pos, size, compress):

    # Open pattern textures created by the workers:
    areas = [Image.open(file.name) for file in files]

    # Puts a pattern subtexture into its position in the 4D texture:
    def putpattern(pattern):
        for left in range(size):
            for right in range(size):
                p = vec2(left, right)
                pp = pos + p + vec2(size, size) * vec2(*edges[pattern])
                tex4d.putpixel(pp, rgb(areas[pattern].getpixel(compress(p))))

    # Put each pattern into its place:
    for i in range(16):
        putpattern(i)

    # Save the texture:
    tex4d.save("AreaTexDX10.tga")

# Creates a 2D orthogonal pattern subtexture:
def tex2dortho(args):
    pattern, path, offset = args
    size = (SIZE_ORTHO - 1)**2 + 1
    tex2d = Image.new("RGBA", (size, size))
    for y in range(size):
        for x in range(size):
            p = areaortho(pattern, x, y, offset)
            p = p[0], p[1], 0.0, 0.0
            tex2d.putpixel((x, y), bytes(p))
    tex2d.save(path, "TGA")

# Creates a 2D diagonal pattern subtexture:
def tex2ddiag(args):
    pattern, path, offset = args
    tex2d = Image.new("RGBA", (SIZE_DIAG, SIZE_DIAG))
    for y in range(SIZE_DIAG):
        for x in range(SIZE_DIAG):
            p = areadiag(pattern, x, y, offset)
            p = p[0], p[1], 0.0, 0.0
            tex2d.putpixel((x, y), bytes(p))
    tex2d.save(path, "TGA")

# Calculate the orthogonal patterns 4D texture for a given offset:
def tex4dortho(tex4d, files, y, offset):
    # Build each pattern subtexture concurrently:
    cores = max(1, cpu_count() - 1)
    pool = Pool(processes=cores) 
    pool.map(tex2dortho, [(i, files[i].name, offset) for i in range(16)])

    # Then, assemble the 4D texture:
    # (for orthogonal patterns, we compress the texture coordinates quadratically,
    #  to be able to reach longer distances for a given texture size)
    pos = vec2(0, 5 * SIZE_ORTHO * y)
    assemble(tex4d, files, edgesortho, pos, SIZE_ORTHO, lambda v: (v[0]**2, v[1]**2))

# Calculate the diagonal patterns 4D texture for a given offset:
def tex4ddiag(tex4d, files, y, offset):
    # Build each pattern subtexture concurrently:
    cores = max(1, cpu_count() - 1)
    pool = Pool(processes=cores) 
    pool.map(tex2ddiag, [(i, files[i].name, offset) for i in range(16)])

    # Then, assemble the 4D texture:
    pos = vec2(5 * SIZE_ORTHO, 4 * SIZE_DIAG * y)
    assemble(tex4d, files, edgesdiag, pos, SIZE_DIAG, lambda v: v);

#------------------------------------------------------------------------------
# Entry Point

# Copy the texture to a DirectX 9 friendly format:
def dx9(tex4d):
    tex4d_dx9 = Image.new("RGBA", tex4d.size)
    for x in range(tex4d.size[0]):
        for y in range(tex4d.size[1]):
            p = tex4d.getpixel((x, y))
            tex4d_dx9.putpixel((x, y), la(p))
    tex4d_dx9.save("AreaTexDX9.tga")

if __name__ == '__main__':
    # Create temporal textures:
    files = [NamedTemporaryFile(delete=False) for i in range(16)]

    # Create AreaTexDX10:
    tex4d = Image.new("RGBA", (2 * 5 * SIZE_ORTHO, len(SUBSAMPLE_OFFSETS_ORTHO) * 5 * SIZE_ORTHO))
    for y, offset in enumerate(SUBSAMPLE_OFFSETS_ORTHO):
        tex4dortho(tex4d, files, y, offset)
    for y, offset in enumerate(SUBSAMPLE_OFFSETS_DIAG):
        tex4ddiag(tex4d, files, y, offset)
    tex4d.save("AreaTexDX10.tga")

    # Convert to DX9 (AreaTexDX9):
    dx9(tex4d)

    # Output C++ code:
    cpp(tex4d)
