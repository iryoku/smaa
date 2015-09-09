sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	Demo.cpp \
	SMAA.cpp \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES))



dx9demo_MODULES:=dxut Support


ifeq ($(DX9),y)

PROGRAMS+= \
	dx9demo \
	#empty line

endif


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
