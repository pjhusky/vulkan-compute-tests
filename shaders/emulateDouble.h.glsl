// on MacOS, Vulkan runs on the Metal platform (through MoltenVK)
// the problem is that Metal DOES NOT SUPPORT double!!!
// => use float-float representation, in which a d64 is represented by two f32 numbers, or df64 for short 
// => convert f32 <==> df64
// add / subtract ( == add negative num )
// mul
// sqrt

// http://andrewthall.org/papers/df64_qf128.pdf
// https://github.com/sukop/doubledouble/blob/master/doubledouble.py

// ### representation ###
// df64 = ( hi, lo );
// hi part is the single-precision float, lo part is 0
vec2 df64_from_f32( float val_f32 ) {
    vec2 val_df64 = vec2( val_f32, 0.0 );
    return val_df64;
}

float df64_to_f32( vec2 val_df64 ) {
    float val_f32 = val_df64.x;
    return val_f32;
}

// ### comparisons ###
bool df64_eq( vec2 a, vec2 b ) {
    //return all( a == b );
    return a.x == b.x && a.y == b.y;
}

bool df64_neq( vec2 a, vec2 b ) {
    //return any( a != b );
    return a.x != b.x || a.y != b.y;
}

bool df64_lt( vec2 a, vec2 b ) {
    return ( a.x < b.x || ( a.x == b.x && a.y < b.y ) );
}


// ### addition ###
vec2 quickTwoSum( float a, float b ) {
    float s = a + b;
    float e = b - ( s - a );
    return vec2( s, e );
}

vec2 twoSum( float a, float b ) {
    float s = a + b;
    float v = s - a;
    float e = ( a - ( s - v ) ) + ( b - v );
    return vec2( s, e );
}

vec2 twoDiff( float a, float b ) {
    float s = a - b;
    float v = s - a;
    float e = ( a - ( s - v ) ) - ( b + v );
    return vec2( s, e );
}

vec2 df64_add( vec2 a, vec2 b ) {
    vec2 s, t;
    s = twoSum( a.x, b.x );
    t = twoSum( a.y, b.y );
    s.y += t.x;
    s = quickTwoSum( s.x, s.y );
    s.y += t.y;
    s = quickTwoSum( s.x, s.y );
    return s;
}


// ### multiplication ###
vec2 split( float a ) {
    const float split = 4097; // ( 1 << 12 ) + 1;
    float t = a * split;
    float a_hi = t - ( t - a );
    float a_lo = a - a_hi;
    return vec2( a_hi, a_lo );
}

vec2 twoProd( float a, float b ) {
    float p = a * b;
    vec2 aS = split( a );
    vec2 bS = split( b );
    float err = ( ( aS.x * bS.x - p )
                + aS.x * bS.y + aS.y * bS.x )
                + aS.y  * bS.y;
    return vec2( p, err );
}

vec2 df64_mult( vec2 a, vec2 b ) {
    vec2 p = twoProd( a.x, b.x );
    p.y += a.x * b.y;
    p.y += a.y * b.x;
    p = quickTwoSum( p.x, p.y );
    return p;
}

vec2 df64_sqrt( vec2 a ) {
    float xn = inversesqrt( a.x );
    float yn = a.x * xn;
    vec2 yn_df64 = df64_from_f32( yn );
    vec2 ynsqr = df64_mult( yn_df64, yn_df64 );

 //   float diff = df64_to_f32( df64_diff( a, ynsqr  ) );
    float diff = df64_to_f32( df64_add( a, df64_mult( ynsqr, df64_from_f32( -1.0 ) )  ) );
    vec2 prod = twoProd( xn, diff ) * 0.5;
    return df64_add( df64_from_f32( yn ), prod );
}


// ### convenience functions ###
vec2 df64_dot3( vec2 ax_df64, vec2 ay_df64, vec2 az_df64, vec2 bx_df64, vec2 by_df64, vec2 bz_df64 ) {
    //v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;

    vec2 xMul_df64 = df64_mult( ax_df64, bx_df64 );
    vec2 yMul_df64 = df64_mult( ay_df64, by_df64 );
    vec2 zMul_df64 = df64_mult( az_df64, bz_df64 );

    return df64_add( df64_add( xMul_df64, yMul_df64 ), zMul_df64 );
}



//////////////////////////////////////////////////////////////////////////////////

// http://mrob.com/pub/math/f161.html

// double-double in CUDA
// https://gist.github.com/seibert/5914108
// https://gist.github.com/advanpix/68dd7e653e4fea4d97ab

// https://web.archive.org/web/20120714073115/http://homepages.math.uic.edu/~jan/mcs572/quad_double_cuda.pdf

// https://www.researchgate.net/publication/220706912_Supporting_extended_precision_on_graphics_processors

//////////////////////////////////////////////////////////////////////////////////

//#if 0

// https://www.davidhbailey.com/dhbpapers/qd.pdf
// https://web.archive.org/web/20110807003725/http://crd.lbl.gov/~dhbailey/mpdist/
// https://web.archive.org/web/20060118031825/http://crd.lbl.gov/~dhbailey/mpdist/

// https://www.researchgate.net/publication/2910134_Algorithms_for_Quad-Double_Precision_Floating_Point_Arithmetic


/********** Divisions **********/


//////////////////// ####################### ///////////////////

