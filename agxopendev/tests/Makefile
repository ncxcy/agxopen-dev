CC ?= gcc
CXX ?= g++
CFLAGS ?= -Wall -Wextra -pedantic -Werror -O2
CXXFLAGS ?= -Wall -Wextra -pedantic -Werror -O2

INC = -I../include -I.
CORE = ../src/rtkit.c ../src/power.c
C_STANDARDS = c99 c11 c17
CXX_STANDARDS = c++11 c++14 c++17 c++20

KSRCS = ../src/mailbox.c ../src/rtbuddy.c ../src/display.c ../src/rtkit.c ../src/power.c
KFLAGS = -D__KERNEL__ -Ikshim -I../include -I../src -I. -g -O1
KCFLAGS_ALL = -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare
KCXXFLAGS_ALL = -std=gnu++17 -Wall -Wextra -Werror -Wno-unused-parameter
SAN =
KOBJ = kobj
KBIN = agxdriverharness

all: portable regs sim driver

driver:
	mkdir -p $(KOBJ)
	@for f in $(KSRCS); do \
		$(CC) $(KCFLAGS_ALL) $(KFLAGS) $(SAN) -c $$f -o $(KOBJ)/`basename $$f .c`.o || exit 1; \
	done
	$(CXX) $(KCXXFLAGS_ALL) $(KFLAGS) $(SAN) driver_harness.cpp $(KOBJ)/*.o -lpthread -o $(KBIN)
	./$(KBIN)

driver_asan:
	$(MAKE) driver SAN="-fsanitize=address,undefined" KOBJ=kobj_asan KBIN=agxdriverharness_asan

driver_tsan:
	$(MAKE) driver SAN="-fsanitize=thread" KOBJ=kobj_tsan KBIN=agxdriverharness_tsan

portable:
	@for std in $(C_STANDARDS); do \
		$(CC) -std=$$std $(CFLAGS) $(INC) portable_c.c $(CORE) -o agxportablec || exit 1; \
		./agxportablec || exit 1; \
	done
	$(CC) -std=c11 $(CFLAGS) $(INC) -c ../src/rtkit.c -o agxrtkit.o
	$(CC) -std=c11 $(CFLAGS) $(INC) -c ../src/power.c -o agxpower.o
	@for std in $(CXX_STANDARDS); do \
		$(CXX) -std=$$std $(CXXFLAGS) $(INC) portable_cpp.cpp agxrtkit.o agxpower.o -o agxportablecpp || exit 1; \
		./agxportablecpp || exit 1; \
	done

regs:
	$(CC) -std=c11 $(CFLAGS) -I../src $(INC) -c ../src/power.c -o agxpower.o
	$(CC) -std=c11 $(CFLAGS) -I../src $(INC) -c ../src/uat.c -o agxuat.o
	$(CC) -std=c11 $(CFLAGS) -I../src $(INC) -c ../src/gfxhandoff.c -o agxgfxhandoff.o
	$(CXX) -std=c++17 $(CXXFLAGS) -I../src $(INC) -lpthread regs_test.cpp uat_test.cpp handoff_test.cpp agxpower.o agxuat.o agxgfxhandoff.o -o agxregstest
	./agxregstest

sim:
	$(CC) -std=c11 $(CFLAGS) $(INC) -c ../src/rtkit.c -o agxrtkit.o
	$(CC) -std=c11 $(CFLAGS) $(INC) -c ../src/power.c -o agxpower.o
	$(CXX) -std=c++17 $(CXXFLAGS) $(INC) rtkit_sim.cpp agxrtkit.o agxpower.o -o agxrtkitsim
	./agxrtkitsim

sanitize: driver_asan driver_tsan
	$(CC) -std=c11 $(CFLAGS) -g -fsanitize=address,undefined -I../src $(INC) -c ../src/power.c -o agxpower_asan.o
	$(CC) -std=c11 $(CFLAGS) -g -fsanitize=address,undefined -I../src $(INC) -c ../src/uat.c -o agxuat_asan.o
	$(CC) -std=c11 $(CFLAGS) -g -fsanitize=address,undefined -I../src $(INC) -c ../src/gfxhandoff.c -o agxgfxhandoff_asan.o
	$(CXX) -std=c++17 $(CXXFLAGS) -g -fsanitize=address,undefined -I../src $(INC) -lpthread regs_test.cpp uat_test.cpp handoff_test.cpp agxpower_asan.o agxuat_asan.o agxgfxhandoff_asan.o -o agxregstest_asan
	./agxregstest_asan
	$(CC) -std=c11 $(CFLAGS) -g -fsanitize=thread -I../src $(INC) -c ../src/power.c -o agxpower_tsan.o
	$(CC) -std=c11 $(CFLAGS) -g -fsanitize=thread -I../src $(INC) -c ../src/uat.c -o agxuat_tsan.o
	$(CC) -std=c11 $(CFLAGS) -g -fsanitize=thread -I../src $(INC) -c ../src/gfxhandoff.c -o agxgfxhandoff_tsan.o
	$(CXX) -std=c++17 $(CXXFLAGS) -g -fsanitize=thread -I../src $(INC) -lpthread regs_test.cpp uat_test.cpp handoff_test.cpp agxpower_tsan.o agxuat_tsan.o agxgfxhandoff_tsan.o -o agxregstest_tsan
	./agxregstest_tsan
	$(CC) -std=c11 $(CFLAGS) -g -fsanitize=address,undefined $(INC) -c ../src/rtkit.c -o agxrtkit_san.o
	$(CC) -std=c11 $(CFLAGS) -g -fsanitize=address,undefined $(INC) -c ../src/power.c -o agxpower_san.o
	$(CXX) -std=c++17 $(CXXFLAGS) -g -fsanitize=address,undefined $(INC) rtkit_sim.cpp agxrtkit_san.o agxpower_san.o -o agxrtkitsim_san
	./agxrtkitsim_san

clean:
	rm -rf kobj kobj_asan kobj_tsan agxdriverharness agxdriverharness_asan agxdriverharness_tsan
	rm -f agxportablec agxportablecpp agxregstest agxregstest_asan agxregstest_tsan agxrtkitsim agxrtkitsim_san agxregstest_san *.o

.PHONY: all portable regs sim driver driver_asan driver_tsan sanitize clean
