/**********************************************************************
 * Copyright (c) 2013, 2014 Pieter Wuille                             *
 * Distributed under the GPL3 software license, see the accompanying   *
 * file COPYING or http://www.gnu.org/licenses/gpl.html.*
 **********************************************************************/

#ifndef _SECP256K1_NUM_IMPL_H_
#define _SECP256K1_NUM_IMPL_H_

#if defined HAVE_CONFIG_H
#include "libsecp256k1-config.h"
#endif

#include "num.h"

#if defined(USE_NUM_GMP)
#include "num_gmp_impl.h"
#elif defined(USE_NUM_NONE)
/* Nothing. */
#else
#error "Please select num implementation"
#endif

#endif
