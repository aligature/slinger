CXX=g++
RM=rm -f
CPPFLAGS = -Wall --std=c++14 -I/opt/local/include -I/opt/build/cpprest/include -DBOOST_LOG_DYN_LINK
LDLIBS = -L/opt/local/lib -L/opt/build/cpprest/lib -lboost_system-mt -lboost_thread-mt -lboost_chrono-mt -lboost_program_options-mt -lboost_log-mt -lboost_filesystem-mt -lcpprest

SOURCES=main.cpp
OBJECTS=$(subst .cpp,.o,$(SOURCES))
OUTPUT=slinger

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	g++ -o $(OUTPUT) $(OBJECTS) $(LDLIBS)

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	g++ $(CPPFLAGS) -MM $(SOURCES) >>./.depend;

clean:
	rm -f $(OBJECTS) $(OUTPUT)

include .depend
