
TERARK_HOME := ../../../../terark
INCS = -I../../../src
CHECK_TERARK_FSA_LIB_UPDATE := 0
FUCK_MAC_MAKE_PWD := $(shell pwd)
LIBS = -L../../../lib -lterark-db-${COMPILER}-r -lboost-filesystem -lboost-system

include ../../../../terark/tools/fsa/Makefile

.PHONY: user.hpp
user.hpp : db/dbmeta.json
	# brain damaged Mac OS X EICapitan SIP requires DYLD_LIBRARY_PATH here
	env DYLD_LIBRARY_PATH=/opt/lib ../terark-db-schema-compile/dbg/terark-db-schema-compile.exe db/dbmeta.json test_ns User > user.hpp

