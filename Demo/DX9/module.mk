sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	Code \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES))
dx9demo_SRC:=$(foreach directory, $(DIRS), $(SRC_$(directory)) ) $(d)/Demo.rc

d  := $(dirstack_$(sp))
sp := $(basename $(sp))
