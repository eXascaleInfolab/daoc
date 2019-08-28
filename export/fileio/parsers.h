//! \brief Common parsers headers of the High Resolution Hierarchical Clustering with Stable State (HiReCS) library
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! > 	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2016-08-12

#ifndef PARSERS_H
#define PARSERS_H

#include <cstdio>  // File operations, perror
#include <cstring>  // strchr, strerror
#include <cstdlib>  // strtof, strtoi, etc.
//#include <algorithm>  // sort(), swap(), max(), move[container items]()
#include <stdexcept>  // Exception (for Arguments processing)

#include "types.h"
#include "iotypes.h"


namespace hirecs {

using std::is_integral;
using std::is_floating_point;
// Used in most of the parsers
using std::out_of_range;
using std::domain_error;


//! \brief Parse and validate a value
//! \pre ResT is an arithmetic type
//! \tparam ResT  - required type of the parsed value
//! \tparam StrToValF  - parsing function in the format
//! 	ValT strToVal(const char *str, char **str_end, int base)  - for integral types
//! 	ValT strToVal(const char *str, char **str_end)  - for floating point types
//!
//! \param str const char* - the source text to be parsed
//! \param strToVal StrToValF  - the parsing function
//! \param pose=nullptr char**  - address in that parsing source after the parsed value
//! \param bool (*invalid)(ResT val, char end)=nullptr
//! 		where val - parsed value, end - symbol following the parsed value or 0
//! 	- forced invalidation condition of the parsed value
//! \param invalmsg=nullptr const char*  - invalidation message
//! \param base=10 int  - base of the parsed integral value, 10 - decimal
//! \return ResT
template <typename ResT, typename StrToValF, enable_if_t<is_integral<ResT>::value>* = nullptr>
inline ResT parseVal(const char* str, StrToValF strToVal, char **pose=nullptr
	, bool (*invalid)(ResT val, char end)=nullptr, const char* invalmsg=nullptr, int base=10);

//! \brief Parse and validate a value
//! \pre ResT is an arithmetic type
//! \tparam ResT  - required type of the parsed value
//! \tparam StrToValF  - parsing function in the format
//! 	ValT strToVal(const char *str, char **str_end, int base)  - for integral types
//! 	ValT strToVal(const char *str, char **str_end)  - for floating point types
//!
//! \param str const char* - the source text to be parsed
//! \param strToVal StrToValF  - the parsing function
//! \param pose=nullptr char**  - address in that parsing source after the parsed value
//! \param bool (*invalid)(ResT val, char end)=nullptr
//! 		where val - parsed value, end - symbol following the parsed value or 0
//! 	- forced invalidation condition of the parsed value
//! \param invalmsg=nullptr const char*  - invalidation message
//! \return ResT
template <typename ResT, typename StrToValF
	, enable_if_t<is_floating_point<ResT>::value, float>* = nullptr>
inline ResT parseVal(const char* str, StrToValF strToVal, char **pose=nullptr
	, bool (*invalid)(ResT val, char end)=nullptr, const char* invalmsg=nullptr);

//! \brief Validate parsed value
//! Checks that the value is a valid ResT instance
//! \pre ValT is an arithmetic type
//!
//! \param val VatT  - the value to be validated
//! \param str=nullptr const char*  - the parsed text to be shown in case of the error
//! \param invalidate=false bool  - forced invalidation condition of the parsed value
//! \param invalmsg=nullptr const char*  - invalidation message
//! \return void
template <typename ResT, typename VatT>
void validateVal(VatT val, const char* str=nullptr, bool invalidate=false
	, const char* invalmsg=nullptr);

}  // hirecs


#endif // PARSERS_H
