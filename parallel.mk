CXX = g++
CXXFLAGS = -DTBB_PREVIEW_GLOBAL_CONTROL -std=c++14 -O3 -Wall -ltbb
TARGET = parallelkmeans
SRCS = kmeans-parallel.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)