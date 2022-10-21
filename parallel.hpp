/** 
 *  @file parallel.hpp
 *      This is a extension to the c++17 parallel library enabling simple to use 
 *      parallel similar to openMP but with more flexible locking,
 *      runtime self optimization and future results... 
 */

#ifndef __PARALLEL_HPP
#define __PARALLEL_HPP

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <future>
#include <functional>
#include <mutex>
#include <memory>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "thread_pool.hpp"
#include "optimizer.hpp"

/**
 *  @brief scope_guard automatical executes function given when 
 *          it goes out of scope          
 */
template< typename F >
struct scope_guard
{
    F task;
    scope_guard( F&& f ) : task( f ) {}
    scope_guard( scope_guard&& ) = default;
    ~scope_guard() { task(); } 
};

/**
 *  @brief factory for scope_guard
 */
template< typename F >
scope_guard< F > on_scope_exit( F&& f ) { return std::forward< F >( f ); }

/**
 *  @brief main namespace of this library
 */
namespace parallel
{
    /**
     *  @brief internal objects for the thread management, not for external use 
     */
    namespace intern
    {
        extern thread_pool global_pool;
        extern std::unordered_map< std::type_index, optimizer > optimizers;
    }
    
    /**
     *  @brief get optimizer to a parallel function call
     */
    template< typename F >
    optimizer& get_optimizer( F const& )
    {
        return intern::optimizers[ typeid( F ) ];
    } 
   
    /**
     *  @brief change the number of threads in the global pool
     */ 
    inline void set_concurrency( size_t new_concurrency )
    {
        parallel::concurrency = new_concurrency;
        intern::global_pool.resize( new_concurrency - 1 ); 
        intern::random.resize( new_concurrency );
    }

    /**
     *  @brief for each parallel iteration using the global thread pool, blocking behavior
     *         these namespace scoped functions provide runtime optimization.
     */
    template< typename InputIterator, typename F >
    void for_each( InputIterator first, InputIterator last, F&& f )
    {
        if ( first == last ) 
            return;

        // ------------------------------------------------------------------- //
        
        static optimizer& local_optimizer = get_optimizer( f ); // static variable to store execution statistics
        
        auto   remember = on_scope_exit( local_optimizer.register_duration() ); // scope guard for runtime logging
        size_t local_concurrency = local_optimizer.get_concurrency();

        // ------------------------------------------------------------------- //

        typename std::conditional< std::is_rvalue_reference< decltype( f ) >::value, std::shared_ptr< F >, typename std::remove_reference< F >::type * >::type task;

        if constexpr( std::is_rvalue_reference< decltype( f ) >::value )
            task = std::make_shared< F >( std::forward< F >( f ) );
        else 
            task = &f; 

        if ( local_concurrency == 1 )
        {
            for ( ; first != last; ++first )
               ( *task )( *first );
            return; 
        }

        size_t size       = last - first; 
        size_t block_size = size / local_concurrency,
               remainder  = size % local_concurrency;
        
        InputIterator block_begin,
                      block_end = first;

        for ( size_t i = 0; i < local_concurrency; ++i )
        {
            block_begin = block_end; 
            block_end   = block_begin + ( block_size + ( remainder > i ) );

            intern::global_pool.async( [ task, block_begin, block_end ] 
            {
                InputIterator it = block_begin; 
                for ( ; it != block_end; ++it )
                    ( *task )( *it );
            });
        }
        intern::global_pool.synchronize();
    }
    
    /**
     *  @brief for each parallel iteration using the global thread pool, blocking behavior
     */ 
    template< typename T, typename F > 
    void for_each( T&& t, F&& f )
    {
        parallel::for_each( t.begin(), t.end(), std::forward< F >( f ) );
        intern::global_pool.synchronize();
    }

    /**
     *  @brief for each parallel iteration using the global thread pool, nonblocking behavior
     */ 
    template< typename InputIterator, typename F >
    void for_each( InputIterator first, InputIterator last, F&& f, size_t leave_slots )
    {
        if ( first == last ) 
            return; 

        // ------------------------------------------------------------------- //
        
        static optimizer& local_optimizer = get_optimizer( f ); // static variable to store execution statistics
        
        auto   remember = on_scope_exit( local_optimizer.register_duration() ); // scope guard for runtime logging
        size_t local_concurrency = local_optimizer.get_concurrency();

        // ------------------------------------------------------------------- //
        
        typename std::conditional< std::is_rvalue_reference< decltype( f ) >::value, std::shared_ptr< F >, typename std::remove_reference< F >::type * >::type task;

        if constexpr( std::is_rvalue_reference< decltype( f ) >::value )
            task = std::make_shared< F >( std::forward< F >( f ) );
        else 
            task = &f; 

        if ( local_concurrency == 1 or leave_slots >= local_concurrency )
        {
            for ( ; first != last; ++first )
               ( *task )( *first ); 
            return; 
        }

        size_t size       = last - first; 
        size_t block_size = size / ( local_concurrency - leave_slots ),
               remainder  = size % ( local_concurrency - leave_slots );

        InputIterator block_begin,
                      block_end = first;
        
        for ( size_t i = 0; i < ( local_concurrency - leave_slots ); ++i )
        {
            block_begin = block_end; 
            block_end   = block_begin + ( block_size + ( remainder > i ) );

            intern::global_pool.async( [ task, block_begin, block_end ] 
            {
                InputIterator it = block_begin; 
                for ( ; it != block_end; ++it )
                    ( *task )( *it );
            });
        }
    }
    
    /**
     *  @brief for each parallel iteration using the global thread pool, nonblocking behavior
     */ 
    template< typename T, typename F > 
    void for_each( T&& t, F&& f, size_t leave_slots )
    {
        for_each( t.begin(), t.end(), std::forward< F >( f ), leave_slots );
    }

    /**
     *  @brief for each parallel iteration using the global thread pool, blocking behavior
     */ 
    template< typename F >
    void for_each( size_t first, size_t last, F&& f )
    {
        if ( first == last ) 
            return; 

        // ------------------------------------------------------------------- //
        
        static optimizer& local_optimizer = get_optimizer( f ); // static variable to store execution statistics
        
        auto   remember = on_scope_exit( local_optimizer.register_duration() ); // scope guard for runtime logging
        size_t local_concurrency = local_optimizer.get_concurrency();

        // ------------------------------------------------------------------- //

        typename std::conditional< std::is_rvalue_reference< decltype( f ) >::value, std::shared_ptr< F >, typename std::remove_reference< F >::type * >::type task;

        if constexpr( std::is_rvalue_reference< decltype( f ) >::value )
            task = std::make_shared< F >( std::forward< F >( f ) );
        else 
            task = &f; 

        if ( local_concurrency == 1 )
        {
            for ( ; first != last; ++first )
               ( *task )( first ); 
            return; 
        }

        size_t size       = last - first; 
        size_t block_size = size / local_concurrency,
               remainder  = size % local_concurrency;
        
        size_t block_begin,
               block_end = first;
        
        for ( size_t i = 0; i < local_concurrency; ++i )
        {
            block_begin = block_end; 
            block_end   = block_begin + ( block_size + ( remainder > i ) );

            intern::global_pool.async( [ task, block_begin, block_end ] 
            {
                size_t idx = block_begin; 
                for ( ; idx != block_end; ++idx )
                    ( *task )( idx );
            });
        }
        intern::global_pool.synchronize();
    }
    
    /**
     *  @brief assign work to the global thread pool
     */ 
    template< typename F, typename ...Ts >
    decltype(auto) async( F&& f, Ts&&... ts )
    {
        return intern::global_pool.async( std::forward< F >( f ), std::forward< Ts >( ts )... ); 
    }
   
    /**
     *  @brief blocks until all work assigned to the global pool is done
     */ 
    inline void synchronize() 
    { 
        intern::global_pool.synchronize(); 
    }
}

#endif // __PARALLEL_HPP
