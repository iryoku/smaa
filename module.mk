.PHONY: default all bindirs clean distclean


.SUFFIXES:

#initialize these
PROGRAMS:=
ALLSRC:=
# directories which might contain object files
# used for both clean and bindirs
ALLDIRS:=

default: all


ifeq ($(ASAN),y)

OPTFLAGS+=-fsanitize=address
LDFLAGS_asan?=-fsanitize=address
LDFLAGS+=$(LDFLAGS_asan)

endif


ifeq ($(TSAN),y)

CFLAGS+=-DPIC
OPTFLAGS+=-fsanitize=thread -fpic
LDFLAGS_tsan?=-fsanitize=thread -pie
LDFLAGS+=$(LDFLAGS_tsan)

endif


ifeq ($(UBSAN),y)

OPTFLAGS+=-fsanitize=undefined -fno-sanitize-recover
LDFLAGS+=-fsanitize=undefined -fno-sanitize-recover
CXXFLAGS+=-frtti

endif


ifeq ($(LTO),y)

CFLAGS+=$(LTOCFLAGS)
LDFLAGS+=$(LTOLDFLAGS) $(OPTFLAGS)

endif


ifeq ($(DX9),y)

CFLAGS+=-DTIMER_DIRECTX_9

endif


CFLAGS+=-I$(TOPDIR)/Demo/DXUT/Core
CFLAGS+=-I$(TOPDIR)/Demo/DXUT/Optional
CFLAGS+=-I$(TOPDIR)/Demo/Support
CFLAGS+=-I$(TOPDIR)/Textures

CFLAGS+=$(OPTFLAGS)


# (call directory-module, dirname)
define directory-module

# save old
DIRS_$1:=$$(DIRS)

dir:=$1
include $(TOPDIR)/$1/module.mk

ALLDIRS+=$1

ALLSRC+=$$(SRC_$1)

# restore saved
DIRS:=$$(DIRS_$1)

endef  # directory-module


DIRS:= \
	Demo \
	# empty line
$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


TARGETS:=$(foreach PROG,$(PROGRAMS),$(PROG)$(EXESUFFIX))

all: $(TARGETS)


# check if a directory needs to be created
# can't use targets with the same name as directory because unfortunate
# interaction with VPATH (targets always exists because source dir)
#  $(call missingdir, progname)
define missingdir

ifneq ($$(shell test -d $1 && echo n),n)
MISSINGDIRS+=$1
endif

endef # missingdir

MISSINGDIRS:=
$(eval $(foreach d, $(ALLDIRS), $(call missingdir,$(d)) ))


# create directories which might contain object files
bindirs:
ifneq ($(MISSINGDIRS),)
	mkdir -p $(MISSINGDIRS)
endif


clean:
	rm -f $(TARGETS) $(foreach dir,$(ALLDIRS),$(dir)/*$(OBJSUFFIX))

distclean: clean
	rm -f $(foreach dir,$(ALLDIRS),$(dir)/*.d)
	-rmdir -p --ignore-fail-on-non-empty $(ALLDIRS)


# rules here

%$(OBJSUFFIX): %.c | bindirs
	$(CC) -c -MF $*.d -MP -MMD -std=gnu99 $(CFLAGS) -o $@ $<


%$(OBJSUFFIX): %.cpp | bindirs
	$(CXX) -c -MF $*.d -MP -MMD $(CXXFLAGS) -o $@ $<


%$(RESOURCESUFFIX): %.rc | bindirs
	$(RC) $(RCOUT)$@ $<


# $(call program-target, progname)
define program-target

ALLSRC+=$$(filter %.c,$$($1_SRC))
ALLSRC+=$$(filter %.cpp,$$($1_SRC))

$1_SRC+=$$(foreach module, $$($1_MODULES), $$(SRC_$$(module)))

$1_OBJ:=$$($1_SRC:.c=$(OBJSUFFIX))
$1_OBJ:=$$($1_OBJ:.cpp=$(OBJSUFFIX))
$1_OBJ:=$$($1_OBJ:.rc=$(RESOURCESUFFIX))
$1$(EXESUFFIX): $$($1_OBJ) | bindirs
	$$(CXX) $(LDFLAGS) -o $$@ $$^ $$(foreach module, $$($1_MODULES), $$(LDLIBS_$$(module))) $$($1_LIBS) $(LDLIBS)

endef  # program-target


$(eval $(foreach PROGRAM,$(PROGRAMS), $(call program-target,$(PROGRAM)) ) )


-include $(foreach FILE,$(ALLSRC),$(patsubst %.cpp,%.d,$(patsubst %.c,%.d,$(FILE))))
