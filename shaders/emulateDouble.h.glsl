// on MacOS, Vulkan runs on the Metal platform (through MoltenVK)
// the problem is that Metal DOES NOT SUPPORT double!!!
// => use float-float representation, in which a d64 is represented by two f32 numbers, or df64 for short 
// => convert f32 <==> df64
// add / subtract ( == add negative num )
// mul
// sqrt

#define TRUE    1
#define FALSE   0

// set everything to false to fall back to single-precision (native) floats
#define USE_NATIVE_FP64         FALSE // ==> won't run on MacOS over Metal

#define FP128_uint4_fp_32_96    ( FALSE && !USE_NATIVE_FP64 )
#define FP256_ulong4_fp_64_192  ( FALSE && !FP128_uint4_fp_32_96 && !USE_NATIVE_FP64 )

#define FP_64_64_R128           ( FALSE && !FP128_uint4_fp_32_96 && !FP256_ulong4_fp_64_192 && !USE_NATIVE_FP64 )
#define DF64_F32_F32            ( TRUE && !FP_64_64_R128 && !FP128_uint4_fp_32_96 && !FP256_ulong4_fp_64_192 && !USE_NATIVE_FP64 )


#if ( DF64_F32_F32 == TRUE )
// http://andrewthall.org/papers/df64_qf128.pdf
// https://github.com/sukop/doubledouble/blob/master/doubledouble.py

// ### representation ###
// df64 = ( hi, lo );
// hi part is the single-precision float, lo part is 0
highp vec2 df64_from_f32( highp float val_f32 ) {
    highp vec2 val_df64 = vec2( val_f32, 0.0 );
    return val_df64;
}

highp float df64_to_f32( highp vec2 val_df64 ) {
    highp float val_f32 = val_df64.x;
    return val_f32;
}

// ### comparisons ###
bool df64_eq( highp vec2 a, highp vec2 b ) {
    //return all( a == b );
    return a.x == b.x && a.y == b.y;
}

bool df64_neq( highp vec2 a, highp vec2 b ) {
    //return any( a != b );
    return a.x != b.x || a.y != b.y;
}

bool df64_lt( highp vec2 a, highp vec2 b ) {
    return ( a.x < b.x || ( a.x == b.x && a.y < b.y ) );
}


// ### addition ###
highp vec2 quickTwoSum( highp float a, highp float b ) {
    highp float s = a + b;
    highp float e = b - ( s - a );
    return vec2( s, e );
}

highp vec2 twoSum( highp float a, highp float b ) {
    highp float s = a + b;
    highp float v = s - a;
    highp float e = ( a - ( s - v ) ) + ( b - v );
    return vec2( s, e );
}

highp vec2 twoDiff( highp float a, highp float b ) {
    highp float s = a - b;
    highp float v = s - a;
    highp float e = ( a - ( s - v ) ) - ( b + v );
    return vec2( s, e );
}

highp vec2 df64_add( highp vec2 a, highp vec2 b ) {
    highp vec2 s, t;
    s = twoSum( a.x, b.x );
    t = twoSum( a.y, b.y );
    s.y += t.x;
    s = quickTwoSum( s.x, s.y );
    s.y += t.y;
    s = quickTwoSum( s.x, s.y );
    return s;
}


// ### multiplication ###
highp vec2 split( highp float a ) {
    highp const float split = 4097; // ( 1 << 12 ) + 1;
    highp float t = a * split;
    highp float a_hi = t - ( t - a );
    highp float a_lo = a - a_hi;
    return vec2( a_hi, a_lo );
}

highp vec2 twoProd( highp float a, highp float b ) {
    highp float p = a * b;
    highp vec2 aS = split( a );
    highp vec2 bS = split( b );
    highp float err = ( ( aS.x * bS.x - p )
                + aS.x * bS.y + aS.y * bS.x )
                + aS.y  * bS.y;
    return vec2( p, err );
}

highp vec2 df64_mult( highp vec2 a, highp vec2 b ) {
    highp vec2 p = twoProd( a.x, b.x );
    p.y += a.x * b.y;
    p.y += a.y * b.x;
    p = quickTwoSum( p.x, p.y );
    return p;
}

highp vec2 df64_sqrt( highp vec2 a ) {
    float xn = inversesqrt( a.x );
    //highp float xn = 1.0 / sqrt( a.x ); // better precision?
    highp float yn = a.x * xn;
    highp vec2 yn_df64 = df64_from_f32( yn );
    highp vec2 ynsqr = df64_mult( yn_df64, yn_df64 );

 //   float diff = df64_to_f32( df64_diff( a, ynsqr  ) );
    highp float diff = df64_to_f32( df64_add( a, df64_mult( ynsqr, df64_from_f32( -1.0 ) )  ) );
    highp vec2 prod = twoProd( xn, diff ) * 0.5;
    return df64_add( df64_from_f32( yn ), prod );
}


// ### convenience functions ###
highp vec2 df64_dot3( 
    highp vec2 ax_df64, highp vec2 ay_df64, highp vec2 az_df64, 
    highp vec2 bx_df64, highp vec2 by_df64, highp vec2 bz_df64 ) {
    //v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;

    highp vec2 xMul_df64 = df64_mult( ax_df64, bx_df64 );
    highp vec2 yMul_df64 = df64_mult( ay_df64, by_df64 );
    highp vec2 zMul_df64 = df64_mult( az_df64, bz_df64 );

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

#elif ( FP_64_64_R128 == TRUE )

    // https://github.com/fahickman/r128/blob/master/r128.h

    // 64-bit integer support
    // If your compiler does not have stdint.h, add appropriate defines for these macros.
    #define R128_U32 uint
    #define R128_S64 int64_t
    #define R128_U64 uint64_t
    // #define R128_LIT_S64(x) x##ll
    // #define R128_LIT_U64(x) x##ull

    // struct R128 {
    //     R128_U64 lo;
    //     R128_U64 hi;
    // };
    #define R128    u64vec2 // x .. lo, y .. hi


    #define R128_SET2(v, l, h) do { v.x = R128_U64(l); v.y = R128_U64(h); } while( false )

    #define R128_R0(v) (R128_U32(v.x))

    #define R128_R2(v) (R128_U32(v.y))

    #define R128_SET4(v, r0, r1, r2, r3) do { v.x = (R128_U64(r0) | ( R128_U64(r1) << 32 ) ); \
                                              v.y = (R128_U64(r2) | ( R128_U64(r3) << 32 ) ); } while( false )


    #define R128_R1(v) (R128_U32(v.x >> 32))

    #define R128_R3(v) (R128_U32(v.y >> 32))

    #define R128_min    R128( 0, 0x80000000u << 32U )

    #define R128_max    R128( (0xffffffffu << 32U) | 0xffffffffu , (0x7fffffffu << 32U) | 0xffffffffu )

    #define R128_smallest   R128( 1, 0 )

    #define R128_zero       R128( 0, 0 )

    #define R128_one        R128( 0, 1 )

    int r128_Clz64(R128_U64 v) {
        R128_U64 n = 64, w;
        w = ( v >> 32 ); if ( w != 0 ) { n -= 32; v = w; }
        w = ( v >> 16 ); if ( w != 0 ) { n -= 16; v = w; }
        w = ( v >>  8 ); if ( w != 0 ) { n -=  8; v = w; }
        w = ( v >>  4 ); if ( w != 0 ) { n -=  4; v = w; }
        w = ( v >>  2 ); if ( w != 0 ) { n -=  2; v = w; }
        w = ( v >>  1 ); if ( w != 0 ) { n -=  1; v = w; }
        return int(n - v);
    }

    #define r128_Umul64( a, b ) ( ( a ) * R128_U64( b ) )

    R128_U32 r128_Udiv64( R128_U32 nlo, R128_U32 nhi, R128_U32 d, out R128_U32 rem ) {
        R128_U64 n64 = ( ( R128_U64( nhi ) << 32 ) | nlo );
        rem = R128_U32( n64 % d );
        return R128_U32( n64 / d );
    }

    void r128_Neg(out R128 dst, const R128 src) {
        if (src.x != 0) {
            dst.x = ~src.x + 1;
            dst.y = ~src.y;
        } else {
            dst.x = 0;
            dst.y = ~src.y + 1;
        }
    }

    void r128Shl( out R128 dst, const R128 src, int amount) {
        R128_U64 r[4];

        r[0] = src.x;
        r[1] = src.y;

        amount &= 127;
        if (amount >= 64) {
            r[1] = r[0] << (amount - 64);
            r[0] = 0;
        } else if (amount != 0) {
            r[1] = (r[1] << amount) | (r[0] >> (64 - amount));
            r[0] = r[0] << amount;
        }

        dst.x = r[0];
        dst.y = r[1];
    }

    void r128Shr(out R128 dst, const R128 src, int amount) {
        R128_U64 r[4];

        r[2] = src.x;
        r[3] = src.y;

        amount &= 127;
        if (amount >= 64) {
            r[2] = r[3] >> (amount - 64);
            r[3] = 0;
        } else if (amount != 0) {
            r[2] = (r[2] >> amount) | (r[3] << (64 - amount));
            r[3] = r[3] >> amount;
        }

        dst.x = r[2];
        dst.y = r[3];
    }

    void r128Sar(out R128 dst, const R128 src, int amount) {
        R128_U64 r[4];

        r[2] = src.x;
        r[3] = src.y;

        amount &= 127;
        if (amount >= 64) {
            r[2] = R128_U64(R128_S64(r[3]) >> (amount - 64));
            r[3] = R128_U64(R128_S64(r[3]) >> 63);
        } else if (amount != 0) {
            r[2] = ( (r[2] >> amount) | R128_U64( R128_S64( r[3] ) << (64 - amount) ) );
            r[3] = R128_U64( R128_S64(r[3]) >> amount );
        }

        dst.x = r[2];
        dst.y = r[3];
    }

    // 64*64->128
    void r128_Umul128(out R128 dst, R128_U64 a, R128_U64 b) {
        R128_U32 alo = R128_U32(a);
        R128_U32 ahi = R128_U32(a >> 32);
        R128_U32 blo = R128_U32(b);
        R128_U32 bhi = R128_U32(b >> 32);
        R128_U64 p0, p1, p2, p3;

        p0 = r128_Umul64(alo, blo);
        p1 = r128_Umul64(alo, bhi);
        p2 = r128_Umul64(ahi, blo);
        p3 = r128_Umul64(ahi, bhi);

        {
            R128_U64 carry, lo, hi;
            carry = ( ( R128_U64( R128_U32( p1 ) ) + R128_U64( R128_U32( p2 ) ) + ( p0 >> 32 ) ) >> 32 );

            lo = p0 + ((p1 + p2) << 32);
            hi = p3 + ( R128_U32( p1 >> 32 ) + R128_U32( p2 >> 32 ) ) + carry;

            R128_SET2(dst, lo, hi);
        }
    }

    // 128/64->64
    R128_U64 r128_Udiv128(R128_U64 nlo, R128_U64 nhi, R128_U64 d, out R128_U64 rem) {
        R128_U64 tmp;
        R128_U32 d0, d1;
        R128_U32 n3, n2, n1, n0;
        R128_U32 q0, q1;
        R128_U32 r;
        int shift;

        //R128_ASSERT(d != 0);    //division by zero
        //R128_ASSERT(nhi < d);   //overflow

        // normalize
        shift = r128_Clz64(d);

        if (shift != 0) {
            R128 tmp128;
            R128_SET2(tmp128, nlo, nhi);
            r128Shl(tmp128, tmp128, shift);
            n3 = R128_R3(tmp128);
            n2 = R128_R2(tmp128);
            n1 = R128_R1(tmp128);
            n0 = R128_R0(tmp128);
            d <<= shift;
        } else {
            n3 = R128_U32(nhi >> 32);
            n2 = R128_U32(nhi);
            n1 = R128_U32(nlo >> 32);
            n0 = R128_U32(nlo);
        }

        d1 = R128_U32(d >> 32);
        d0 = R128_U32(d);

        // first digit
        //R128_ASSERT(n3 <= d1);
        if (n3 < d1) {
            q1 = r128_Udiv64(n2, n3, d1, r);
        } else {
            q1 = 0xffffffffu;
            r = n2 + d1;
        }
        // refine1:
        // if (r128_Umul64(q1, d0) > (R128_U64(r) << 32) + n1) {
        //     --q1;
        //     if (r < ~d1 + 1) {
        //         r += d1;
        //         goto refine1;
        //     }
        // }
        {
            bool refine;
            do {
                refine = false;
                if (r128_Umul64(q1, d0) > (R128_U64(r) << 32) + n1) {
                    --q1;
                    if (r < ~d1 + 1) {
                        r += d1;
                        refine = true;
                    }
                }
            } while ( refine );
        }

        tmp = (R128_U64(n2) << 32) + n1 - (r128_Umul64(q1, d0) + (r128_Umul64(q1, d1) << 32));
        n2 = R128_U32(tmp >> 32);
        n1 = R128_U32(tmp);

        // second digit
        //R128_ASSERT(n2 <= d1);
        if (n2 < d1) {
            q0 = r128_Udiv64(n1, n2, d1, r);
        } else {
            q0 = 0xffffffffu;
            r = n1 + d1;
        }

        // refine0:
        // if (r128_Umul64(q0, d0) > (R128_U64(r) << 32) + n0) {
        //     --q0;
        //     if (r < ~d1 + 1) {
        //         r += d1;
        //         goto refine0;
        //     }
        // }
        {
            bool refine;
            do {
                refine = false;
                if (r128_Umul64(q0, d0) > (R128_U64(r) << 32) + n0) {
                    --q0;
                    if (r < ~d1 + 1) {
                        r += d1;
                        refine = true;
                    }
                }
            } while( refine );
        }

        tmp = (R128_U64(n1) << 32) + n0 - (r128_Umul64(q0, d0) + (r128_Umul64(q0, d1) << 32));
        n1 = R128_U32(tmp >> 32);
        n0 = R128_U32(tmp);

        rem = ((R128_U64(n1) << 32) + n0) >> shift;
        return (R128_U64(q1) << 32) + q0;
    }

    int r128_Ucmp(const R128 a, const R128 b)
    {
        if (a.y != b.y) {
            if (a.y > b.y) {
                return 1;
            } else {
                return -1;
            }
        } else {
            if (a.x == b.x) {
                return 0;
            } else if (a.x > b.x) {
                return 1;
            } else {
                return -1;
            }
        }
    }

    void r128Add(out R128 dst, const R128 a, const R128 b) {
        //unsigned char carry = 0;
        uint carry = 0;
        {
            R128_U64 r = a.x + b.x;
            carry = ( r < a.x ) ? 1 : 0;
            dst.x = r;
            dst.y = a.y + b.y + carry;
        }
    }

    void r128Sub(out R128 dst, const R128 a, const R128 b) {
        //unsigned char borrow = 0;
        uint borrow = 0;
        {
            R128_U64 r = a.x - b.x;
            borrow = ( r > a.x ) ? 1 : 0;
            dst.x = r;
            dst.y = a.y - b.y - borrow;
        }
    }

    void r128_Umul(out R128 dst, const R128 a, const R128 b) {
        R128 p0, p1, p2, p3, round;

        r128_Umul128(p0, a.x, b.x);
        round.y = 0; round.x = p0.x >> 63;
        p0.x = p0.y; p0.y = 0; //r128Shr(&p0, &p0, 64);
        r128Add(p0, p0, round);

        r128_Umul128(p1, a.y, b.x);
        r128Add(p0, p0, p1);

        r128_Umul128(p2, a.x, b.y);
        r128Add(p0, p0, p2);

        r128_Umul128(p3, a.y, b.y);
        p3.y = p3.x; p3.x = 0; //r128Shl(&p3, &p3, 64);
        r128Add(p0, p0, p3);

        R128_SET2(dst, p0.x, p0.y);
    }

    // Shift d left until the high bit is set, and shift n left by the same amount.
    // returns non-zero on overflow.
    int r128_Norm(out R128 n, out R128 d, out R128_U64 n2) {
        R128_U64 d0, d1;
        R128_U64 n0, n1;
        int shift;

        d1 = d.y;
        d0 = d.x;
        n1 = n.y;
        n0 = n.x;

        if (d1 != 0) {
            shift = r128_Clz64(d1);
            if (shift != 0) {
                d1 = (d1 << shift) | (d0 >> (64 - shift));
                d0 = d0 << shift;
                n2 = n1 >> (64 - shift);
                n1 = (n1 << shift) | (n0 >> (64 - shift));
                n0 = n0 << shift;
            } else {
                n2 = 0;
            }
        } else {
            shift = r128_Clz64(d0);
            if (r128_Clz64(n1) <= shift) {
                return 1; // overflow
            }

            if (shift != 0) {
                d1 = d0 << shift;
                d0 = 0;
                n2 = (n1 << shift) | (n0 >> (64 - shift));
                n1 = n0 << shift;
                n0 = 0;
            } else {
                d1 = d0;
                d0 = 0;
                n2 = n1;
                n1 = n0;
                n0 = 0;
            }
        }

        R128_SET2(n, n0, n1);
        R128_SET2(d, d0, d1);
        return 0;
    }

    void r128_Udiv(out R128 quotient, const R128 dividend, const R128 divisor) {
        R128 tmp;
        R128_U64 d0, d1;
        R128_U64 n1, n2, n3;
        R128 q;

        //R128_ASSERT(divisor.y != 0 || divisor.x != 0);  // divide by zero

        // scale dividend and normalize
        {
            R128 n, d;
            R128_SET2(n, dividend.x, dividend.y);
            R128_SET2(d, divisor.x, divisor.y);
            if (r128_Norm(n, d, n3) != 0) {
                R128_SET2(quotient, R128_max.x, R128_max.y);
                return;
            }

            d1 = d.y;
            d0 = d.x;
            n2 = n.y;
            n1 = n.x;
        }

        // first digit
        //R128_ASSERT(n3 <= d1);
        {
            R128 t0, t1;
            t0.x = n1;
            if (n3 < d1) {
                q.y = r128_Udiv128(n2, n3, d1, t0.y);
            } else {
                q.y = ( ( 0xffffffffu << 32U ) | 0xffffffffu );
                t0.y = n2 + d1;
            }

        // refine1:
        //     r128_Umul128(&t1, q.y, d0);
        //     if (r128_Ucmp(&t1, t0) > 0) {
        //         --q.y;
        //         if (t0.y < ~d1 + 1) {
        //             t0.y += d1;
        //             goto refine1;
        //         }
        //     }
            {
                bool refine;

                do {
                    refine = false;
                    r128_Umul128(t1, q.y, d0);
                    if (r128_Ucmp(t1, t0) > 0) {
                        --q.y;
                        if (t0.y < ~d1 + 1) {
                            t0.y += d1;
                            refine = true;
                        }
                    }
                } while( refine );
            }

        }

        {
            R128 t0, t1, t2;
            t0.y = n2;
            t0.x = n1;

            r128_Umul128(t1, q.y, d0);
            r128_Umul128(t2, q.y, d1);

            t2.y = t2.x; t2.x = 0;  //r128Shl(&t2, &t2, 64);
            r128Add(tmp, t1, t2);
            r128Sub(tmp, t0, tmp);
        }
        n2 = tmp.y;
        n1 = tmp.x;

        // second digit
        //R128_ASSERT(n2 <= d1);
        {
            R128 t0, t1;
            t0.x = 0;
            if (n2 < d1) {
                q.x = r128_Udiv128(n1, n2, d1, t0.y);
            } else {
                q.x = ( ( 0xffffffffu << 32U ) | 0xffffffffu );
                t0.y = n1 + d1;
            }

        // refine0:
        //     r128_Umul128(t1, q.x, d0);
        //     if (r128_Ucmp(t1, t0) > 0) {
        //         --q.x;
        //         if (t0.y < ~d1 + 1) {
        //             t0.y += d1;
        //             goto refine0;
        //         }
        //     }
        // }
            {
                bool refine;
                do {
                    refine = false;
                    r128_Umul128(t1, q.x, d0);
                    if (r128_Ucmp(t1, t0) > 0) {
                        --q.x;
                        if (t0.y < ~d1 + 1) {
                            t0.y += d1;
                            refine = true;
                        }
                    }
                } while( refine );
            }
        }

        R128_SET2(quotient, q.x, q.y);
    }

    R128_U64 r128_Umod(out R128 n, out R128 d) {
        R128_U64 d0, d1;
        R128_U64 n3, n2, n1;
        R128_U64 q;

        //R128_ASSERT(d.y != 0 || d.x != 0);  // divide by zero

        if (r128_Norm(n, d, n3) != 0) {
            return ( ( 0xffffffffu << 32U ) | 0xffffffffu );
        }

        d1 = d.y;
        d0 = d.x;
        n2 = n.y;
        n1 = n.x;

        //R128_ASSERT(n3 < d1);
        {
            R128 t0, t1;
            t0.x = n1;
            q = r128_Udiv128(n2, n3, d1, t0.y);

            {
                bool refine;
                do {
                    refine = false;
                    r128_Umul128(t1, q, d0);
                    if (r128_Ucmp(t1, t0) > 0) {
                        --q;
                        if (t0.y < ~d1 + 1) {
                            t0.y += d1;
                            refine = true;
                        }
                    }
                } while( refine );
            }
        }

        return q;
    }

    void r128FromInt(out R128 dst, R128_S64 v) {
        dst.x = 0;
        dst.y = R128_U64(v);
    }

    void r128Copy(out R128 dst, const R128 src) {
        dst.x = src.x;
        dst.y = src.y;
    }

    // void r128CopyConst(out R128 dst, __constant R128 src) {
    //     dst.x = src.x;
    //     dst.y = src.y;
    // }

    //void r128FromFloat(R128 *dst, double v) {
    void r128FromFloat(out R128 dst, float v) {
        if (v < -9223372036854775808.0) {
            //r128CopyConst(dst, &R128_min);
            r128Copy(dst, R128_min);
        } else if (v >= 9223372036854775808.0) {
            //r128CopyConst(dst, &R128_max);
            r128Copy(dst, R128_max);
        } else {
            R128 r;
            int sign = 0;

            if (v < 0) {
                v = -v;
                sign = 1;
            }

            r.y = R128_U64(R128_S64(v));
            v -= float( R128_S64(v) );
            r.x = R128_U64(v * 18446744073709551616.0);

            if (sign != 0) {
                r128_Neg(r, r);
            }

            r128Copy(dst, r);
        }
    }

    R128_S64 r128ToInt(const R128 v) {
        return R128_S64(v.y);
    }

    int r128IsNeg(const R128 v) {
        return ( R128_S64(v.y) < 0 ) ? 1 : 0;
    }

    //double r128ToFloat(const R128 *v) {
    float r128ToFloat(const R128 v) {
        R128 tmp;
        int sign = 0;
        float d;

        R128_SET2(tmp, v.x, v.y);
        if (r128IsNeg(tmp) != 0) {
            r128_Neg(tmp, tmp);
            sign = 1;
        }

        d = float( tmp.y ) + float(tmp.x) * (1.0 / 18446744073709551616.0);
        if (sign != 0) {
            d = -d;
        }

        return d;
    }


    void r128Neg(out R128 dst, const R128 src) {
        r128_Neg(dst, src);
    }

    void r128Not(out R128 dst, const R128 src) {
        dst.x = ~src.x;
        dst.y = ~src.y;
    }

    void r128Or(out R128 dst, const R128 a, const R128 b) {
        dst.x = a.x | b.x;
        dst.y = a.y | b.y;
    }

    void r128And(out R128 dst, const R128 a, const R128 b) {
        dst.x = a.x & b.x;
        dst.y = a.y & b.y;
    }

    void r128Xor(out R128 dst, const R128 a, const R128 b) {
        dst.x = a.x ^ b.x;
        dst.y = a.y ^ b.y;
    }

    void r128Mul(out R128 dst, const R128 a, const R128 b) {
        int sign = 0;
        R128 ta, tb, tc;

        R128_SET2(ta, a.x, a.y);
        R128_SET2(tb, b.x, b.y);

        if (r128IsNeg(ta) != 0) {
            r128_Neg(ta, ta);
            //sign = !sign;
            sign = 1 - sign;
        }
        if (r128IsNeg(tb) != 0) {
            r128_Neg(tb, tb);
            //sign = !sign;
            sign = 1 - sign;
        }

        r128_Umul(tc, ta, tb);
        if (sign != 0) {
            r128_Neg(tc, tc);
        }

        r128Copy(dst, tc);
    }

    void r128Div(out R128 dst, const R128 a, const R128 b) {
        int sign = 0;
        R128 tn, td, tq;

        R128_SET2(tn, a.x, a.y);
        R128_SET2(td, b.x, b.y);

        if (r128IsNeg(tn) != 0) {
            r128_Neg(tn, tn);
            //sign = !sign;
            sign = 1 - sign;
        }

        if (td.x == 0 && td.y == 0) {
            // divide by zero
            if (sign != 0) {
                r128Copy(dst, R128_min);
            } else {
                r128Copy(dst, R128_max);
            }
            return;
        } else if (r128IsNeg(td) != 0) {
            r128_Neg(td, td);
            //sign = !sign;
            sign = 1 - sign;
        }

        r128_Udiv(tq, tn, td);

        if (sign != 0) {
            r128_Neg(tq, tq);
        }

        r128Copy(dst, tq);
    }

    void r128Mod(out R128 dst, const R128 a, const R128 b) {
        int sign = 0;
        R128 tn, td, tq;

        R128_SET2(tn, a.x, a.y);
        R128_SET2(td, b.x, b.y);

        if (r128IsNeg(tn) != 0) {
            r128_Neg(tn, tn);
            //sign = !sign;
            sign = 1 - sign;
        }

        if (td.x == 0 && td.y == 0) {
            // divide by zero
            if (sign != 0) {
                r128Copy(dst, R128_min);
            } else {
                r128Copy(dst, R128_max);
            }
            return;
        } else if (r128IsNeg(td) != 0) {
            r128_Neg(td, td);
            //sign = !sign;
            sign = 1 - sign;
        }

        tq.y = r128_Umod(tn, td);
        tq.x = 0;

        if (sign != 0) {
            tq.y = ~tq.y + 1;
        }

        r128Mul(tq, tq, b);
        r128Sub(dst, a, tq);
    }

    void r128Rsqrt(out R128 dst, const R128 v) {
        const R128 threeHalves = R128( 0x80000000u << 32U, 1u );
        R128 val, est;
        int i;

        if (R128_S64(v.y) < 0) {
            r128Copy(dst, R128_min);
            return;
        }

        R128_SET2(val, v.x, v.y);

        // get initial estimate
        if (val.y != 0) {
            int shift = (64 + r128_Clz64(val.y)) >> 1;
            est.x = (1u << shift);
            est.y = 0;
        } else if (val.x != 0) {
            int shift = r128_Clz64(val.x) >> 1;
            est.y = (1u << shift);
            est.x = 0;
        } else {
            R128_SET2(dst, 0, 0);
            return;
        }

        // x /= 2
        r128Shr(val, val, 1);

        // Newton-Raphson iterate
        for (i = 0; i < 7; ++i) {
            R128 newEst;

            // newEst = est * (threeHalves - (x / 2) * est * est);
            r128_Umul(newEst, est, est);
            r128_Umul(newEst, newEst, val);
            r128Sub(newEst, threeHalves, newEst);
            r128_Umul(newEst, est, newEst);

            if (newEst.x == est.x && newEst.y == est.y) {
                break;
            }
            R128_SET2(est, newEst.x, newEst.y);
        }

        r128Copy(dst, est);
    }

    void r128Sqrt(out R128 dst, const R128 v) {
        R128 val, est;
        int i;

        if (R128_S64(v.y) < 0) {
            r128Copy(dst, R128_min);
            return;
        }

        R128_SET2(val, v.x, v.y);

        // get initial estimate
        if (val.y != 0) {
            int shift = (63 - r128_Clz64(val.y)) >> 1;
            r128Shr(est, val, shift);
        } else if (val.x != 0) {
            int shift = (1 + r128_Clz64(val.x)) >> 1;
            r128Shl(est, val, shift);
        } else {
            R128_SET2(dst, 0, 0);
            return;
        }

        // Newton-Raphson iterate
        for (i = 0; i < 7; ++i) {
            R128 newEst;

            // newEst = (est + x / est) / 2
            r128_Udiv(newEst, val, est);
            r128Add(newEst, newEst, est);
            r128Shr(newEst, newEst, 1);

            if (newEst.x == est.x && newEst.y == est.y) {
                break;
            }
            R128_SET2(est, newEst.x, newEst.y);
        }

        r128Copy(dst, est);
    }

    int r128Cmp(const R128 a, const R128 b) {
        if (a.y == b.y) {
            if (a.x == b.x) {
                return 0;
            } else if (a.x > b.x) {
                return 1;
            } else {
                return -1;
            }
        } else if (R128_S64(a.y) > R128_S64(b.y)) {
            return 1;
        } else {
            return -1;
        }
    }

    // int r128CmpConst(const R128 a, __constant R128 b) {
    //     if (a.y == b.y) {
    //         if (a.x == b.x) {
    //             return 0;
    //         } else if (a.x > b.x) {
    //             return 1;
    //         } else {
    //             return -1;
    //         }
    //     } else if (R128_S64(a.y) > R128_S64(b.y)) {
    //         return 1;
    //     } else {
    //         return -1;
    //     }
    // }

    void r128Min(out R128 dst, const R128 a, const R128 b) {
        if (r128Cmp(a, b) < 0) {
            r128Copy(dst, a);
        } else {
            r128Copy(dst, b);
        }
    }

    void r128Max(out R128 dst, const R128 a, const R128 b) {
        if (r128Cmp(a, b) > 0) {
            r128Copy(dst, a);
        } else {
            r128Copy(dst, b);
        }
    }

    void r128Floor(out R128 dst, const R128 v) {
        if (R128_S64(v.y) < 0) {
            dst.y = v.y - ( (v.x != 0) ? 1 : 0 );
        } else {
            dst.y = v.y;
        }
        dst.x = 0;
    }

    void r128Ceil(out R128 dst, const R128 v) {
        if (R128_S64(v.y) > 0) {
            dst.y = v.y + ( (v.x != 0) ? 1 : 0 );
        } else {
            dst.y = v.y;
        }
        dst.x = 0;
    }

    void r128Dot3( out R128 result, 
                   const R128 aX, const R128 aY, const R128 aZ, 
                   const R128 bX, const R128 bY, const R128 bZ ) {
        R128 axbx, ayby, azbz;
        r128Mul( axbx, aX, bX ); r128Mul( ayby, aY, bY ); r128Mul( azbz, aZ, bZ );
        
        //r128Add( result, &axbx, &ayby ); r128Add( result, result, &azbz );
        R128 plusXY;
        r128Add( plusXY, axbx, ayby ); r128Add( result, plusXY, azbz );
    }
#endif