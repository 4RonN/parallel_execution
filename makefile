compiler = g++
cpp_flags = -std=c++17 -pthread -Wall -Wpedantic -Wextra -Werror=shadow

objects= main.o parallel.o
all: cpp_flags += -O3 -DNDEBUG
all: $(objects)	
	$(compiler) $(cpp_flags) $(objects) -o main 

main.o: main.cpp 
	$(compiler) $(cpp_flags) main.cpp -c 

parallel.o: parallel.cpp parallel.hpp thread_pool.hpp optimizer.hpp 
	$(compiler) $(cpp_flags) parallel.cpp -c 

clean: 
	rm -f *.o

