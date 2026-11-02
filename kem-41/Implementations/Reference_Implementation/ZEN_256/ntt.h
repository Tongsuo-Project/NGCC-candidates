/*
Copyright (c) 2026 Yu Zhang.
Organization: State Key Laboratory of Cyberspace Security Defense,Institute of Information Engineering, CAS
              School of Cyber Security, University of Chinese Academy of Sciences  
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-256 instance.
*/
#ifndef NTT_H
#define NTT_H

#include <stdint.h>
#include "params.h"
#define QINV -767
#define MONT 171

static inline int16_t montgomery_reduce(int32_t a)
{
    /* Only the low 16 bits are needed; unsigned multiplication avoids
     * signed overflow when a contains an accumulated dot product. */
    int16_t u = (int16_t)((uint32_t)a * (uint32_t)QINV);
    int32_t t = a - (int32_t)u * ZEN_Q;

    return (int16_t)(t >> 16);
}

#ifdef __cplusplus
extern "C"
{
#endif

    static const int16_t f[128] =
    {
        171, 605, 688, 361, 186, 766, 519, 649, 461, 129, 753, 546, 407, 626, 131, 432, 
        693, 671, 36, 694, 430, 514, 282, 566, 735, 199, 178, 270, 759, 149, 369, 577, 
        147, 655, 497, 54, 767, 645, 689, 423, 86, 718, 364, 267, 161, 754, 288, 169, 
        191, 307, 719, 745, 599, 226, 121, 581, 389, 279, 180, 394, 612, 263, 641, 523, 
        746, 112, 618, 635, 717, 621, 227, 232, 698, 212, 236, 21, 341, 379, 567, 549, 
        352, 292, 238, 145, 194, 493, 70, 495, 117, 333, 66, 247, 532, 686, 517, 525, 
        331, 528, 167, 357, 414, 291, 411, 105, 654, 560, 14, 99, 509, 29, 366, 391, 
        451, 278, 353, 354, 585, 127, 330, 466, 222, 691, 421, 725, 201, 158, 350, 168
    };

    static const int16_t fn[128] = 
    {
        601, 419, 611, 568, 44, 348, 78, 547, 303, 439, 642, 184, 415, 416, 491, 318, 
        378, 403, 740, 260, 670, 755, 209, 115, 664, 358, 478, 355, 412, 602, 241, 438, 
        244, 252, 83, 237, 522, 703, 436, 652, 274, 699, 276, 575, 624, 531, 477, 417, 
        220, 202, 390, 428, 748, 533, 557, 71, 537, 542, 148, 52, 134, 151, 657, 23, 
        246, 128, 506, 157, 375, 589, 490, 380, 188, 648, 543, 170, 24, 50, 462, 578, 
        600, 481, 15, 608, 502, 405, 51, 683, 346, 80, 124, 2, 715, 272, 114, 622, 
        192, 400, 620, 10, 499, 591, 570, 34, 203, 487, 255, 339, 75, 733, 98, 76, 
        337, 638, 143, 362, 223, 16, 640, 308, 120, 250, 3, 583, 408, 81, 164, 655
    };

    /// @brief Compute the forward Number Theoretic Transform (NTT) of a polynomial
    /// @param[in,out] a Base address of input polynomial coefficient array; replaced by its NTT representation
    /// @return None
    void poly_ntt(int16_t *a);

    /// @brief Compute the forward Number Theoretic Transform (NTT) of a polynomial and map coefficients into the standard range modulo ZEN_Q
    /// @param[in,out] a Base address of input polynomial coefficient array; replaced by its NTT representation with coefficients reduced modulo ZEN_Q
    /// @return None
    void poly_ntt_mq(int16_t *a);

    /// @brief Compute the inverse Number Theoretic Transform (INTT) of a polynomial
    /// @param[in,out] a Base address of input polynomial coefficient array in NTT domain; replaced by the polynomial in coefficient domain
    /// @return None
    void poly_intt(int16_t *a);

    /// @brief Perform block-wise polynomial multiplication in the NTT domain
    /// @param[out] r Base address of output polynomial coefficient array
    /// @param[in] a Base address of first input polynomial coefficient array in NTT domain
    /// @param[in] b Base address of second input polynomial coefficient array in NTT domain
    /// @return None
    void poly_basemul_ntt(int16_t *r, const int16_t *a, const int16_t *b);

    /// @brief Perform block-wise polynomial multiplication in the NTT domain and map coefficients into the standard range modulo ZEN_Q
    /// @param[out] r Base address of output polynomial coefficient array
    /// @param[in] a Base address of first input polynomial coefficient array in NTT domain
    /// @param[in] b Base address of second input polynomial coefficient array in NTT domain
    /// @return None
    void poly_basemul_ntt_mq(int16_t *r, const int16_t *a, const int16_t *b);

    /// @brief Perform block-wise inversion of a polynomial in the NTT domain
    /// @param[out] r Base address of output polynomial coefficient array in NTT domain
    /// @param[in] a Base address of input polynomial coefficient array in NTT domain
    /// @return None
    void poly_baseinv_ntt(int16_t *r, const int16_t *a);


#ifdef __cplusplus
}
#endif
#endif
