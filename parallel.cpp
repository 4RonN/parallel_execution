
#include "parallel.hpp"

namespace parallel
{
    using generator = xoroshiro128plus;
        
    size_t concurrency = std::thread::hardware_concurrency(); 

    /**
     *  @brief internal objects for the thread management, not for external use 
     */
    namespace intern
    {
        std::vector< std::thread::id > identifier ( 1, std::this_thread::get_id() );
        std::mutex                     id_mutex = {}; 
        std::vector< generator >       random     ( concurrency );

        thread_pool global_pool( concurrency - 1 );

        std::unordered_map< std::type_index, optimizer > optimizers;
    }

} // namespace parallel
