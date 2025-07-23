CXX = g++
CXXFLAGS = -std=c++11 -O3 -march=native -flto
LDFLAGS = -flto

TARGET = v4l2_test
SRCS = main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
