CXX = g++
CXXFLAGS = -g  -L/usr/X11R6/lib -lm -lpthread -lX11  -O0 #-O3 

OBJS = lib/rtsp.cpp lib/rtp.cpp lib/rtcp.cpp lib/packets.h
client: client.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $<
server: server.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $<
all: client server
	@echo "Done"