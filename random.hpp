#ifndef __RANDOM_HPP
#define __RANDOM_HPP

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>

//#include "math/blas.hpp"

namespace internal 
{
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

        float uniform_float()
        {
            float_left = !float_left;

            if ( !float_left )
            {
                latest_sample = operator()();
                return ( static_cast<float>( static_cast< uint32_t >( latest_sample ) ) 
                        * ( 1.0f / static_cast<float>( std::numeric_limits< uint32_t >::max() ) ) );
            }
            else
                return ( static_cast<float>( latest_sample >> 0x20 ) 
                        * ( 1.0f / static_cast<float>( std::numeric_limits< uint32_t >::max() ) ) ); 
        }
        
        double uniform_double() 
        {
            return ( static_cast<double>( operator()() ) * ( 1.0 / static_cast<double>( std::numeric_limits< uint64_t >::max() ) ) );
        }

        unsigned int uniform_int( int min , int max )
        {
            return ( static_cast<unsigned int>( operator()() / 2 ) % ( max - min + 1 ) + min );
        }
       
        double gaussian_s()
        {
            double a1 = 3.949846138,
                   a3 = 0.252408784,
                   a5 = 0.076542912,
                   a7 = 0.008355968,
                   a9 = 0.029899776,
                   sum = 0.0;

            for( int i = 0 ; i < 12 ; ++i )
                sum += uniform_float();

            double r = (sum-6.)/4.;
            double r2=r*r;
            return (((( a9*r2 + a7 )*r2 + a5 )*r2 + a3 )*r2 + a1 )*r ;
        }
        
        float gaussianf_s()
        {
            float a1 = 3.949846138f,
                  a3 = 0.252408784f,
                  a5 = 0.076542912f,
                  a7 = 0.008355968f,
                  a9 = 0.029899776f,
                  sum = 0.0f;

            for( int i = 0 ; i < 12 ; ++i )
                sum += uniform_float();

            float r  = ( sum - 6.0f ) * 0.25f;
            float r2 = r * r;
            return (((( a9*r2 + a7 )*r2 + a5 )*r2 + a3 )*r2 + a1 )*r ;
        }
        
        double gaussian()
        {
            generate_d = !generate_d;

            if ( !generate_d )
               return z1_d;

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
        
        float gaussianf()
        {
            generate_f = !generate_f;

            if ( !generate_f )
               return z1_f;

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
   
        #ifdef __MATH_BLAS_HPP
        math::double_vektor double_unit_vektor()
        {
            double rsq = 2, rd1, rd2;
            
            do
            {
                rd1 = 1.0 - 2.0 * uniform_double();
                rd2 = 1.0 - 2.0 * uniform_double();
                rsq = rd1 * rd1 + rd2 * rd2;
            }
            while ( rsq > 1.0 );

            double rdh = 2.0 * sqrt( 1.0 - rsq );
            return { rd1 * rdh, rd2 * rdh, 1.0 - 2.0 * rsq };
        }

        math::float_vektor float_unit_vektor()
        {
            float rsq = 2.0, rd1, rd2;
            
            do
            {
                rd1 = 1.0f - 2.0f * uniform_float();
                rd2 = 1.0f - 2.0f * uniform_float();
                rsq = rd1 * rd1 + rd2 * rd2;
            }
            while ( rsq > 1.0f );

            float rdh = 2.0f * std::sqrt( 1.0f - rsq );
            return { rd1 * rdh, rd2 * rdh, ( 1.0f - 2.0f * rsq ) };
        }

        math::float_vektor float_unit_vektor_xy_plane()
        {
            float x = uniform_float();
            float y = std::sqrt( 1.0f - x*x ),
                  z = 0;
            return { x, y, z };
        }
        #endif
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
        std::ifstream urandom( "/dev/urandom" );

        if ( !urandom )
            std::cerr << "error: /dev/urandom ?" << std::endl;

        urandom.read( (char*) &s , 2 * sizeof(uint64_t) );

        return;

        s[ 0 ] = std::hash< uint64_t >()( ( s[ 1 ] ^ std::hash< std::thread::id >()( std::this_thread::get_id() ) ) | reinterpret_cast< uint64_t >( this ) );
        s[ 1 ] = std::hash< uint64_t >()( ( s[ 0 ] | std::hash< std::thread::id >()( std::this_thread::get_id() ) ) ^ reinterpret_cast< uint64_t >( this+0xa0864abc ) );
    }

    uint64_t operator()() final override 
    {
        uint64_t &s1 = s[ 1 ],
                 &s0 = s[ 0 ];

        s1 ^= s0;
        s0  = internal::rotl( s0, 55 ) ^ s1 ^ ( s1 << 14 ); // a,b 
        s1  = internal::rotl( s1, 36 ); //c 

        return s0 + s1;
    }
    
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
