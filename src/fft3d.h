// This file is part of SIRIUS
//
// Copyright (c) 2013 Anton Kozhevnikov, Thomas Schulthess
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that 
// the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the 
//    following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
//    and the following disclaimer in the documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED 
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef __FFT3D_H__
#define __FFT3D_H__

/** \file fft3d.h
    
    \brief Interface to FFTW3 library.
    
    FFT convention:
    \f[
        f({\bf r}) = \sum_{{\bf G}} e^{i{\bf G}{\bf r}} f({\bf G})
    \f]
    is a \em backward transformation from a set of pw coefficients to a function.  

    \f[
        f({\bf G}) = \frac{1}{\Omega} \int e^{-i{\bf G}{\bf r}} f({\bf r}) d {\bf r} = 
            \frac{1}{N} \sum_{{\bf r}_j} e^{-i{\bf G}{\bf r}_j} f({\bf r}_j)
    \f]
    is a \em forward transformation from a function to a set of coefficients. 

    FFTW performs an "out of place" transformation, which means that we need to allocate both input and output buffers.
    To get the most performance out of multithreading we are going to put whole FFTs into different threads instead
    of using threaded implementation for each transform. 
*/

#include <fftw3.h>
#include <vector>

namespace sirius
{

template <processing_unit_t> 
class FFT3D;

#include "fft3d_cpu.hpp"

#ifdef _GPU_
#include "fft3d_gpu.hpp"
#endif

};

#endif // __FFT3D_H__

/** \page ft_pw Fourier transform and plane wave normalization

    We use plane waves in two different cases: a) plane waves (or augmented plane waves in the case of APW+lo method)
    form a basis for expanding Kohn-Sham wave functions and b) plane waves are used to expand charge density and
    potential. When we are dealing with plane wave basis functions it is convenient to adopt the following 
    normalization:
    \f[
        \langle {\bf r} |{\bf G+k} \rangle = \frac{1}{\sqrt \Omega} e^{i{\bf (G+k)r}}
    \f]
    such that
    \f[
        \langle {\bf G+k} |{\bf G'+k} \rangle_{\Omega} = \delta_{{\bf GG'}}
    \f]
    in the unit cell. However, for the expansion of periodic functions such as density or potential, the following 
    convention is more appropriate:
    \f[
        \rho({\bf r}) = \sum_{\bf G} e^{i{\bf Gr}} \rho({\bf G})
    \f]
    where
    \f[
        \rho({\bf G}) = \frac{1}{\Omega} \int_{\Omega} e^{-i{\bf Gr}} \rho({\bf r}) d{\bf r} = 
            \frac{1}{\Omega} \sum_{{\bf r}_i} e^{-i{\bf Gr}_i} \rho({\bf r}_i) \frac{\Omega}{N} = 
            \frac{1}{N} \sum_{{\bf r}_i} e^{-i{\bf Gr}_i} \rho({\bf r}_i) 
    \f]
    i.e. with such convention the plane-wave expansion coefficients are obtained with a normalized FFT.
*/

