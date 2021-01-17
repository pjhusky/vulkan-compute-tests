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

// def _two_difference(x, y):
//     r = x - y
//     t = r - x
//     e = (x - (r - t)) - (y + t)
//     return r, e

// vec2 twoDiff( float x, float y ) {
//     float r = x - y;
//     float t = r - x;
//     float e = ( x - ( r - t ) ) - ( y + t );
//     return vec2( r, e );
// }


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

// ### subtraction ###
// a - b 
// vec2 df64_sub( vec2 a, vec2 b ) {
//     vec2 minusOne_df64 = df64_from_f32( -1.0 );
//     vec2 minusB = df64_mul( b, minusOne_df64 );
//     return df64_add( a, minusB );
// }


// ### division ###
// float2 df64_div( vec2 b, vec2 a ) {
//     float xn = 1.0 / a.x;
//     float yn = b.x * xn;
//     //float diff = ( df64_diff( b, df64_mult( a, yn ) ) ).x
//     float diff = df64_diff( b, df64_mult( a, f64_from_f32( yn ) ) ).x;
//     vec2 prod = twoProd( xn, diff );

//     return df64_add( yn, prod );
// }   


// def sqrt(self):
//         if self.x == 0.0:
//             return _zero
//         r = sqrt(self.x)
//         s, f = _two_product(r, r)
//         e = (self.x - s - f + self.y)*0.5/r
//         r, e = _two_sum_quick(r, e)
//         return DoubleDouble(r, e)

// vec2 df64_sqrt( vec2 a ) {
//     if ( a.x == 0.0 ) { return df64_from_f32( 0.0 ); }
//     float r = sqrt( a.x );
//     vec2 sf = twoProd( r, r );
//     float s = sf.x;
//     float f = sf.y;
//     float e = ( a.x - s - f + a.x ) * 0.5 / r;
//     return quickTwoSum( r, e ); 
// }

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

// convert f32 vec3s to df64 and then perform dot with df64 precision
vec2 df64_dot3( vec3 a_f32, vec3 b_f32 ) {
    vec2 ax_df64 = df64_from_f32( a_f32.x );
    vec2 ay_df64 = df64_from_f32( a_f32.y );
    vec2 az_df64 = df64_from_f32( a_f32.z );

    vec2 bx_df64 = df64_from_f32( b_f32.x );
    vec2 by_df64 = df64_from_f32( b_f32.y );
    vec2 bz_df64 = df64_from_f32( b_f32.z );

    return df64_dot3( ax_df64, ay_df64, az_df64, bx_df64, by_df64, bz_df64 );
}