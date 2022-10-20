#include <iostream>

#include "parallel.hpp"

int main()
{
    // print number in the range [1,9] in parallel
    parallel::for_each( 1ul, 10ul, [] ( int i ) { std::cout << i << std::endl; } ); 

    // fill vector with random numbers 
    std::vector< double > random( 100 );

    auto generate_number = [] ( auto& element )
                {
                    thread_local auto& generator = parallel::get_generator(); // use reference so that internal state of the generators changes!
                    element = generator.gaussian();
                };

    // for repeated calls, the self-optimization takes place:
    for ( int i = 0; i < 100; ++i ) 
        parallel::for_each( random.begin(), random.end(), generate_number ); // this call self optimizies the number of threads used
                                                                             // 100 random numbers => no parallel speedup

    for ( size_t i = 0; i < random.size() / 10; ++i )
       std::cout << random[ i ] << std::endl;
    
    random.resize( 100'000 );  
    
    for ( int i = 0; i < 100; ++i )
        parallel::for_each( random.begin(), random.end(), generate_number );
    
    // 100'000 random numbers => this will gain parallel speedup, but as now, no re-optimization is triggered
    
    return 0;
}
