# example of local configuration
# copy to local.mk

TOPDIR:=..


# default to debugging disabled
DEBUG:=n

# pick one
DX9:=n
DX10:=y

CXX:=msvgcc
CC:=msvgcc
RC:=i686-w64-mingw32-windres
RCOUT:=-o

# /GR[-] enable C++ RTTI
# /WX treat warnings as errors
# /W<n> set warning level (default n=1)
# /Wp64 enable 64 bit porting warnings  ERROR: msvc includes not clean for this!
CFLAGS:=/WX
CFLAGS+=-I$(TOPDIR)
CFLAGS+=-D_CRT_SECURE_NO_DEPRECATE
CFLAGS+=-DUNICODE


# lazy assignment because CFLAGS is changed later
CXXFLAGS=$(CFLAGS)
CXXFLAGS+=/EHsc


LDFLAGS:=/nodefaultlib:libcmt.lib /subsystem:windows /WX
LDLIBS:=advapi32.lib comctl32.lib comdlg32.lib d3dx9.lib d3dx10.lib d3d9.lib
LDLIBS+=dsound.lib dxerr.lib gdi32.lib imm32.lib ole32.lib oleaut32.lib
LDLIBS+=user32.lib shell32.lib version.lib winmm.lib

OBJSUFFIX:=.obj
EXESUFFIX:=.exe
RESOURCESUFFIX:=.res


# the fucker requires different libs for debugging...
ifeq ($(DEBUG),y)

CFLAGS+=/MDd -D_DEBUG
# /RTC1 Enable fast checks (/RTCsu)
# /Z7 enable old-style debug info
OPTFLAGS+=/RTC1 /Z7
LDFLAGS+=/nodefaultlib:msvcrt.lib
LDLIBS+=msvcrtd.lib

else

CFLAGS+=/MD -DNDEBUG
# /O2 maximize speed
# /Oi enable intrinsic functions
# /GA optimize for Windows Application
# /GF enable read-only string pooling
# /fp:fast "fast" floating-point model; results are less predictable
# /arch:<SSE|SSE2>
OPTFLAGS+=/O2 /Oi /GA /GF /fp:fast /arch:SSE2
LDLIBS+=msvcrt.lib

endif
