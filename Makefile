GCC:=gcc
RM:=rm
CFLAGS:=-std=gnu23
CFLAGS+=-g

QUIET:=$(if $(VERBOSE),,@)

.PHONY: run clean
all: randtest

run: randtest
	@echo  "  RUN     randtest"
	$(QUIET)./randtest

randtest: randtest.c
	@echo  "  CC      $<"
	$(QUIET)$(GCC) $(CFLAGS) -o $@ $<

clean:
	@echo  "  RM      randtest"
	$(QUIET)$(RM) randtest || true
