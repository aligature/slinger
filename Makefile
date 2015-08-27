CXX=g++
RM=rm -f
CPPFLAGS = -Wall --std=c++11 -I/opt/local/include -I/opt/build/cpprest/include
LDLIBS = -L/opt/local/lib -L/opt/build/cpprest/lib -lboost_system-mt -lboost_thread-mt -lboost_chrono-mt -lcpprest

SOURCES=main.cpp
OBJECTS=$(subst .cpp,.o,$(SOURCES))

all: slinger

slinger: $(OBJECTS)
	g++ -o slinger $(OBJECTS) $(LDLIBS)

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	g++ $(CPPFLAGS) -MM $(SOURCES) >>./.depend;

clean:
	rm -f $(OBJS)

include .depend
