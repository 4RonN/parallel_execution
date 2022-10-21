#ifndef __THREAD_POOL_HPP
#define __THREAD_POOL_HPP

#include <condition_variable>
#include <future>
#include <functional>
#include <mutex>
#include <memory>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include "random.hpp"

namespace parallel
{
    using generator = xoroshiro128plus;
        
    namespace intern
    {
        extern std::vector< std::thread::id > identifier;
        extern std::mutex                     id_mutex;
        extern std::vector< generator >       random;
    }

    /**
    *  @brief A thread pool with standart capabilities.
    *         The %thread_pool also work as standalone. 
    *         However, we define parallel call fuctions on its basis later. 
    */
    class thread_pool 
    {
        std::vector< std::thread >           workers     {};
        std::queue< std::function< void() >> task_queue  {};
        std::vector< std::mutex >            busy        {};
        std::mutex                           queue_mutex {}; 
        std::condition_variable              condition   {}; 
        bool                                 stop        { false };

        void thread_loop( size_t thread_idx ) // careful with lambda catures here, use this, certain objects may leave scope
        {
            // initialize: 
            {
                std::lock_guard< std::mutex > lock( intern::id_mutex );
                intern::identifier.push_back( std::this_thread::get_id() ); // list thread_id

                if ( intern::identifier.size() >= intern::random.size() ) // create random number generator for thread 
                    intern::random.emplace_back();
            }

            for ( ;; )
            {
                std::function<void()> task;

                std::unique_lock< std::mutex > on_duty( busy[ thread_idx ], std::defer_lock ); 
                {
                    std::unique_lock< std::mutex > lock( queue_mutex );
                    condition.wait( lock, [ this ]{ return stop || !task_queue.empty(); });

                    if( task_queue.empty() && stop )
                        return;

                    task = std::move( task_queue.front() );
                    task_queue.pop();
                    
                    on_duty.lock();
                }
                task(); 
            }
           
            // on exit:   
            {
                std::lock_guard< std::mutex > lock( intern::id_mutex );
                intern::identifier.erase( std::find( intern::identifier.begin(), intern::identifier.end(), std::this_thread::get_id() ) );
            }
        }

        public:
        
        /**
         * @brief creates a %thread_pool without threads.
         */
        thread_pool() = default; 

        /**
         * @brief creates a %thread_pool with some number of threads.
         * @param threads Number of threads to create.
         */
        thread_pool( size_t threads ) : workers( threads ), 
                                        busy( threads ), 
                                        stop( false ) 
        {
            for( size_t thread_idx = 0; thread_idx < workers.size(); ++thread_idx )
                workers[ thread_idx ] = std::thread( &thread_pool::thread_loop, this, thread_idx );
        }

        /**
         *  @brief wouldn't make sence...
         */
        thread_pool( thread_pool const& ) = delete;

        /**
         *  @brief a nightmare implementing synchronization and mutex move...
         */
        thread_pool( thread_pool && )     = delete;

        /**
         *  @brief destroys the %thread_pool after finishing all task_queue in its queue,
         *       stopping all threads.
         */
        ~thread_pool()
        {
            {
                std::unique_lock<std::mutex> lock( queue_mutex );
                stop = true;
                condition.notify_all(); // has to be inside the lock region !
            }

            for( auto& worker: workers )
                if ( worker.joinable() ) 
                    worker.join();
        }

        size_t size() const { return workers.size(); };

        /**
         *  @brief use more or less threads. calls a sync, also when size is not changing.
         *  @param threads number of threads to use. 
         */
        void resize( size_t threads )
        {
            synchronize(); // just to make the behaviour consistent in all cases.

            if ( threads != workers.size() )
            {
                busy = std::vector< std::mutex >( threads );

                if( threads > workers.size() )
                {
                    for ( size_t idx = workers.size(); idx < threads; ++idx )
                       workers.emplace_back( &thread_pool::thread_loop, this, idx );
                }
                else
                {
                    {
                        std::lock_guard<std::mutex> lock( queue_mutex );
                        stop = true;
                        condition.notify_all(); 
                    }

                    for( std::thread& worker: workers )
                        worker.join();

                    stop = false;

                    workers.resize( threads );

                    for ( size_t idx = 0; idx < workers.size(); ++idx )
                       workers[ idx ] = std::thread( &thread_pool::thread_loop, this, idx );
                }
            }
        }

        /**
         *  @brief enqueue a new task with arguments potentionally returning the result of f encapsulated in a std::future object.
         *  @param f a callable object to be added to the queue.
         *  @param ts arguments to bind to the tast represented by f.
         */             
        template< typename F, typename ...Ts >
        decltype(auto) async( F&& f, Ts&&... ts ) 
        {
            using return_type = typename std::result_of< F( Ts...) >::type;
            
            // if return type is not void, construct a std::packaged_task, which returns a future 
            auto task = std::make_shared< typename std::conditional< std::is_same< return_type, void >::value, std::function< void() >, std::packaged_task< return_type() >>::type >(
                    [ closure = std::forward< F >( f ), args = std::make_tuple( std::forward< Ts >( ts )... ) ] () -> decltype(auto) { return apply( closure, args ); } );

            {
                std::lock_guard< std::mutex > lock( queue_mutex );

                if( stop )
                    throw std::runtime_error( "enqueue on stopped thread_pool" );

                task_queue.emplace( [ task ]{ ( *task )(); } );

                condition.notify_one();
            }
            
            if constexpr( not std::is_same< return_type, void >::value )
                return task -> get_future();
        }

        /**
         *  @brief for each iteration inside the thread pool without synchronization
         */
        template< typename InputIterator, typename F >
        void async_for_each( InputIterator first, InputIterator last, F&& f )
        {
            if ( first == last ) 
                return;

            typename std::conditional< std::is_rvalue_reference< decltype( f ) >::value, std::shared_ptr< F >, typename std::remove_reference< F >::type * >::type task;

            if constexpr( std::is_rvalue_reference< decltype( f ) >::value )
                task = std::make_shared< F >( std::forward< F >( f ) );
            else 
                task = &f; 

            size_t size       = last - first; 
            size_t block_size = size / workers.size(),
                   remainder  = size % workers.size();
            
            InputIterator block_begin,
                          block_end = first;

            for ( size_t i = 0; i < workers.size(); ++i )
            {
                block_begin = block_end; 
                block_end   = block_begin + ( block_size + ( remainder > i ) );

                async( [ task, block_begin, block_end ] 
                {
                    InputIterator it = block_begin; 
                    for ( ; it != block_end; ++it )
                        ( *task )( *it );
                });
            }
        }
        
        /**
         *  @brief for each iteration inside the thread pool without synchronization
         */
        template< typename T, typename F > 
        void async_for_each( T&& t, F&& f )
        {
            async_for_each( t.begin(), t.end(), std::forward< F >( f ) );
        }

        /**
         *  @brief for each iteration inside the thread pool without synchronization
         *  @param leave_slots do not use all threads, but leave some for other tasks. 
         */
        template< typename InputIterator, typename F >
        void async_for_each( InputIterator first, InputIterator last, F&& f, size_t leave_slots )
        {
            if ( first == last ) 
                return; 
            
            typename std::conditional< std::is_rvalue_reference< decltype( f ) >::value, std::shared_ptr< F >, typename std::remove_reference< F >::type * >::type task;

            if constexpr( std::is_rvalue_reference< decltype( f ) >::value )
                task = std::make_shared< F >( std::forward< F >( f ) );
            else 
                task = &f; 

            size_t size       = last - first; 
            size_t block_size = size / ( workers.size() - leave_slots ),
                   remainder  = size % ( workers.size() - leave_slots );

            InputIterator block_begin,
                          block_end = first;
            
            for ( size_t i = 0; i < ( workers.size() - leave_slots ); ++i )
            {
                block_begin = block_end; 
                block_end   = block_begin + ( block_size + ( remainder > i ) );

                async( [ task, block_begin, block_end ] 
                {
                    InputIterator it = block_begin; 
                    for ( ; it != block_end; ++it )
                        ( *task )( *it );
                });
            }
        }
        
        /**
         *  @brief for each iteration inside the thread pool without synchronization
         *  @param leave_slots do not use all threads, but leave some for other tasks. 
         */
        template< typename T, typename F > 
        void async_for_each( T&& t, F&& f, size_t leave_slots )
        {
            for_each( t.begin(), t.end(), std::forward< F >( f ), leave_slots );
        }

        /**
         *  @brief for each iteration inside the thread pool without synchronization.
         *         Iteration based on index range.
         */
        template< typename F >
        void async_for_each( size_t first, size_t last, F&& f )
        {
            if ( first == last ) 
                return; 

            typename std::conditional< std::is_rvalue_reference< decltype( f ) >::value, std::shared_ptr< F >, typename std::remove_reference< F >::type * >::type task;

            if constexpr( std::is_rvalue_reference< decltype( f ) >::value )
                task = std::make_shared< F >( std::forward< F >( f ) );
            else 
                task = &f; 

            size_t size       = last - first; 
            size_t block_size = size / workers.size(),
                   remainder  = size % workers.size();
            
            size_t block_begin,
                   block_end = first;
            
            for ( size_t i = 0; i < workers.size(); ++i )
            {
                block_begin = block_end; 
                block_end   = block_begin + ( block_size + ( remainder > i ) );

                async( [ task, block_begin, block_end ] 
                {
                    size_t idx = block_begin; 
                    for ( ; idx != block_end; ++idx )
                        ( *task )( idx );
                });
            }
        }

        /**
         *  @brief with the global thread, join the working group of threads until all task_queue are processed 
         *          resulting in a synchronization. 
         */             
        void synchronize() 
        {
            for (;;)
            {
                std::function< void() > task;
                bool                    done = false;
                
                {
                    std::lock_guard< std::mutex > lock( queue_mutex );

                    done = task_queue.empty();

                    if ( not done ) 
                    {
                        task = std::move( task_queue.front() );
                        task_queue.pop();
                    }
                } // unlock queue

                if ( done ) 
                {
                    for( size_t i = 0; i < workers.size(); ++i ) 
                        std::lock_guard< std::mutex > sync( busy[ i ] ); // join all workers
                    
                    break;
                }
                else
                    task();
            }
        }
    };

    /**
     *  @brief function to access thread index 
     */
    inline size_t get_tid()
    {
        std::lock_guard< std::mutex > lock( intern::id_mutex );

        return std::find( intern::identifier.cbegin(), intern::identifier.cend(), std::this_thread::get_id() ) 
                - intern::identifier.cbegin();
    }

    /**
     *  @brief function to access the per-thread pseudo random number generator.
     */
    inline generator& get_generator() 
    {
        thread_local size_t tid = get_tid();
        std::lock_guard< std::mutex > lock( intern::id_mutex );

        return intern::random[ tid ];
    }

} // namespace parallel

#endif // __THREAD_POOL_HPP
