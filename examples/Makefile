# Adapted from https://stackoverflow.com/a/11206700

# only consider subdirs with Makefile
SUBDIRS := $(dir $(wildcard */Makefile))
TARGETS := build check clean fmt lint

# foo/.all bar/.all foo/.clean bar/.clean
SUBDIRS_TARGETS := \
	$(foreach t,$(TARGETS),$(addsuffix $t,$(SUBDIRS)))

# .all .clean
HERE_TARGETS := $(addsuffix $(TARGETS),.)
HERE_ONLY := test bench

.PHONY : $(TARGETS) $(SUBDIRS_TARGETS) $(HERE_TARGETS) $(HERE_ONLY)

# static pattern rule, expands into:
# all clean : % : foo/.% bar/.%
$(TARGETS) : % : $(addsuffix %,$(SUBDIRS)) $(addsuffix %,.)
	@echo 'Done "$*" target'

# here, for foo/.all:
#   $(@D) is foo
#   $(@F) is .all, with leading period
#   $(@F:.%=%) is just all
$(SUBDIRS_TARGETS) :
	$(MAKE) -C $(@D) $(@F:.%=%)

.build:
	deno install

.check:
	deno check

.clean: ;

# Handles .ts and .md in child directories.
.fmt:
	deno fmt

.lint:
	deno lint

# Do not recurse for test
test: build
	deno test --allow-all --quiet .

# Do not recurse for test
bench: build
	deno run --allow-all bench.ts
