CC=g++

.PHONY: all clean

all: target debugger


clean:
	rm target debugger

target: target.cpp
	$(CC) $^ -o target -fno-PIE -no-pie -fPIC -O0

debugger: debugger.cpp
	$(CC) $^ -o debugger
