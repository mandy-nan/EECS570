# this directory must be in pin/source/tools/
include ../makefile.gnu.config

PIN_ROOT=$(PWD)/../../..

TOOL=frontend
SIM=rcdcsim
FRONTEND_FILES=PinCallbacks.o frontend.o
SIMULATOR_FILES=SimulatorThread.o cache/Snippets.o
TEST_FILES=test/UnitTestMain.o test/HierarchicalCacheUnitTests.o test/StoreBufferUnitTests.o test/L2StoreBufferUnitTests.o cache/Snippets.o

ifdef PAUSE_TOOL
PAUSE_TOOL_FLAG=-pause_tool 30
else
PAUSE_TOOL_FLAG=
endif

DBG= -Wall -g -O0 -fno-omit-frame-pointer 
OPT=$(DBG)
#OPT=-O3 -DNDEBUG -fomit-frame-pointer 
CXXFLAGS += -I. -Icache -Iboost_lockfree -Wall -Wno-unknown-pragmas -MMD
CXXFLAGS += -fPIC -frounding-math
LIBS = 

# boost_lockfree code is from: "git clone git://tim.klingt.org/boost_lockfree.git"

#all: $(TOOL).so $(SIM) pipefork unittest
all: $(TOOL).so $(SIM) pipefork

## build rules

%.o : %.cpp Makefile
	$(CXX) -c $(CXXFLAGS) $(PIN_CXXFLAGS) -o $@ $<


$(TOOL).so: $(FRONTEND_FILES) $(PIN_LIBNAMES)
	$(CXX) $(PIN_LDFLAGS) $(LDFLAGS) -o $@ $^ $(PIN_LIBS) $(LIBS) 

$(SIM): $(SIMULATOR_FILES)
	$(CXX) $(LDFLAGS) -o $@ $^ -lboost_program_options $(LIBS)

pipefork: pipefork.cpp
	$(CXX) $(LDFLAGS) -o $@ $^

.PHONY: run-unittests
run-unittests: unittest
	./$< --report_level=short --log_level=message

# unit tests require >=libboost-test1.40 ubuntu package
unittest: $(TEST_FILES)
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) -lboost_unit_test_framework -o $@ $^


	#$(PIN_ROOT)/pin $(PAUSE_TOOL_FLAG) -t $(PWD)/$(TOOL).so -tosim-fifo $(PWD)/fifo.frontend -cores 2 --  simple_test/mnan/exe 3 1000&
test: $(TOOL).so
	test -p fifo.frontend || mkfifo fifo.frontend 
	test -p fifo.det-non || mkfifo fifo.det-non
	test -p fifo.det-hb || mkfifo fifo.det-hb
	$(PWD)/pipefork $(PWD)/fifo.frontend $(PWD)/fifo.det-non $(PWD)/fifo.det-hb &
	$(PWD)/$(SIM) --nondet --tosim-fifo $(PWD)/fifo.det-non --ignore-stack --cores 2 --statsfile $(PWD)/det-non.out&
	$(PWD)/$(SIM) --det-hb --tosim-fifo $(PWD)/fifo.det-hb --ignore-stack --cores 2 --statsfile $(PWD)/det-hb.out&
	$(PIN_ROOT)/pin $(PAUSE_TOOL_FLAG) -t $(PWD)/$(TOOL).so -tosim-fifo $(PWD)/fifo.frontend -cores 2 --  simple_test/mnan/exe 3 1000&

test-tso: $(TOOL).so
	test -p fifo.det-tso || mkfifo fifo.det-tso
	test -p fifo.frontend || mkfifo fifo.frontend 
	$(PWD)/pipefork $(PWD)/fifo.frontend $(PWD)/fifo.det-tso &
	$(PWD)/$(SIM) --det-tso --tosim-fifo $(PWD)/fifo.det-tso --ignore-stack --cores 1 --statsfile $(PWD)/tso.txt&
	./simple_test/exe --portability &
	$(PIN_ROOT)/pin $(PAUSE_TOOL_FLAG) -t $(PWD)/$(TOOL).so -tosim-fifo $(PWD)/fifo.frontend -cores 1 -- /bin/ls &

test-hb: $(TOOL).so
	test -p fifo.det-hb || mkfifo fifo.det-hb
	test -p fifo.frontend || mkfifo fifo.frontend 
	$(PWD)/pipefork $(PWD)/fifo.frontend $(PWD)/fifo.det-hb &
	$(PWD)/$(SIM) --det-hb --tosim-fifo $(PWD)/fifo.det-hb --ignore-stack --cores 1 --statsfile $(PWD)/hb.txt&
	./simple_test/exe --portability &
	$(PIN_ROOT)/pin $(PAUSE_TOOL_FLAG) -t $(PWD)/$(TOOL).so -tosim-fifo $(PWD)/fifo.frontend -cores 1 -- /bin/ls &

test-parsec: $(TOOL).so
	test -p fifo.det-hb || mkfifo fifo.det-hb
	test -p fifo.det-tso || mkfifo fifo.det-tso
	test -p fifo.nondet || mkfifo fifo.nondet
	test -p fifo.frontend || mkfifo fifo.frontend
	$(PWD)/pipefork $(PWD)/fifo.frontend $(PWD)/fifo.det-hb $(PWD)/fifo.det-tso $(PWD)/fifo.nondet &
	$(PWD)/$(SIM) --nondet --tosim-fifo $(PWD)/fifo.nondet --ignore-stack --cores 8 --statsfile $(PWD)/.stats-parsec-nondet.py --scheme ParsecTest &
	$(PWD)/$(SIM) --det-tso --tosim-fifo $(PWD)/fifo.det-tso --ignore-stack --cores 8 --statsfile $(PWD)/.stats-parsec-tso.py --scheme ParsecTest &
	$(PWD)/$(SIM) --det-hb --tosim-fifo $(PWD)/fifo.det-hb --ignore-stack --cores 8 --statsfile $(PWD)/.stats-parsec-hb.py --scheme ParsecTest &
	$(HOME)/parsec/bin/parsecmgmt -p streamcluster -c gcc-hooks -i simdev -n 2 -s "$(PIN_ROOT)/pin $(PAUSE_TOOL_FLAG) -t $(PWD)/$(TOOL).so -tosim-fifo $(PWD)/fifo.frontend -cores 8 --" -a run 2>&1 | tee parsec.log &

wc:
	wc -l *.[hc]pp cache/*.[hc]pp

clean:
	-rm -f *.o cache/*.o test/*.o $(TOOL).so $(SIM) pipefork *.out *.tested *.failed *.d cache/*.d test/*.d unittest .st* fifo.*

	#grep TSO output.out > tso.out
	#perl -p -i -w -e 's/TSO//g' tso.out
dmp_chk:
	grep HB output.out > hb.out
	grep NON output.out > non.out
	perl -p -i -w -e 's/NON-DET//g' non.out
	perl -p -i -w -e 's/HB//g' hb.out
	diff non.out hb.out	 

over: clean all

-include *.d test/*.d

