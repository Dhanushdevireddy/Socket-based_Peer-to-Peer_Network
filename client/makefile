SOURCES = $(wildcard *.cpp)
EXECUTABLE = client
all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	g++ -std=c++17 -Wall -o $@ $(SOURCES) -lcrypto

clean:
	rm -f $(EXECUTABLE)