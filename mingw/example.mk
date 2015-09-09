# example of local configuration
# copy to local.mk


# location of source
TOPDIR:=..


LTO:=n
ASAN:=n
TSAN:=n
UBSAN:=n


# pick one
DX9:=n
DX10:=y


# compiler options etc
CC:=i686-w64-mingw32-gcc
CXX:=i686-w64-mingw32-g++
RC:=i686-w64-mingw32-windres
RCOUT:=-o


CFLAGS:=-I.
CFLAGS+=-fpermissive
CFLAGS+=-municode
CFLAGS+=-gstabs

#CFLAGS+=-Wall -Wextra -Wshadow -Werror

CFLAGS+=-w

OPTFLAGS:=-O2 -mtune=generic


# lazy assignment because CFLAGS is changed later and to get TOPDIR right
CXXFLAGS=$(CFLAGS) -std=c++11 -I$(TOPDIR)/mingw -include $(TOPDIR)/mingw/mingw-dxfix.h


LDFLAGS:=-mwindows
LDFLAGS+=-municode
LDFLAGS+=-gstabs
LDFLAGS+=-static-libstdc++ -static-libgcc
LDLIBS:=-lcomctl32 -ld3d9 -ld3dx9 -ld3dx10 -ldxerr9 -ldxguid


LTOCFLAGS:=-flto
LTOLDFLAGS:=-flto


OBJSUFFIX:=.o
EXESUFFIX:=.exe
RESOURCESUFFIX:=.o
