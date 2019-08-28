//! \brief Raw parsing routines.
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
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

#include <cstdlib>  // strtoul
#include <cstring>  // strerror
#include <cerrno>  // FILE errno (is used for the fast output)
//#include <stdexcept>  // Exception (for Arguments processing)
#include <cassert>
#include <system_error>  // error_code

//#ifdef __unix__
//#include <sys/stat.h>
//#else
#if defined(__has_include) && __has_include(<filesystem>)
	#include <filesystem>
	namespace fs = std::filesystem;
#elif defined(__has_include) && __has_include(<experimental/filesystem>)
	#include <experimental/filesystem>
	namespace fs = std::experimental::filesystem;
#else
	#error "STL filesystem is not available. The native alternative is not implemented."
#endif // __has_include
//#endif // __unix__

#include "types.h"
#include "iotypes.h"


namespace daoc {

using std::is_integral;
using std::is_floating_point;
using std::is_same;
using std::is_arithmetic;
using std::invalid_argument;
using std::error_code;


// Conditional operations -----------------------------------------------------
//! \brief Add link to the links
//! \post links are extended
//!
//! \param links Links<LinkT>&  - the links to be extended
//! \param id Id  - link id
//! \param weight=0 WeightT  - link weight
//! \return void
template <typename LinkT>
constexpr static enable_if_t<LinkT::IS_WEIGHTED>
addLink(Links<LinkT>& links, Id id, typename LinkT::Weight weight=0)
{
	if(weight)
		links.emplace_back(id, weight);
	else links.emplace_back(id);  // Use default value (= 1) if weight is not specified
}

template <typename LinkT>
constexpr static enable_if_t<!LinkT::IS_WEIGHTED>
addLink(Links<LinkT>& links, Id id, typename LinkT::Weight weight=0)
//constexpr void Operations<true>::addLink(Links<LinkT>& links, Id id, WeightT weight)
{
	links.emplace_back(id);
}

// Parser ----------------------------------------------------------------------
//! \brief Fetch file size
//!
//! \param name const string&  - file name
//! \return size_t  - file size
inline size_t filesize(const string& name)
{
	size_t  cmsbytes = -1;  // Return -1 on error
//#ifdef __unix__  // sqrt(cmsbytes) lines => linebuf = max(4-8Kb, sqrt(cmsbytes) * 2) with dynamic realloc
//	struct stat  filest;
//	int fd = fileno(m_file);  // FILE*
//	if(fd != -1 && !fstat(fd, &filest))
//		return filest.st_size;
//#endif // __unix
	error_code  err;
	cmsbytes = fs::file_size(name, err);
	if(cmsbytes == size_t(-1))
		fprintf(ftrace, "WARNING filesize(), file size evaluation failed: %s\n", err.message().c_str());

//	// Get length of the file
//	fseek(m_file, 0, SEEK_END);
//	cmsbytes = ftell(m_file);  // The number of bytes in the input communities
//	if(cmsbytes == size_t(-1))
//		perror("WARNING size(), file size evaluation failed");
//	//fprintf(ftrace, "  %s: %lu bytes\n", fname, cmsbytes);
//	rewind(m_file);  // Set position to the begin of the file

	return cmsbytes;
}

//! \brief Skip symbols
//!
//! \param str char*&  - input string to be updated to the position following the skipped symbols
//! \param skips const char*  - the symbols to skipped
//! \return char  - first symbol following the skipped once or EOL (0)
inline char skipSymbols(char*& str, const char* skips=" \t")
{
	// Note: isspace() should not be used, because \r, \n are not allowed to the skipped
	while(*str && strchr(skips, *str))  // Note: 0 is not a space symbol
		++str;
	return *str;
}

//! \brief Validate parsed value
//! Checks that the value is a valid ResT instance
//! \pre ValT is an arithmetic type
//!
//! \param val VatT  - the value to be validated
//! \param str=nullptr const char*  - the parsed text to be shown in case of the error
//! \param invalidate=false bool  - forced invalidation condition of the parsed value
//! \param invalmsg=nullptr const char*  - invalidation message (without the ending '\n')
//! \return void
template <typename ResT, typename VatT>
void validateVal(VatT val, const char* str=nullptr, bool invalidate=false
	, const char* invalmsg=nullptr)
{
	static_assert(is_arithmetic<VatT>::value, "validateVal(), ValT should be an arithmetic type");

	if(invalidate || (!is_same<ResT, VatT>::value
	&& (val > numeric_limits<ResT>::max() || val < numeric_limits<ResT>::lowest()))) {
		// Define length of the displaying str fragment
		unsigned len = 0;
		if(str) {
			// Note: +2 (+1 for the cut value, +1 for \n), *2 to have additional context
			constexpr unsigned maxlen = (numeric_limits<ResT>::digits10 + 2) * 2;
			while(str[len] && ++len < maxlen);  // ATTENTION: the cycle body is intentionally empty
		}

		// Form the output message
		string  msg("Invalid format of val or out of range conversion of '");
		if(str)
			msg.append(str, 0, len).append("' with the value = ") += std::to_string(val);
		if(invalmsg)
			msg.append(": ") += invalmsg;
		throw invalid_argument(msg += '\n');
	}
}

//! \brief Parse and validate a value
//! \pre ResT is an arithmetic type,
//! 	str should be a valid non-empty string starting from the non-space symbol
//!
//! \tparam ResT  - required type of the parsed value
//! \tparam StrToValF  - parsing function in the format
//! 	ValT strToVal(const char *str, char **str_end, int base)  - for integral types
//! 	ValT strToVal(const char *str, char **str_end)  - for floating point types
//! \tparam InvalF  - type of the validation function in the format:
//! 	bool (*)(ResT val, char end);
//! 	The corresponding function defines whether the parsed value is invalid.
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
template <typename ResT, typename StrToValF, typename InvalF=bool (*)(ResT val, char end)>
inline enable_if_t<is_integral<ResT>::value, ResT>
parseVal(char*& str, StrToValF strToVal, InvalF invalid=nullptr
	, const char* invalmsg=nullptr, int base=10)
// bool (*invalid)(ResT val, char end)
{
#if VALIDATE >= 2
	assert(str && *str && !isspace(*str)  // Note: 0 is not a space symbol
		&& "parseVal_integral(), str is expected to be a valid string starting from the non-space symbol");
#endif // VALIDATE

	//fprintf(ftrace, "Initial str: %s\n", str);
	char *pose = str;
	errno = 0;
	const auto val = strToVal(str, &pose, base);  // Base: 10 - decimal value
	if(errno)
		invalmsg = strerror(errno);
	const bool invalidate = errno || (invalid && (*invalid)(val, *pose));
	validateVal<ResT>(val, str, invalidate, invalmsg);
	str = pose;
	//fprintf(ftrace, "Resulting str: %s\n", str);
	return val;
}

//! \brief Parse and validate a value
//! \pre ResT is an arithmetic type,
//! 	str should be a valid non-empty string starting from the non-space symbol
//!
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
template <typename ResT, typename StrToValF, typename InvalF=bool (*)(ResT val, char end)>
inline enable_if_t<is_floating_point<ResT>::value, ResT>
parseVal(char*& str, StrToValF strToVal, InvalF invalid=nullptr, const char* invalmsg=nullptr)
//bool (*invalid)(ResT val, char end)
{
#if VALIDATE >= 2
	assert(str && *str && !isspace(*str)  // Note: 0 is not a space symbol
		&& "parseVal_floating(), str is expected to be a valid string starting from the non-space symbol");
#endif // VALIDATE

	char *pose = str;
	errno = 0;
	const auto val = strToVal(str, &pose);
	if(errno)
		invalmsg = strerror(errno);
	const bool invalidate = errno || (invalid && (*invalid)(val, *pose));
	validateVal<ResT>(val, str, invalidate, invalmsg);
	str = pose;
	return val;
}

//! \brief Lower case and skip the token from the str if it matches the specified tok
//! \post str pointer can be updated, but the str content is not modified
//!
//! \param str char*&  - the input string to be updated to the position following
//! 	the terminating symbol if the tok is matched
//! \param tok const char*  - the control token
//! \param tend=" \t" const char*  - terminating symbol following the token,
//! 	0 (EOL) is always included for the non-null string
//! \return bool  - whether the token is matched and skipped from the string
////! \brief Skip the converted token from the str if it matches the specified tok
////! \param convf=tolower int (*)(int)  - converting function by symbols. Default: tolower()
//inline bool skipConvTok(char*& str, const char* tok, const char tend=0, int (*convf)(int)=tolower)
inline bool lowerAndSkip(char*& str, const char* tok, const char* tend=" \t")
{
#if VALIDATE >= 2
	assert(str && *str && tok  // Note: 0 is not a space symbol
		&& "str is expected to be a valid string starting from the non-space symbol");
#endif // VALIDATE

	char *const  stok = str;
	while(*str && *tok == tolower(*str)) {
#if VALIDATE >= 2
		assert(*tok == tolower(*tok) && "lowerAndSkip(), the tok should be a lowercase word");
#endif // VALIDATE
		++str;
		++tok;
	}
	if(!*tok && (!tend || strchr(tend, *str++)))  // Update str to the position following the terminator
		return true;
	// Reset str to the begin
	str = stok;
	return false;
}

}  // daoc

#endif // PARSERS_H
