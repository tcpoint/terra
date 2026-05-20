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

#endif // COMMON_H
