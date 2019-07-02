# This Makefile is intended to be included by another Makefile. It contains a
# number of common functions, variables, recipes. Ideally the calling Makefile
# will only need to define targets for that project.
#
# Features:
#  - Shadow building
#  - Auto dependency generation
#  - debug target uses address sanitizer when available (GCC >= 4.8.0),
#    and undefined behavior sanitizer (GCC >=4.9.0)
#  - coloured error messages when available (GCC >=4.9.0)
#  - pre-configured targets for cleaning .o files, insure leftovers
#  - targets for callgrind, memcheck, insure, profiling and code coverage
#
# Organization
#  - Supporting functions
#  - Definitions of the variables used by this makefile (ex., CXX, WARNINGS)
#  - Predefined configurations of variables (ex., DEBUG)
#  - File type targets (ex., .o)
#  - Named targets (ex., profile)
#  - Inclusion of auto-dependencies
#
# TODO Profile guided optimization
# TODO Precompiled headers
# TODO Static analysis
# TODO Clang support
# TODO -fsanitize=integer for clang
# TODO -fsanitize=thread
# TODO ubsan prints messages to the stderr; does not terminate
# TODO dynamic/static libs
# TODO _GLIBCXX_DEBUG and _GLIBCXX_DEBUG_PEDANTIC
# TODO split up makefile into common functions, variables, configurations, recipes
# TODO -B option for TOOLCHAIN
# TODO CONFIG=RELEASE (strip, for example)
# TODO Different debug levels
# TODO CONFIG=HARDENED including some of the following options
#   TODO -fstack-protector-strong?
#   TODO -Wformat-security
#   TODO -pie -fPIE
#   TODO full relro -Wl,-z,relro,-z,now
# TODO -pipe
# TODO -flto

# Shadow building ==============================================================
# This gets the directory of the calling Makefile (and, presumably, where the
# source code resides

# Functions ====================================================================
gccversion = $(shell $(CXX) -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/')
gcc48 = $(shell expr $(gccversion) \>= 40800)
gcc49 = $(shell expr $(gccversion) \>= 40900)
gcc51 = $(shell expr $(gccversion) \>= 50100)

# Convert a word to lowercase
lower = $(shell echo $(1) | tr '[:upper:]' '[:lower:]')

# Convert a word to uppercase
upper = $(shell echo $(1) | tr '[:lower:]' '[:upper:]')

# Variables ====================================================================
#
# The basic variables that this makefile uses

# Toolchain --------------------------------------------------------------------
# Which programs to use during compilation

# Path to a different toolchain (ex., ~/bin/gcc-4.9.0)
TOOLCHAIN =

ifndef TOOLCHAIN
# C++ compiler
CXX = g++

# Archive (.a) program
AR = ar

# Linker
LD = $(CXX)

PROFILER = gprof
else
# Use a particular set of tools
CXX = $(TOOLCHAIN)/bin/g++ -B$(TOOLCHAIN)/bin/
AR = $(TOOLCHAIN)/bin/ar
LD = $(CXX)
PROFILER = $(TOOLCHAIN)/bin/gprof
endif

# Misc
VALGRIND = valgrind

# Compiler flags ---------------------------------------------------------------
# Arguments to the toolchain programs

# Which version of C++ to compile for
STANDARD = -ansi -pedantic

# Optimization flags
OPTIMIZATION = -O2

# Architecture flags
TARGET_ARCH =

# Warnings
WARNINGS = -Wall -Wsign-compare -Wconversion -Wpointer-arith \
 -Winit-self -Wcast-qual -Wredundant-decls -Wcast-align -Wwrite-strings \
 -Wno-long-long -Woverloaded-virtual -Wformat -Wno-unknown-pragmas \
 -Wnon-virtual-dtor -Wno-c++0x-compat
# -Wold-style-cast

ifeq "$(gcc51)" "1"
#WARNINGS += -Wsuggest-attribute=const -Wsuggest-attribute=pure
endif

# Level of debugging information to provide
DEBUGGING = -g

# Machine independent options
#-fno-nonansi-builtins
OPTIONS = -fuse-cxa-atexit -ffor-scope

ifeq "$(gcc49)" "1"
OPTIONS += -fdiagnostics-color
endif

# Flags to use when compiling C++ source into object files
CXXFLAGS = $(STANDARD) $(DEBUGGING) $(OPTIMIZATION) $(WARNINGS) $(OPTIONS) $(TARGET_ARCH)

# Directories to search for includes
INCLUDE =

# Any macros that can be defined on the command line
DEFINES = -D_FORTIFY_SOURCE=1

# Preprocessor flags
CPPFLAGS = $(INCLUDE) $(DEFINES)

# How to build an archive
ARFLAGS = crs

# Linker flags
LDFLAGS =
LDLIBS =

ifdef TOOLCHAIN
# Statically link the GCC and 
LDFLAGS += -static-libgcc -static-libstdc++

# Dynamic linking
#LDFLAGS += -Wl,-rpath -Wl,$(TOOLCHAIN)/lib
#LDLIBS = -L$(TOOLCHAIN)/lib
endif

# Printing =====================================================================
# If this is not empty, print messages
VERBOSE =

# Predefined configurations ====================================================
# Combinations of the OPTIMIZATION, etc.
ifdef CONFIG
UCONFIG := $(call upper,$(CONFIG))

# Fast -------------------------------------------------------------------------
# A faster build, primarily for testing
ifeq "$(UCONFIG)" "FAST"
OPTIMIZATION = -O0
endif

# Tune -------------------------------------------------------------------------
# Heavily optimized for a fast application
ifeq "$(UCONFIG)" "TUNE"
ifeq "$(gcc48)" "1"
TARGET_ARCH = -march=native
endif
OPTIMIZATION = -O3 -mtune=native
endif

# Debug ------------------------------------------------------------------------
# Programmer is tracking a bug
ifeq "$(UCONFIG)" "DEBUG"

# Set additional warnings and flags
WARNINGS += -Wshadow -Wfloat-equal
ifeq "$(gcc51)" "1"
WARNINGS += -Wstrict-overflow=5
endif

DEBUGGING = -ggdb3 -fno-omit-frame-pointer -fno-default-inline -fno-inline -fno-merge-constants -ffloat-store
# -fno-elide-constructors
	
# Set the optimization level
ifeq "$(gcc48)" "1"
OPTIMIZATION = -Og
else
OPTIMIZATION = -O0
endif

# Turn on address sanitizer
ifeq "$(gcc48)" "1"
DEBUGGING += -fsanitize=address
LDFLAGS += -fsanitize=address -static-libasan
endif

# Turn on undefined behavior sanitizer and leak sanitizer
ifeq "$(gcc49)" "1"
DEBUGGING += -fsanitize=undefined -fsanitize=leak
LDFLAGS += -fsanitize=undefined -fsanitize=leak -static-libubsan -static-liblsan
endif
endif

# Profile ----------------------------------------------------------------------
# Programmer wants profile information
ifeq "$(UCONFIG)" "PROFILE"
OPTIMIZATION = -O2 -pg
LDFLAGS += -pg
endif

# Coverage ---------------------------------------------------------------------
# Programmer wants code coverage information
ifeq "$(UCONFIG)" "COVERAGE"
DEBUGGING += --coverage -fno-elide-constructors
LDFLAGS += --coverage
OPTIMIZATION = -O0
endif

# Insure -----------------------------------------------------------------------
# Programmer wants to run insure
ifeq "$(UCONFIG)" "INSURE"
CXX = insure
LDLIBS += -lstdc++
OPTIMIZATION = -O0
endif
endif

# Targets ======================================================================
.PHONY: no_default_goal depend clean_insure clean_objects clean_profile clean_depend distclean coverage profile memcheck callgrind cachegrind help help_common examples

no_default_goal:
	$(error "The including Makefile hasn't specified a default goal. It should.")

# Dependency generation --------------------------------------------------------
DEPDIR := .deps

# Build object files with automatic dependency generation
# TODO maybe use LINK.cpp from 'make -p'
%.o: %.cpp
	@mkdir -p $(@D)/$(DEPDIR)
ifeq ($(VERBOSE),)
	@echo [CXX] $@
	@$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -MF $(@D)/$(DEPDIR)/$(*F).d -o $@ $<
else
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -MF $(@D)/$(DEPDIR)/$(*F).d -o $@ $<
endif

%.a:
ifeq ($(VERBOSE),)
	@echo [AR] $@
	@$(AR) $(ARFLAGS) $@ $^ > /dev/null
else
	$(AR) $(ARFLAGS) $@ $^ > /dev/null
endif

# A script for combining .a's. Dependency list for the mri should be
# subdirectories containing static libs. Name of the output library
# is inferred from the mri name (abc.mri produces libabc.a)
%.mri:
	@echo 'CREATE $(patsubst %.mri,lib%.a,$@)' > $@
	@for dir in $^; do echo ADDLIB $$dir >> $@; done
	@echo 'SAVE' >> $@


# TODO dynamic libs
# %.so: CXXFLAGS += -fPIC
# %.so: LDFLAGS := -shared
# %.so:
#	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
# optionally add -Wl,-soname,$(SONAME)

# TODO java classes
# %.class: %.java
#	$(JC) -cp . $<

# Code coverage ----------------------------------------------------------------
coverage:
ifdef COVERAGE_OUT
	@TMPFILE=`mktemp`; lcov --capture --directory . --output-file $$TMPFILE; genhtml $$TMPFILE --output-directory $(COVERAGE_OUT); $(RM) $$TMPFILE
else
	$(error You have not set COVERAGE_OUT)
endif

# Profiling --------------------------------------------------------------------
profile:
ifdef PROFILE_TARGET
	$(PROFILE_TARGET) $(PROFILE_TARGET_ARGS)
ifdef PROFILE_OUT
	@$(PROFILER) -c $(PROFILE_TARGET) > $(PROFILE_OUT)
else
	$(error You have not specified PROFILE_OUT)
endif
else
	$(error You have not specified PROFILE_TARGET)
endif

# Memcheck ---------------------------------------------------------------------
# User should set MEMCHECK_TARGET, MEMCHECK_TARGET_ARGS, and a recipe like:
# memcheck: $(MEMCHECK_TARGET)
memcheck:
	$(VALGRIND) --tool=memcheck -v --read-var-info=yes --track-origins=yes --show-reachable=yes --leak-check=full --read-inline-info=yes --keep-stacktraces=alloc-and-free $(VALGRIND_TARGET) $(VALGRIND_TARGET_ARGS) 2> $(MEMCHECK_OUT)

# Callgrind --------------------------------------------------------------------
# User should set CALLGRIND_TARGET, CALLGRIND_TARGET_ARGS, and a recipe like:
# callgrind: $(CALLGRIND_TARGET)
callgrind:
	$(VALGRIND) --tool=callgrind $(VALGRIND_TARGET) $(VALGRIND_TARGET_ARGS)

cachegrind:
	$(VALGRIND) --tool=cachegrind --branch-sim=yes $(VALGRIND_TARGET) $(VALGRIND_TARGET_ARGS)

# Clean targets ----------------------------------------------------------------
# remove all .o files
clean_objects:
ifeq ($(VERBOSE),)
	@find -name '*.o' -exec $(RM) '{}' \;
else
	find -name '*.o' -exec $(RM) '{}' \;
endif

# remove all files left by insure
clean_insure:
ifeq ($(VERBOSE),)
	@find -name '*.tcl' -exec $(RM) '{}' \;
	@$(RM) tca.map tca.log
else
	find -name '*.tcl' -exec $(RM) '{}' \;
	$(RM) tca.map tca.log
endif

# remove all files left by gprof
clean_profile:
ifeq ($(VERBOSE),)
	@$(RM) profile
	@find \( -name '*.gcda' -o -name '*.gcno' \) -exec $(RM) '{}' \;
else
	$(RM) profile
	find \( -name '*.gcda' -o -name '*.gcno' \) -exec $(RM) '{}' \;
endif

clean_depend:
ifeq ($(VERBOSE),)
	@find -depth -name $(DEPDIR) -type d -exec $(RM) -r '{}' \;
else
	find -depth -name $(DEPDIR) -type d -exec $(RM) -r '{}' \;
endif

depend: clean clean_depend

distclean: depend

# Documentation ----------------------------------------------------------------
help_common:
	@echo "==================================================================="
	@echo "Here are some standard Makefile variables and their defaults:"
	@echo "  CXX      ($(CXX))"
	@echo "  CPPFLAGS ($(CPPFLAGS))"
	@echo "  LDFLAGS  ($(LDFLAGS))"
	@echo "  LDLIBS   ($(LDLIBS))"
	@echo "  AR       ($(AR))"
	@echo "  ARFLAGS  ($(ARFLAGS))"
	@echo
	@echo "You can set the TOOLCHAIN variable to use a different compiler,"
	@echo "linker, profiler. Ex., (~/bin/gcc-4.9.0)"
	@echo
	@echo "The CXXFLAGS variable is comprised of a number of other variables:"
	@echo "  STANDARD       ($(STANDARD))"
	@echo "  DEBUGGING      ($(DEBUGGING))"
	@echo "  OPTIMIZATION   ($(OPTIMIZATION))"
	@echo "  WARNINGS       ($(WARNINGS))"
	@echo "  TARGET_ARCH    ($(TARGET_ARCH))"
	@echo
	@echo "For convenience, several preconfigured options are available by"
	@echo "setting the CONFIG variale:"
	@echo "  DEBUG    - Turns on debugging info; turns down optimization; enables"
	@echo "             address sanitizer, etc. if available"
	@echo "  PROFILE  - Keeps optimization for release build; adds debugging"
	@echo "             and gprof flags"
	@echo "  COVERAGE - Don't inline functions"
	@echo "  INSURE   - Build the target with the insure compiler"
	@echo "  TUNE     - High optimization level; CPU-specific instructions"
	@echo "  FAST     - A fast compilation with low optimization level"
	@echo
	@echo "==================================================================="
	@echo "Here are some special targets and variables that affect them"
	@echo
	@echo "  profile    - First build a target with CONFIG=PROFILE and run it."
	@echo "       PROFILE_TARGET the name of the program to profile"
	@echo "            (default: $(PROFILE_TARGET))"
	@echo "       PROFILE_TARGET_ARGS arguments to the program to profile"
	@echo "            (default: $(PROFILE_TARGET_ARGS))"
	@echo "       PROFILE_OUT the file to place the profile data in"
	@echo "            (default: $(PROFILE_OUT))"
	@echo "       PROFILER Path to profiler"
	@echo "           (default: $(PROFILER))"
	@echo "  coverage   - First build a target with CONFIG=COVERAGE and run it."
	@echo "       COVERAGE_OUT specifies where to put the resulting web pages"
	@echo "           (default: $(COVERAGE_OUT))"
	@echo
	@echo "  memcheck   - Run memcheck for VALGRIND_TARGET, testing memory access"
	@echo "  callgrind  - Run callgrind for VALGRIND_TARGET, testing performance"
	@echo "  cachegrind - Run cachegrind for VALGRIND_TARGET, testing cache"
	@echo "       VALGRIND_TARGET The program to check"
	@echo "           (default: $(VALGRIND_TARGET))"
	@echo "       VALGRIND_TARGET_ARGS The arguments to VALGRIND_TARGET"
	@echo "           (default: $(VALGRIND_TARGET_ARGS))"
	@echo "       MEMCHECK_OUT Where to put the memcheck results"
	@echo "           (default: $(MEMCHECK_OUT))"
	@echo "==================================================================="

help: help_common

# ==============================================================================
# Include dependencies which are stored as .d files in the $(DEPDIR) directories
# TODO prefer -wholename to -path, but old version of find doesn't have wholename
-include $(shell find -path '*/$(DEPDIR)/*.d')
