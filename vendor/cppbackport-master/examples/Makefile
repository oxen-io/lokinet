# Sources are located relative to Makefile
MAKEFILE_DIR := $(shell readlink -f $(dir $(word $(words $(MAKEFILE_LIST)), $(MAKEFILE_LIST))))
LIB_DIR := $(MAKEFILE_DIR)/../lib

# The path of the invoking module
vpath %.cpp $(MAKEFILE_DIR)

default: examples

include $(MAKEFILE_DIR)/../make.inc

CXXFLAGS += -pthread
INCLUDE += -iquote$(LIB_DIR)
LDFLAGS += -pthread

LIBNAME = cppbackport
EXAMPLES = any fbind histogram maxalign optional smartptr two_threads clock gibberish \
    lshere multithreading multitype pathiter tempdir directory_listing hash mathfns now singlell \
    testtraits

.PHONY: default lib examples

lib:
	$(MAKE) -f $(LIB_DIR)/Makefile

$(EXAMPLES): lib

examples: $(EXAMPLES)
any: any.o
clock: clock.o
directory_listing: directory_listing.o
fbind: fbind.o
gibberish: gibberish.o
hash: hash.o
histogram: histogram.o
lshere: lshere.o
mathfns: mathfns.o
maxalign: maxalign.o
multithreading: multithreading.o
multitype: multitype.o
now: now.o
optional: optional.o
pathiter: pathiter.o
singlell: singlell.o
smartptr: smartptr.o
tempdir: tempdir.o
testtraits: testtraits.o
two_threads: two_threads.o

$(EXAMPLES):
	$(LD) $(LDFLAGS) $(@).o -L. -l$(LIBNAME) -o $@

clean:
	$(MAKE) -f $(LIB_DIR)/Makefile clean
	@$(RM) $(EXAMPLES) $(EXAMPLES:%=%.o)
