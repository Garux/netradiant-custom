#include "noise.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Perlin noise
// ---------------------------------------------------------------------------

static const int perlin_perm_src[256] = {
	151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
	140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
	247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
	 57,177, 33, 88,237,149, 56, 87,174, 20,125,136,171,168, 68,175,
	 74,165, 71,134,139, 48, 27,166, 77,146,158,231, 83,111,229,122,
	 60,211,133,230,220,105, 92, 41, 55, 46,245, 40,244,102,143, 54,
	 65, 25, 63,161,  1,216, 80, 73,209, 76,132,187,208, 89, 18,169,
	200,196,135,130,116,188,159, 86,164,100,109,198,173,186,  3, 64,
	 52,217,226,250,124,123,  5,202, 38,147,118,126,255, 82, 85,212,
	207,206, 59,227, 47, 16, 58, 17,182,189, 28, 42,223,183,170,213,
	119,248,152,  2, 44,154,163, 70,221,153,101,155,167, 43,172,  9,
	129, 22, 39,253, 19, 98,108,110, 79,113,224,232,178,185,112,104,
	218,246, 97,228,251, 34,242,193,238,210,144, 12,191,179,162,241,
	 81, 51,145,235,249, 14,239,107, 49,192,214, 31,181,199,106,157,
	184, 84,204,176,115,121, 50, 45,127,  4,150,254,138,236,205, 93,
	222,114, 67, 29, 24, 72,243,141,128,195, 78, 66,215, 61,156,180
};

static int perlin_p[512];

static bool perlin_initialised = false;

static void perlin_init(){
	if ( perlin_initialised ) return;
	for ( int i = 0; i < 256; ++i )
		perlin_p[i] = perlin_p[i + 256] = perlin_perm_src[i];
	perlin_initialised = true;
}

static double perlin_fade( double t ){
	return t * t * t * ( t * ( t * 6.0 - 15.0 ) + 10.0 );
}

static double perlin_lerp( double t, double a, double b ){
	return a + t * ( b - a );
}

static double perlin_grad( int hash, double x, double y ){
	int h = hash & 15;
	double u = h < 8 ? x : y;
	double v = h < 4 ? y : ( h == 12 || h == 14 ? x : 0.0 );
	return ( ( h & 1 ) == 0 ? u : -u ) + ( ( h & 2 ) == 0 ? v : -v );
}

double Perlin::noise( double x, double y ){
	perlin_init();
	int X = (int)std::floor( x ) & 255;
	int Y = (int)std::floor( y ) & 255;
	x -= std::floor( x );
	y -= std::floor( y );
	double u = perlin_fade( x );
	double v = perlin_fade( y );
	int A = perlin_p[X]     + Y;
	int B = perlin_p[X + 1] + Y;
	return perlin_lerp( v,
		perlin_lerp( u, perlin_grad( perlin_p[A],     x,       y       ),
		                perlin_grad( perlin_p[B],     x - 1.0, y       ) ),
		perlin_lerp( u, perlin_grad( perlin_p[A + 1], x,       y - 1.0 ),
		                perlin_grad( perlin_p[B + 1], x - 1.0, y - 1.0 ) ) );
}

// ---------------------------------------------------------------------------
// Simplex noise
// ---------------------------------------------------------------------------

static const int simplex_perm_src[256] = {
	151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
	140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
	247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
	 57,177, 33, 88,237,149, 56, 87,174, 20,125,136,171,168, 68,175,
	 74,165, 71,134,139, 48, 27,166, 77,146,158,231, 83,111,229,122,
	 60,211,133,230,220,105, 92, 41, 55, 46,245, 40,244,102,143, 54,
	 65, 25, 63,161,  1,216, 80, 73,209, 76,132,187,208, 89, 18,169,
	200,196,135,130,116,188,159, 86,164,100,109,198,173,186,  3, 64,
	 52,217,226,250,124,123,  5,202, 38,147,118,126,255, 82, 85,212,
	207,206, 59,227, 47, 16, 58, 17,182,189, 28, 42,223,183,170,213,
	119,248,152,  2, 44,154,163, 70,221,153,101,155,167, 43,172,  9,
	129, 22, 39,253, 19, 98,108,110, 79,113,224,232,178,185,112,104,
	218,246, 97,228,251, 34,242,193,238,210,144, 12,191,179,162,241,
	 81, 51,145,235,249, 14,239,107, 49,192,214, 31,181,199,106,157,
	184, 84,204,176,115,121, 50, 45,127,  4,150,254,138,236,205, 93,
	222,114, 67, 29, 24, 72,243,141,128,195, 78, 66,215, 61,156,180
};

static int simplex_perm[512];
static bool simplex_initialised = false;

static void simplex_init(){
	if ( simplex_initialised ) return;
	for ( int i = 0; i < 256; ++i )
		simplex_perm[i] = simplex_perm[i + 256] = simplex_perm_src[i];
	simplex_initialised = true;
}

static const int grad3[12][2] = {
	{ 1, 1},{ -1, 1},{ 1,-1},{ -1,-1},
	{ 1, 0},{ -1, 0},{ 1, 0},{ -1, 0},
	{ 0, 1},{  0,-1},{ 0, 1},{  0,-1}
};

static int simplex_fast_floor( double x ){
	int xi = (int)x;
	return x < xi ? xi - 1 : xi;
}

static double simplex_dot( const int g[2], double x, double y ){
	return g[0] * x + g[1] * y;
}

double Simplex::noise( double x, double y ){
	simplex_init();

	const double F2 = 0.5 * ( std::sqrt( 3.0 ) - 1.0 );
	const double G2 = ( 3.0 - std::sqrt( 3.0 ) ) / 6.0;

	double s = ( x + y ) * F2;
	int i = simplex_fast_floor( x + s );
	int j = simplex_fast_floor( y + s );

	double t  = ( i + j ) * G2;
	double x0 = x - ( i - t );
	double y0 = y - ( j - t );

	int i1 = x0 > y0 ? 1 : 0;
	int j1 = x0 > y0 ? 0 : 1;

	double x1 = x0 - i1 + G2;
	double y1 = y0 - j1 + G2;
	double x2 = x0 - 1.0 + 2.0 * G2;
	double y2 = y0 - 1.0 + 2.0 * G2;

	int ii  = i & 255;
	int jj  = j & 255;
	int gi0 = simplex_perm[ii      + simplex_perm[jj     ]] % 12;
	int gi1 = simplex_perm[ii + i1 + simplex_perm[jj + j1]] % 12;
	int gi2 = simplex_perm[ii + 1  + simplex_perm[jj + 1 ]] % 12;

	double t0 = 0.5 - x0*x0 - y0*y0;
	double n0 = t0 < 0.0 ? 0.0 : ( t0*t0*t0*t0 ) * simplex_dot( grad3[gi0], x0, y0 );

	double t1 = 0.5 - x1*x1 - y1*y1;
	double n1 = t1 < 0.0 ? 0.0 : ( t1*t1*t1*t1 ) * simplex_dot( grad3[gi1], x1, y1 );

	double t2 = 0.5 - x2*x2 - y2*y2;
	double n2 = t2 < 0.0 ? 0.0 : ( t2*t2*t2*t2 ) * simplex_dot( grad3[gi2], x2, y2 );

	return 70.0 * ( n0 + n1 + n2 );
}
