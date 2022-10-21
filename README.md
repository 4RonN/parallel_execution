# parallel_execution
Parallel and asynchronous C++ execution based on std::thread, using thread pooling, self optimization and unique random number generators.
The compilation requires C++-17 via the -std=c++17 flag. For example, we may fill an std::vector with random numbers in parallel:
```
std::vector< double > random( 100'000 );

auto generate_number = [] ( auto& element )
            {   
                thread_local auto& generator = parallel::get_generator(); 
                element = generator.gaussian();
            };
    
parallel::for_each( random.begin(), random.end(), generate_number );
```
See main.cpp for an example use case, the creation of random numbers in parallel. 
In the current settings, the library tests the number of threads used in each parallel function call, searching for the optimum.
The chosen number of threads is reported on the std::out. 
To change this behavior, edit lines 56 and 57 in parallel.hpp
 
