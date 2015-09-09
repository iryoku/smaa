sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	DXUTcamera.cpp \
	DXUTgui.cpp \
	DXUTres.cpp \
	DXUTsettingsdlg.cpp \
	DXUTShapes.cpp \
	SDKmesh.cpp \
	SDKmisc.cpp \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
