//! \brief Basic arithmetic for the integral numbers considering overflows and non fundamental types.
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! > 	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2018-02-07

#ifndef ARITHMETIC_HPP
#define ARITHMETIC_HPP

// MACROSES:
//  - UTEST  - make some internal class members public for the unit testing
//	- MACRODEF_H  - do not include 'macrodef.h' if MACRODEF_H is defined
// Usually, should not be touched ----------------------------------------------
//  - CPU_LITTLE_ENDIAN {0,1}  - use the big/little-endian bytes order instead of the automatic identification.
//  	ATTENTNION: results incorrect evaluations on the little-endian architecture.

#include <cstdint>  // uintX_t
#include <type_traits>  // is_scalar, is_integral, enable_if_t

#ifdef _MSC_VER  // MSVC
	#include <cstdlib>  // _byteswap_xxx
#endif // _MSC_VER

#if VALIDATE >= 2
	#include <stdexcept>  // logic_error
	#include <cassert>
#endif // VALIDATE

#ifndef MACRODEF_H  // Note: allows to omit the file by the macro specification
	#include "macrodef.h"  // VALIDATE, TRACE
#endif // MACRODEF_H


// Macro Definitions -----------------------------------------------------------
// Identify endianess, which affects hashing and arithmetics.
#ifndef CPU_LITTLE_ENDIAN
	#ifdef __BYTE_ORDER__
		#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			#define CPU_LITTLE_ENDIAN  1
		#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			#define CPU_LITTLE_ENDIAN  0
		#else
			#error "Unsupported byte order"
		#endif // __BYTE_ORDER__
	#else
		#include <endian.h>
		#ifdef __BYTE_ORDER
			#if __BYTE_ORDER == __LITTLE_ENDIAN
				#define CPU_LITTLE_ENDIAN  1
			#elif __BYTE_ORDER == __BIG_ENDIAN
				#define CPU_LITTLE_ENDIAN  0
			#else
				#error "Unsupported byte order"
			#endif // __BYTE_ORDER
		#else
			// Manual identification of the endianess
			#define ORDER_LITTLE_ENDIAN  0x01020304U
			#define ORDER_BIG_ENDIAN  0x04030201U
			//#define ORDER_PDP_ENDIAN    0x42414443UL
			#define ORDER_ENDIAN_TEST  ('\1\2\3\4')  // -> "4321" on CPU_LITTLE_ENDIAN

			#if ORDER_ENDIAN_TEST == ORDER_LITTLE_ENDIAN
				#define CPU_LITTLE_ENDIAN  1
			#elif ORDER_ENDIAN_TEST == ORDER_BIG_ENDIAN
				#define CPU_LITTLE_ENDIAN  0
			#else
				#error "Unsupported byte order"
			#endif // ORDER_ENDIAN_TEST

			#undef ORDER_ENDIAN_TEST
			#undef ORDER_BIG_ENDIAN
			#undef ORDER_LITTLE_ENDIAN
		#endif // __BYTE_ORDER
	#endif // __BYTE_ORDER__
#endif // CPU_LITTLE_ENDIAN
#if TRACE >= 2 && !CPU_LITTLE_ENDIAN
	#warning "Compiling for the big-endian architecture."
#endif // TRACE


namespace daoc {

using std::conditional_t;
using std::is_scalar;
using std::is_integral;
using std::is_signed;
using std::is_unsigned;
using std::make_unsigned_t;
using std::make_signed_t;
using std::enable_if_t;
#if VALIDATE >= 2
using std::logic_error;  // Note implemented
#endif // VALIDATE 2


// Meta Definitions of Base Types ---------------------------------------------
//! \brief Type T by value or const reference depending of the type size
// Note: sizeof(T&) == sizeof(T)
template <typename T>
using ValCRef = conditional_t<sizeof(T) <= sizeof(void*) || is_scalar<T>::value, T, const T&>;

//! \brief Member type by value or const reference depending of the type size
template <typename ItemsT>
using MemberValCRef = ValCRef<typename ItemsT::value_type>;

// Accumulating Type -----------------------------------------------------------
//! \brief Accumulated Integral Type for the fundamental integral types
//! Contains .high and .low parts allowing to perform arithmetics of numbers larger
//! than the machine word.
//! \note The signed value is processed in the implementation (compiler) defined way.
//! Operations rely on the bit shift operation, which is undefined by C++ standard
//! for the (signed) negative numbers. The arithmetic shift is performed in most
//! implementations retaining the sign for the rsh and the logical shift (the sign
//! is overwritten) for lsh.
template <typename ValT>
class AccInt {
public:
	// Note: for the floating point types the implementation should differ and can't
	// include bisection on high and low halves.
	static_assert(is_integral<ValT>::value, "AccInt, ValT type is invalid");
	using Value = ValT;
	using UValue = make_unsigned_t<Value>;

	// Size of the half AccInt (i.e. of Value) in bits
	constexpr static uint8_t  hszbits = sizeof(AccInt) << 2;
	// The right shift is arithmetic and carries the sign (platform dependent, typically yes)
	constexpr static bool  rsharithm = -2 >> 1 < 0;

	// Note: public access specifier is required for the testing
#ifndef UTEST
protected:
#endif // UTEST
    //! \brief Signed mask for the specified value
    //!
    //! \param v IntT  - processing value
    //! \return size_t  - resulting mask: 0 or -1 (0xff...f)
	template <typename IntT>
	constexpr static size_t signedMask(IntT v) noexcept
	{
		static_assert(is_integral<IntT>::value && sizeof(IntT) <= sizeof(AccInt)
			, "highPart<IntT>(), unexpected value type");
		return is_signed<IntT>::value && rsharithm && v < 0 ? -1 : 0;
	}

    //! \brief Identify .high value for the specified argument
    //! \note The signed value is processed in the implementation (compiler) defined way.
    //! The AccInt formation relies on the value shift, which is undefined
    //! by C++ standard for the (signed) negative numbers, the arithmetic shift is
    //! performed in most implementations retaining the sign for the rsh and the standard
    //! shift (the sign is overwritten) for lsh.
    //!
    //! \param v IntT - the argument to be processed
    //! \return Value  - the resulting .high part
	template <typename IntT>
	constexpr static Value highPart(IntT v) noexcept
	{
		static_assert(is_integral<IntT>::value && sizeof(IntT) <= sizeof(AccInt)
			&& sizeof(UValue) << 1 == sizeof(AccInt), "highPart<IntT>(), unexpected value type");
		// Fill with the reminder of the low part
		return sizeof(IntT) > sizeof(UValue) ? v >> hszbits : signedMask(v);
	}
public:
#if CPU_LITTLE_ENDIAN
	UValue low;  //!< Low ValT half of the AccInt
	Value high;  //!< Hight ValT half of the AccInt
#else
	Value high;  //!< Hight ValT half of the AccInt
	UValue low;  //!< Low ValT half of the AccInt
#endif // CPU_LITTLE_ENDIAN
	static_assert(is_unsigned<decltype(low)>::value, "AccInt, unexpected type of .low");

    //! \brief Default constructor
    //! \note Always initializes members to zero
    // Note: an implicit default constructor would not initialize POD types on stack
	AccInt() noexcept: low(0), high(0)  {}

    //! \brief Aggregating constructor
    //!
    //! \param high Value  - high part (possibly signed)
    //! \param low UValue  - low part (always unsigned)
	AccInt(Value high, UValue low) noexcept:  low(low), high(high)  {}

    //! \brief Copy constructor
    //!
    //! \param other const AccInt&  - the value to be copied
	AccInt(const AccInt& other)=default;

    //! \brief Move constructor
    //!
    //! \param other AccInt&&  - the value to be moved
	AccInt(AccInt&& other)=default;

    //! \brief Copy constructor
    //!
    //! \param v Value  - the value to be copied
    template <typename IntT>
	AccInt(IntT v) noexcept: low(v), high(highPart(v))  {}

    //! \brief Copy assignment operator
    //!
    //! \param other const AccInt&  - the value to be assigned
    //! \return AccInt&  - assigned value
	AccInt& operator =(const AccInt& other)=default;

    //! \brief Move assignment operator
    //!
    //! \param other AccInt&&  - the value to be moved
    //! \return AccInt&  - assigned value
	AccInt& operator =(AccInt&& other)=default;

    //! \brief Copy assignment operator
    //!
    //! \param v IntT  - the value to be assigned
    //! \return AccInt&  - assigned value
    template <typename IntT>
    AccInt& operator =(IntT v)
    {
		low = v;
		high = highPart(v);

		return *this;
    }

    //! \brief Reset the value
    //!
    //! \return void
	void clear() noexcept
	{
		low = 0;
		high = 0;
	}

    //! \brief The value is empty
    //!
    //! \return bool
	bool empty() const noexcept  { return !low && !high; }

    //! \brief Assignment xor operator
    //!
    //! \param v IntT  - the value to be xor assigned
    //! \return AccInt&  - resulting value
    template <typename IntT>
    AccInt& operator ^=(IntT v)
    {
		static_assert(is_integral<IntT>::value && sizeof(IntT) <= sizeof(AccInt)
			, "operator=<IntT>(), unexpected value type");
		low ^= v;
		high ^= highPart(v);

		return *this;
	}

    //! \brief Assignment left shit
    //!
    //! \param nbits uint8_t  - the number of shifting bits
    //! \return AccInt&  - resulting value
    AccInt& operator <<=(uint8_t nbits)
    {
#define TRACE_ALSH_  (TRACE >= 3)
#if TRACE_ALSH_
		fprintf(stderr, "  > <<=() initial, high: %#llx, low: %#llx;  nbits: %u, hszbits: %u\n"
			, high, low, nbits, hszbits);
#endif // TRACE
		// ATTENTION: this condition is required otherwise the result is undefined
		// for the out of range (too large) shift
		if(nbits < hszbits)
			high <<= nbits;
		else high = 0;
		// Consider low part reminder
		if(nbits < hszbits << 1)  // The number of bits in the AccInt
			high |= nbits <= hszbits ? low >> (hszbits - nbits) : low << (nbits - hszbits);
#if TRACE_ALSH_
		const Value  hpart = nbits <= hszbits ? low >> (hszbits - nbits) : low << (nbits - hszbits);
#endif // TRACE
		// ATTENTION: this condition is required otherwise the result is undefined
		// for the out of range (too large) shift
		if(nbits < hszbits)
			low <<= nbits;
		else low = 0;
#if TRACE_ALSH_
		fprintf(stderr, "  > <<=(), high: %#llx (hpart: %#llx, high | hpart: %#llx), low: %#llx\n"
			, high, hpart, high | hpart, low);
#endif // TRACE

		return *this;
#undef TRACE_ALSH_
	}

    //! \brief Assignment right shit
    //! \note The platform-defined shift is performed, which is typically arithmetic
    //! right shift caring the sign.
    //!
    //! \param nbits uint8_t  - the number of shifting bits
    //! \return AccInt&  - resulting value
    AccInt& operator >>=(uint8_t nbits)
    {
#define TRACE_ARSH_  (TRACE >= 3)
#if TRACE_ARSH_
		fprintf(stderr, "  > >>=() initial, high: %#llx, low: %#llx;  nbits: %u, hszbits: %u\n"
			, high, low, nbits, hszbits);
#endif // TRACE
		// ATTENTION: this condition is required otherwise the result is undefined
		// for the out of range (too large) shift
		if(nbits < hszbits)
			low >>= nbits;
		else low = 0;
#if TRACE_ARSH_
		const UValue  lpart = nbits <= hszbits ? high << (hszbits - nbits) : high >> (nbits - hszbits);
#endif // TRACE
		// Consider high part reminder
		if(nbits < hszbits << 1) {  // The number of bits in the AccInt
			low |= nbits <= hszbits ? high << (hszbits - nbits) : high >> (nbits - hszbits);
			// Shift high part
			if(nbits < hszbits)
				high >>= nbits;
			else high = signedMask(high);  // ATTENTNION: v should not be casted on the signedMask() call
		} else high = 0;
#if TRACE_ARSH_
		fprintf(stderr, "  > >>=(), high: %#llx, low: %#llx (lpart: %#llx, low | lpart: %#llx)\n"
			, high, low, lpart, low | lpart);
#endif // TRACE

		return *this;
#undef TRACE_ARSH_
	}

    //! \brief Implicitly convert to the specified integral type taking the lower bytes
    //!
    //! \return operator IntT()
    template <typename IntT>
    explicit operator IntT() const noexcept
    {
		static_assert(is_integral<IntT>::value, "IntT(), integral type is expected");
		// Set the sign bit if any
		IntT  res(sizeof(IntT) < sizeof(AccInt)
			? reinterpret_cast<const IntT&>(
#if CPU_LITTLE_ENDIAN
				*this
#else
				// Discard redundant higher part for the big endian
				AccInt(*this) <<= ((sizeof(AccInt) - sizeof(IntT)) << 3)
#endif  // CPU_LITTLE_ENDIAN
			// Note: -1 is required to fill correctly the .high part with 0xf..f
			) : signedMask(high)  // ATTENTNION: v should not be casted on the signedMask() call
		);

		if(sizeof(IntT) >= sizeof(AccInt)) {
			memcpy(reinterpret_cast<uint8_t*>(&res)
#if !CPU_LITTLE_ENDIAN
			 + sizeof(AccInt) - sizeof(IntT)
#endif  // !CPU_LITTLE_ENDIAN
			, this, sizeof *this);
		}

		return res;
    }

    //! \brief Equality operator
    //!
    //! \param other const AccInt&  - the value to be compared
    //! \return bool operator  - the value is equal
    bool operator ==(const AccInt& other) const noexcept
    {
		return low == other.low && high == other.high;
	}

    //! \brief Equality operator with the integral type
    //!
    //! \param v IntT  - the value to be compared
    //! \return bool  - the value is equal
    template <typename IntT>
    bool operator ==(IntT v) const noexcept
    {
		static_assert(is_integral<IntT>::value, "IntT(), integral type is expected");
		return sizeof(IntT) <= sizeof(AccInt)
			? sizeof(IntT) < sizeof(Value)
				// ATTENTNION: v should not be casted on the signedMask() call
				? low == v && high == signedMask(v)
				: low == static_cast<UValue>(v) && high == v >> hszbits
			: v == static_cast<IntT>(*this);
    }
};

//using AUInt64 = AccInt<uint64_t>;

// Doubled Integral Fundamental Type -------------------------------------------
template <typename IntT>
struct DoubledInt {
	static_assert(is_integral<IntT>::value && sizeof(IntT) << 1 <= sizeof(void*)
		, "DoubledInt, the input type either is not integral or too large");
	static_assert(is_signed<IntT>::value, "DoubledInt, the UIntT specialization is not implemented");
	using type = make_signed_t<typename DoubledInt<make_unsigned_t<IntT>>::type>;
};

template <>
struct DoubledInt<uint8_t> {
	using type = uint16_t;
};

template <>
struct DoubledInt<uint16_t> {
	using type = uint32_t;
};

template <>
struct DoubledInt<uint32_t> {
	using type = uint64_t;
};

template <typename IntT>
using DoubledIntT = typename DoubledInt<IntT>::type;

// Arithmetic Operations -------------------------------------------------------
//! \brief Sum of the unsigned integral numbers considering the overflow (carry flag)
//!
//! \param sum UValT&  - resulting sum
//! \param v ValCRef<UValT>  - the value to be added
//! \return bool  - carry flag (overflow), 0 or 1
template <typename UValT>
inline enable_if_t<is_integral<UValT>::value, bool>
csum(UValT& sum, ValCRef<UValT> v) noexcept
{
	static_assert(is_integral<UValT>::value && is_unsigned<UValT>::value
		, "csum(), unsigned integral value is expected of the fundamental type");
	sum += v;
	return sum < v;
}

template <typename AccUIntT>
enable_if_t<!is_integral<AccUIntT>::value, bool>
csum(AccUIntT& sum, ValCRef<AccUIntT> v) noexcept
{
	static_assert(is_integral<typename AccUIntT::Value>::value
		&& is_unsigned<typename AccUIntT::Value>::value
		, "csum(), AccInt of the unsigned integral is expected");
	const bool ovf = csum(sum.low, v.low);
	sum.high += ovf;  // Note: the overflow is possible here, which is considered below
	return csum(sum.high, v.high) || (ovf && sum.high == v.high);
}

//! \brief Square of the value
//!
//! \param v ValT  - the value to be squared
//! \return AccInt<ValT>  - the squared value
template <typename ValT>
static AccInt<make_unsigned_t<ValT>> square(ValT v) noexcept
{
	static_assert(is_integral<ValT>::value && sizeof(ValT) <= sizeof(uint32_t)
		, "sum(), unsigned integral value is expected of the fundamental type");
	return static_cast<DoubledIntT<ValT>>(v) * v;
}

template <>
AccInt<uint64_t> square<uint64_t>(uint64_t v) noexcept
{
	AccInt<uint64_t>  res;  // Use NRVO optimization for the return value

	// cf(mulm_lsh) + cf(mulh)| ab^2 = a^2 << 2*sz + cf(mulm) | 2ab << sz + cf(mull) | b^2
	//
	// Represent uint64_t and high and low uint32_t parts
	AccInt<uint32_t>  av(v);
	// Low value parts multiplication
	const uint64_t  lmul = static_cast<uint64_t>(av.low) * av.low;  // Low carry (32bit), low mul (32 bit)

	// Doubled inter multiplication of the high/low value parts
    uint64_t  mmul = static_cast<uint64_t>(av.high) * av.low;  // Median carry (32bit), median mul (32 bit)
    // Half of the input value size in bits
    constexpr uint8_t  halfvb = sizeof(uint64_t) << 2;
    // Overflow / carry flag goes to the high quarter of the result
    // Note: hcf is uint64_t to shift it correctly and align with .high
    uint64_t  hcf = (static_cast<int64_t>(mmul) < 0);
    mmul <<= 1;  // Doubled middle mul
	// Carry high part from the lmul
    hcf += csum(mmul, lmul >> halfvb);
#if VALIDATE >= 2
    assert(hcf <= 2 && "square<uint64_t>(), too high hcf");
#endif // VALIDATE

	// Fill low part of the result
	res.low = (mmul << halfvb) | static_cast<uint32_t>(lmul);

	// High value parts multiplication
	res.high = static_cast<uint64_t>(av.high) * av.high;
#if VALIDATE >= 2
	// Note: squaring should never cause overflow
	if(csum(res.high, mmul >> halfvb))
		throw logic_error("square<uint64_t>(), unexpected overflow mmul\n");
	if(hcf && csum(res.high, hcf << halfvb))
		throw logic_error("square<uint64_t>(), unexpected overflow hcf\n");
#else
	res.high += (mmul >> halfvb) + (hcf << halfvb);
#endif // VALIDATE

#if TRACE >= 3
	fprintf(stderr, "  > square(), high: %#llx, low: %#llx"
		";  hcf: %#llx, mmul: %#llx, halfvb: %u, lmul: %#llx\n"
		, res.high, res.low, hcf, mmul, halfvb, lmul);
#endif // TRACE

	return res;
}

template <>
AccInt<uint64_t> square<int64_t>(int64_t v) noexcept
{
	return square<uint64_t>(v);
}

//! \brief Xor of the res with the left shifted value
//!
//! \param res AccIntT&  - updating operand of type AccInt
//! \param val UValT  - shifting operand
//! \param nbits uint8_t - shift size in bits
//! \return void
template <typename AccIntT, typename UValT>
void xorlsh(AccIntT& res, UValT val, uint8_t nbits)
{
	static_assert(is_integral<UValT>::value && is_unsigned<UValT>::value
		&& sizeof(UValT) <= sizeof(AccIntT), "xorlsh(), input types are invalid");
	// ATTENTION: if is require because the out of range (too large) shift is not deterministic
	if(nbits < AccIntT::hszbits)
		res.low ^= static_cast<typename AccIntT::UValue>(val) << nbits;
	// Xor high half of the res with the remained part of the shifted val
	// The lost part of the shifted val is:
	// val_bits - (nbits + val_bits - uint_bits) = uint_bits - nbits
	constexpr auto  uvbits = sizeof(UValT) << 3;
	// Note: Size of AccIntT in bits is AccIntT::hszbits << 1
	if(nbits < AccIntT::hszbits << 1 && uvbits + nbits > AccIntT::hszbits) {
#if TRACE >= 3
		fprintf(stderr, "  > xorlsh() initial, high: %#llx, hpart: %#llx\n"
			, res.high, nbits < AccIntT::hszbits ? val >> (AccIntT::hszbits - nbits)
				: static_cast<typename AccIntT::UValue>(val) << (nbits - AccIntT::hszbits));
#endif // TRACE
		res.high ^= nbits < AccIntT::hszbits ? val >> (AccIntT::hszbits - nbits)
			: static_cast<typename AccIntT::UValue>(val) << (nbits - AccIntT::hszbits);
	}
#if TRACE >= 3
	fprintf(stderr, "  > xorlsh(), high: %#llx, low: %#llx;  hszbits: %u, uvbits: %u, nbits: %u\n"
		, res.high, res.low, AccIntT::hszbits, uvbits, nbits);
#endif // TRACE
}

//! \brief Reverse bytes of the integral value
//!
//! \param v IntT  - the processing value
//! \return IntT  - result of the bytes reversing
template <typename IntT>
static inline IntT rbytes(IntT v)
{
	static_assert(is_integral<IntT>::value && sizeof(IntT) == 1
		, "rbytes(), the integral type is expected");
	if(sizeof(IntT) >= 2 && is_signed<IntT>::value)
		return rbytes<make_unsigned_t<IntT>>(v);

	static_assert(sizeof(IntT) == 1, "rbytes(), the specialization is not implemented");
#if VALIDATE >= 2
	assert(0 && "rbytes(), redundant call: nothing to reverse");
#endif // VALIDATE
	return v;
}

template <>
uint16_t rbytes<uint16_t>(uint16_t v)
{
	return
#ifdef __GNUC__  // GCC or clang
		__builtin_bswap16(v)
#elif defined(_MSC_VER)  // MSVC
		_byteswap_ushort(v)
#else
		v << 8 | v >> 8
#endif // C++ Compilers
	;
}

template <>
uint32_t rbytes<uint32_t>(uint32_t v)
{
	return
#ifdef __GNUC__  // GCC or clang
		__builtin_bswap32(v)
#elif defined(_MSC_VER)  // MSVC
		_byteswap_ulong(v)
#else
		(v << 24 & 0xff000000) |
		(v <<  8 & 0x00ff0000) |
		(v >>  8 & 0x0000ff00) |
		(v >> 24 & 0x000000ff)
#endif // C++ Compilers
	;
}

template <>
uint64_t rbytes<uint64_t>(uint64_t v)
{
	return
#ifdef __GNUC__  // GCC or clang
		__builtin_bswap64(v)
#elif defined(_MSC_VER)  // MSVC
		_byteswap_uint64(v)
#else
		(v << 56 & 0xff00000000000000ULL) |
		(v << 40 & 0x00ff000000000000ULL) |
		(v << 24 & 0x0000ff0000000000ULL) |
		(v << 8  & 0x000000ff00000000ULL) |
		(v >> 8  & 0x00000000ff000000ULL) |
		(v >> 24 & 0x0000000000ff0000ULL) |
		(v >> 40 & 0x000000000000ff00ULL) |
		(v >> 56 & 0x00000000000000ffULL)
#endif // C++ Compilers
	;
}

}  // daoc

#endif // ARITHMETIC_HPP
