#ifndef __RANDOM_HPP
#define __RANDOM_HPP

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>

namespace internal 
{
    /**
     *  @brief Common base class for pseudo random number generators. 
     *         Inheriting classes only need to provide a random uint64_t,
     *         besides an initializiation (constructor) and jump() function. 
     */
    class generator_commons
    {
        bool     float_left = true;
        uint64_t latest_sample;
        
        float  z1_f;
        double z1_d;
        bool generate_f = {},
             generate_d = {};

        public:

        virtual uint64_t operator()() = 0;
        virtual void     jump()       = 0;

        // seeding functions depend on the size of the internal state.. thus no virtual function for this. 

        /**
         *  @brief Generate a random float following a uniform distribution in the range [0,1)
         */
        float uniform_float()
        {
            float_left = !float_left; // use all bits of the random uint64_t:

            if ( !float_left )
            {
                latest_sample = operator()();
                return ( static_cast<float>( static_cast< uint32_t >( latest_sample ) ) 
                        * ( 1.0f / static_cast<float>( std::numeric_limits< uint32_t >::max() ) ) ); // use least significant 32 bits
            }
            else
                return ( static_cast<float>( latest_sample >> 0x20 ) 
                        * ( 1.0f / static_cast<float>( std::numeric_limits< uint32_t >::max() ) ) ); // use most significant 32 bits
        }
        
        /**
         *  @brief Generate a random double following a uniform distribution in the range [0,1)
         */
        double uniform_double() 
        {
            return ( static_cast<double>( operator()() ) * ( 1.0 / static_cast<double>( std::numeric_limits< uint64_t >::max() ) ) );
        }

        unsigned int uniform_int( int min , int max )
        {
            return ( static_cast<unsigned int>( operator()() / 2 ) % ( max - min + 1 ) + min );
        }
        
        /**
         *  @brief Generate a random double following a gaussian distribution.
         */
        double gaussian()
        {
            generate_d = !generate_d;

            if ( !generate_d ) // in every second call, return the 2nd random gaussian from the Bux-Muller method.
               return z1_d;

            // using Bux-Muller:
            double u1, u2 = uniform_float(), z0;

            do 
            {
                u1 = uniform_float();
            } 
            while ( u1 <= std::numeric_limits< double >::min() );  

            z0   = std::sqrt( -2.0 * std::log( u1 ) ) * std::cos( 2.0 * M_PI * u2 );
            z1_d = std::sqrt( -2.0 * std::log( u1 ) ) * std::sin( 2.0 * M_PI * u2 );

            return z0;
        }
        
        /**
         *  @brief Generate a random float following a gaussian distribution.
         */
        float gaussianf()
        {
            generate_f = !generate_f;

            if ( !generate_f )
               return z1_f;

            // using Bux-Muller:
            float u1, u2 = uniform_float(), z0;

            do 
            {
                u1 = uniform_float();
            } 
            while ( u1 <= std::numeric_limits< float >::min() ); 

            z0   = std::sqrt( -2.0f * std::log( u1 ) ) * std::cos( float( 2.0 * M_PI ) * u2 );
            z1_f = std::sqrt( -2.0f * std::log( u1 ) ) * std::sin( float( 2.0 * M_PI ) * u2 );

            return z0;
        }
    };
    
    static constexpr uint64_t rotl( uint64_t const& x, int const& k )
    {
        return ( x << k ) | ( x >> ( 64 -k ));
    }
}

/**
 *  @brief A really fast random number generator with a 128 bit internal state.
 */
class xoroshiro128plus : public internal::generator_commons
{
    uint64_t s[ 2 ]; 

public:

    xoroshiro128plus() 
    {
        std::ifstream urandom( "/dev/urandom" ); // initialize with unix random device 

        if ( !urandom )
            std::cerr << "error: /dev/urandom ?" << std::endl;

        urandom.read( (char*) &s , 2 * sizeof(uint64_t) );

        return;
    }

    /**
     *  @brief Manually seed the interal state.
     */
    void seed( uint64_t s1, uint64_t s2 )
    {
        s[0] = s1;
        s[1] = s2; 
    }

    /**
     *  @brief generate new random uint64_t  
     */
    uint64_t operator()() final override 
    {
        uint64_t &s1 = s[ 1 ],
                 &s0 = s[ 0 ];

        s1 ^= s0;
        s0  = internal::rotl( s0, 55 ) ^ s1 ^ ( s1 << 14 ); // a,b 
        s1  = internal::rotl( s1, 36 ); //c 

        return s0 + s1;
    }
    
    /**
     *  @brief jump forward in the sequence.
     */
    void jump() final override
    {
        const uint64_t JUMP[] = { 0xbeac0467eba5facb, 0xd86b048b86aa9922 };

        uint64_t s0 = 0,
                 s1 = 0;
        for( size_t i = 0; i < sizeof( JUMP )/ sizeof( *JUMP ); ++i )
            for(int b = 0; b < 64; b++) 
            {
                if (JUMP[i] & 1ULL << b) 
                {
                    s0 ^= s[0];
                    s1 ^= s[1];
                }
                (*this)();
            }

        s[0] = s0;
        s[1] = s1;
    }
};

#endif // __RANDOM_HPP
