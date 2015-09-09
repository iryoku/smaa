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



dx10demo_MODULES:=dxut Support


ifeq ($(DX10),y)

PROGRAMS+= \
	dx10demo \
	#empty line

endif


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
