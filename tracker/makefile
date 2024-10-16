SOURCES = $(wildcard *.cpp)
EXECUTABLE = tracker
all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	g++ -std=c++17 -Wall -o $@ $(SOURCES)

clean:
	rm -f $(EXECUTABLE)