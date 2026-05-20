#ifndef COMMON_H
#define COMMON_H

    /** Non-Interleaving input buffer
     * Buffer arranged by float[chn][sample] 
     * const so that the user can't modify the input
    */
    typedef const float* const* InputBuffer;

    /** Non-Interleaving output buffer
     * Arranged by float[chn][sample] 
    */
    typedef float** OutputBuffer;

    typedef float sample_t;

/* lovely algorithm from 
  http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
*/
inline uint 
next_power_of_2 (uint n)
{
    assert (n <= 0x40000000);

    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;

    return ++n;
}

#ifndef max
template <class X, class Y> X min (X x, Y y) { return x < (X)y ? x : (X)y; }
template <class X, class Y> X max (X x, Y y) { return x > (X)y ? x : (X)y; }
#endif /* ! max */

    

#endif // COMMON_H
