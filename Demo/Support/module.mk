sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	Camera.cpp \
	Copy.cpp \
	RenderTarget.cpp \
	Timer.cpp \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES))
SRC_Support:=$(SRC_$(d))


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
