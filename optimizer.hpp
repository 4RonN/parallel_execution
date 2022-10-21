#ifndef __OPTIMIZER_HPP  
#define __OPTIMIZER_HPP  

#include <chrono>
#include <iostream>

#include "stacktrace.hpp"

namespace parallel 
{
    extern size_t concurrency;
    
    bool constexpr runtime_optimization      = true;
    bool constexpr test_runtime_optimization = true;
    
    /**
     *  @brief Helper object to measure runtime of parallel function execution for different numbers of threads.
     *         Depending on the minimum runtime, the optimal number of thread is chosen.
     *         The distinction of functions is acomplished via the C++ template instanciation.
     *         Optimization is performed every 1 million executions. 
     */
    struct optimizer
    {
        std::vector< long long > runtimes      = {};
        std::vector< size_t >    concurrencies = {};
        size_t                   counter       = 0,
                                 measuring     = 0,
                                 samples       = 0,
                                 concurrency   = parallel::concurrency;
        size_t static constexpr  re_optimize   = 1'000'000;
       
        optimizer()
        {  
            if ( test_runtime_optimization )
                concurrencies.push_back( 1 );

            if ( std::thread::hardware_concurrency() / 2 > 1 )
                concurrencies.push_back( std::thread::hardware_concurrency() / 2 );

            concurrencies.push_back( std::thread::hardware_concurrency() );
            concurrencies.push_back( std::thread::hardware_concurrency() * 2 );
            
            runtimes.assign( concurrencies.size(), 0 );
        }
        
        size_t get_concurrency()
        {
            if ( not parallel::runtime_optimization )
                return parallel::concurrency;

            if ( ( counter % re_optimize ) == 0 ) 
            {
                // measuring is set to 0
                // the scope guard increases sample and measuring counters 
                
                if ( measuring == concurrencies.size() ) // begin / end  
                {
                    if ( samples < 2 ) // all cuncurrencies measure once. need more samples: 
                    {
                        samples   += 1;
                        measuring  = 0; // start cycle again
                    }
                    else // measurement done for all concurrencies:
                    {
                        concurrency = concurrencies[ std::min_element( runtimes.begin(), runtimes.end() ) - runtimes.begin() ]; // choose optimum
                        runtimes.assign( concurrencies.size(), 0 ); // reset measurement
                        measuring = 0; // reset for next run
                        samples   = 0;  
                        counter  += 1; // stops optimization  

                        if constexpr( test_runtime_optimization ) // print optimizition output
                        {
                            if ( concurrency == 1 )
                            {
                                std::cout << "function gains no multithreading speedup, " << std::flush;
                                print_stacktrace();
                            }
                            else
                            {
                                std::cout << "using " << concurrency << " threads for " << std::flush;
                                print_stacktrace();
                            }
                        }

                        return concurrency; 
                    }
                }
                return concurrencies[ measuring ]; // return currently measured concurrency
            }
            else
            {
                counter += 1;
                return concurrency; // measured optimum
            }
        }

        /**
         *  @brief reset optimization status, this retriggers the optimization on the next call.
         */
        void reset()
        {
            runtimes.assign( concurrencies.size(), 0 ); // reset measurement
            measuring = 0; // reset 
            samples   = 0;
            counter   = 0; 
        }

        /**
         *  @brief cunstruct a helper function for a scope guard that logs the execution time at scope exit of a function.
         */
        std::function< void() > register_duration()
        {
            if ( ( counter % re_optimize ) == 0 ) 
                return [ &, time = std::chrono::system_clock::now() ]{ runtimes[ measuring ] += ( std::chrono::system_clock::now() - time ).count(); measuring++; };
            else
                return []{}; 
        }    
    };

}

#endif // __OPTIMIZER_HPP  
