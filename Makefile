CC = gcc
CFLAGS = -g -Wall -Werror -Idudect -I.

GIT_HOOKS := .git/hooks/applied
obj=Binary_Trie


all: $(GIT_HOOKS) ${obj}

tid := 0

# Control test case option of valgrind
ifeq ("$(tid)","0")
    TCASE :=
else
    TCASE := -t $(tid)
endif

# Control the build verbosity
ifeq ("$(VERBOSE)","1")
    Q :=
    VECHO = @true
else
    Q := @
    VECHO = @printf
endif

# Enable sanitizer(s) or not
ifeq ("$(SANITIZER)","1")
    # https://github.com/google/sanitizers/wiki/AddressSanitizerFlags
    CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-common
    LDFLAGS += -fsanitize=address
endif

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

OBJS := ${obj}.o report.o harness.o

deps := $(OBJS:%.o=.%.o.d)

       
${obj}: $(OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) $(LDFLAGS) -o $@ $^ -lm

%.o: %.c
	@mkdir -p .$(DUT_DIR)
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) -o $@ $(CFLAGS) -c -MMD -MF .$@.d $<


valgrind_existence:
	@which valgrind 2>&1 > /dev/null || (echo "FATAL: valgrind not found"; exit 1)

valgrind: valgrind_existence
	# Explicitly disable sanitizer(s)
	$(MAKE) clean SANITIZER=0 qtest
	$(eval patched_file := $(shell mktemp /tmp/${obj}.XXXXXX))
	cp ${obj} $(patched_file)
	chmod u+x $(patched_file)
	sed -i "s/alarm/isnan/g" $(patched_file)
	#scripts/driver.py -p $(patched_file) --valgrind $(TCASE)
	##TODO: Add test
	@echo
	@echo "Test with specific case by running command:" 

clean:
	rm -f $(OBJS) $(deps) *~ ${obj} /tmp/${obj}.*
	rm -rf *.dSYM
	(cd traces; rm -f *~)

-include $(deps)
