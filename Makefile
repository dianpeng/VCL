# ========================================================================
#
#  Stupid Makefile for building VCL
#
# ========================================================================
TESTFLAG=-Isrc/ -g3 -Iinclude/ -Wall -fsanitize=address -DVCL_MINIMUM_GC_GAP=0

# Testing libs
TESTLIB=-lglog -lpcre -lgtest -lpthread -lboost_system -lboost_filesystem

# Libarary libs
LIBRARYLIB=-lglog -lpcre -lpthread -lboost_system

# Library flags
LIBRARYFLAG=-Isrc/ -g3 -O3 -Iinclude/ -Wall -fsanitize=address

# Production flags
PRODUCTIONFLAG=-Isrc -g3 -Iinclude/ -Wall -DNDEBUG

# Files
SOURCE:=$(shell find src/ -type f -name "*.cc")
INCLUDE:=$(shell find include/ -type f -name "*.h")
TEST:=$(shell find test/ -type f -name "*.cc")
OBJ:=${SOURCE:.cc=.o}
TESTEXEC:=${TEST:.cc=.t}
INSTALLPATH:=/usr

# =======================================================================
# Target
# =======================================================================
all : $(OBJ)
	ar rcs libvcl.a $(OBJ)

src/%.o : src/%.cc
	$(CXX) $(LIBRARYFLAG) -c -o $@ $<

install: all
	cp -r include/vcl/ $(INSTALLPATH)/include
	cp libvcl.a $(INSTALLPATH)/lib

uninstall:
	rm -rf $(INSTALLPATH)/include/vcl
	rm $(INSTALLPATH)/lib/libvcl.a

# =======================================================================
# Test
# =======================================================================
zone-test: $(SOURCE) $(INCLUDE) test/vm/zone-test.cc
	$(CXX) $(TESTFLAG) test/vm/zone-test.cc $(TESTLIB) -o zone-test

lexer-test: $(SOURCE) $(INCLUDE) test/vm/lexer-test.cc
	$(CXX) $(TESTFLAG) src/util.cc src/vm/lexer.cc test/vm/lexer-test.cc $(TESTLIB) -o lexer-test

parser-test: $(SOURCE) $(INCLUDE) test/vm/parser-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/parser-test.cc $(TESTLIB) -o parser-test

compiler-test: $(SOURCE) $(INCLUDE) test/vm/compiler-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/compiler-test.cc $(TESTLIB) -o compiler-test

runtime-test: $(SOURCE) $(INCLUDE) test/vm/runtime-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/runtime-test.cc $(TESTLIB) -o runtime-test

yield-test: $(SOURCE) $(INCLUDE) test/vm/yield-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/yield-test.cc $(TESTLIB) -o yield-test

vcl-test: $(SOURCE) $(INCLUDE) test/vm/vcl-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/vcl-test.cc $(TESTLIB) -o vcl-test

gc-test: $(SOURCE) $(INCLUDE) test/vm/gc-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/gc-test.cc $(TESTLIB) -o gc-test

compilation-unit-test: $(SOURCE) $(INCLUDE) test/vm/compilation-unit-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/compilation-unit-test.cc $(TESTLIB) -o compilation-test

driver: $(SOURCE) $(INCLUDE) test/vm/driver.cc
	$(CXX) $(PRODUCTIONFLAG) $(SOURCE) test/vm/driver.cc $(TESTLIB) -o driver

# =========================================================================
#
# Transpiler
#
# =========================================================================
template-unit-test: $(SOURCE) $(INCLUDE) test/vm/transpiler/template-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/transpiler/template-test.cc $(TESTLIB) -o template-test

lua51-unit-test: $(SOURCE) $(INCLUDE) test/vm/transpiler/target-lua51-test.cc
	$(CXX) $(TESTFLAG) $(SOURCE) test/vm/transpiler/target-lua51-test.cc $(TESTLIB) -o lua51-test

test/%.t : test/%.cc
	$(CXX) $(SOURCE) $(TESTFLAG) -o $@ $< $(TESTLIB)

test : $(TESTEXEC)
	./test/vm/zone-test.t
	./test/vm/lexer-test.t
	./test/vm/parser-test.t
	./test/vm/compiler-test.t
	./test/vm/runtime-test.t
	./test/vm/yield-test.t
	./test/vm/vcl-test.t
	./test/vm/gc-test.t
	./test/vm/compilation-unit-test.t
	./test/vm/driver.t test-case/


.PHONY: clean

clean:
	rm -rf $(OBJ)
	rm -rf $(TESTEXEC)
	rm -rf libvcl.a
