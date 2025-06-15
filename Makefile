GCC:=gcc
RM:=rm
CFLAGS:=-std=gnu23
CFLAGS+=-g
CFLAGS+=-O3
LIBS=m

QUIET:=$(if $(VERBOSE),,@)

.PHONY: run clean debug
all: randtest

run: randtest
	@echo  "  RUN     randtest"
	$(QUIET)./randtest

debug: randtest
	@echo  "  DEBUG   randtest"
	$(QUIET)gdb -ex run ./randtest

randtest: randtest.c
	@echo  "  CC      $<"
	$(QUIET)$(GCC) $(CFLAGS) -o $@ $< $(patsubst %,-l%,$(LIBS))

clean:
	@echo  "  RM      randtest"
	$(QUIET)$(RM) randtest || true
