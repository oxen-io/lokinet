/* Copyright (c) 2016, Pollard Banknote Limited
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file type_traits.h
 * @brief Implementation of C++11 type_traits header
 */
#ifndef PBL_CPP_TYPE_TRAITS_H
#define PBL_CPP_TYPE_TRAITS_H

#include "version.h"

#ifdef CPP11
#include <type_traits>
#endif

// Always include these as they have extensions above C++11
#include "traits/alignment.h"

#ifndef CPP11
#include "traits/declval.h"
#endif

#ifndef CPP14
#include "traits/add_cv.h"
#include "traits/add_pointer.h"
#include "traits/add_reference.h"
#include "traits/common_type.h"
#include "traits/decay.h"
#include "traits/enable_if.h"
#include "traits/make_signed.h"
#include "traits/make_unsigned.h"
#include "traits/remove_cv.h"
#include "traits/remove_extent.h"
#include "traits/remove_pointer.h"
#include "traits/remove_reference.h"
#include "traits/result_of.h"
#include "traits/underlying_type.h"
#endif

#ifndef CPP17
#include "traits/conditional.h"
#include "traits/extent.h"
#include "traits/integral_constant.h"
#include "traits/is_arithmetic.h"
#include "traits/is_array.h"
#include "traits/is_base_of.h"
#include "traits/is_compound.h"
#include "traits/is_const.h"
#include "traits/is_convertible.h"
#include "traits/is_enum.h"
#include "traits/void_t.h"
#include "traits/is_floating_point.h"
#include "traits/is_function.h"
#include "traits/is_fundamental.h"
#include "traits/is_integral.h"
#include "traits/is_member_pointer.h"
#include "traits/is_null_pointer.h"
#include "traits/is_object.h"
#include "traits/is_pointer.h"
#include "traits/is_reference.h"
#include "traits/is_same.h"
#include "traits/is_scalar.h"
#include "traits/is_signed.h"
#include "traits/is_unsigned.h"
#include "traits/is_void.h"
#include "traits/is_volatile.h"
#include "traits/rank.h"
#endif // ifndef CPP17
#endif // PBL_CPP_TYPE_TRAITS_H
