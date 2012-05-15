#include "FrequencyFilter.hpp"
#include <fftw3.h>
#include <stdlib.h>

namespace isis
{
namespace filter
{

FrequencyFilter::FrequencyFilter()
{
}


bool FrequencyFilter::process ( data::Image &image )
{

	data::TypedImage<short> tImage( image );

	const util::ivector4 size = image.getSizeAsVector();
	uint16_t repTime;

	if( parameters["repetitionTime"].isEmpty() ) {
		if( image.hasProperty( "repetitionTime" ) ) {
			repTime = image.getPropertyAs<uint16_t>( "repetitionTime" );
		} else {
			repTime = 1000;
		}
	} else {
		repTime = parameters["repetitionTime"];
	}

	repTime /= 1000.0;

	const ValueType high = parameters["high"].as<ValueType>();
	const ValueType low = parameters["low"].as<ValueType>();
	const bool stop = parameters["stop"].as<bool>();
	const ValueType sharp = parameters["sharp"].as<ValueType>();

	int32_t n = size[3];

	int tail = n / 10;

	if( tail < 50 ) tail = 50;

	if( tail >= n / 2 ) tail = n / 2 - 1;

	if( tail >= n - 1 ) tail = n - 2;

	if( tail < 0 ) tail = 0;

	n += 2 * tail;

	const int nc = ( n / 2 ) + 1;

	double *in = ( double * ) fftw_malloc( sizeof( double ) * n );
	fftw_complex *out = ( fftw_complex * )fftw_malloc( sizeof( fftw_complex ) * nc );

	for( uint16_t i = 0; i < n; i++ ) {
		in[i] = 0;
	}

	fftw_plan p1 = fftw_plan_dft_r2c_1d( n, in, out, FFTW_ESTIMATE );
	fftw_plan p2 = fftw_plan_dft_c2r_1d( n, out, in, FFTW_ESTIMATE );

	float alpha = repTime * n;
	double *highp;
	double *lowp;

	if( sharp > 0 ) {
		highp = ( double * )malloc( sizeof( double ) * nc );
		lowp = ( double * )malloc( sizeof( double ) * nc );

		for( uint16_t i = 1; i < nc; i++ ) {
			highp[i] = 1.0 / ( 1.0 + exp( ( alpha / high - ( double )i ) * sharp ) );
			lowp[i] = 1.0 / ( 1.0 + exp( ( ( double )i - alpha / low ) * sharp ) );
		}
	}

	double sum;
	int l;
	double x_high;
	double x_low;
	double x;
	double freq;

	for( uint16_t s = 0; s < size[2]; s++ ) {
		for( uint16_t c = 0; c < size[1]; c++ ) {
			for( uint16_t r = 0; r < size[0]; r++ ) {
				sum = 0;
				l = 0;

				for( uint16_t j = 0; j < tail; j++ ) {
					in[l] =  ( double )tImage.voxel<short>( r, c, s, tail - j );
					sum += in[l];
					l++;
				}

				for( uint16_t j = 0; j < size[3]; j++ ) {
					in[l] = ( double )tImage.voxel<short>( r, c, s, j );
					sum += in[l];
					l++;
				}

				uint16_t j = size[3] - 2;

				while( l < n && j - 1 >= 0 ) {
					in[l] = ( double )tImage.voxel<short>( r, c, s, j );
					sum += in[l];
					j--;
					l++;
				}

				if( std::abs( sum ) > std::numeric_limits<float>::epsilon() ) {
					fftw_execute( p1 );

					for( uint16_t i = 1; i < nc; i++ ) {
						if( sharp > 0 ) {
							if( high > 0 ) {
								x_high = highp[i];
							} else {
								x_high = 1.0;
							}

							if( low > 0 ) {
								x_low = lowp[i];
							} else {
								x_low = 1.0;
							}

							x = x_high + x_low - 1.0;

							if( stop ) {
								x = std::abs( 1 - x );
							}

							out[i][0] *= x;
							out[i][1] *= x;
						} else {
							freq = 1.0 / ( double )i * alpha;

							if( ( !stop && ( freq < low || ( freq > high && high > 0 ) ) )
								|| ( stop && !( freq < low || ( freq > high && high > 0 ) ) ) ) {
								out[i][0] = out[i][1] = 0;
							}
						}
					}

					fftw_execute( p2 );

					for( uint16_t i = 0; i < n; i++ ) {
						in[i] /= ( double )n;
					}

					for( uint16_t j = tail; j < n - tail; j++ ) {
						tImage.voxel<short>( r, c, s, j - tail ) = static_cast<short>( in[j] );
					}
				}
			}
		}
	}

	image = tImage;
	return true;

}

}
}