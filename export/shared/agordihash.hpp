//! \brief Aggregating Order Invariant (History Independent) Hashing for sets of objects.
//! The DAO (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! > 	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \company eXascale InfoLab (https://exascale.info/), Lumais (http://www.lumais.com)
//! \date 2017-02-21 (special case), 2018-02-09 (full version)

#ifndef AGORDIHASH_HPP
#define AGORDIHASH_HPP

// MACROSES:
//  - UTEST  - make some internal class members public for the unit testing
// Usually, should not be touched ----------------------------------------------
//  - CPU_LITTLE_ENDIAN {0,1}  - use the big/little-endian bytes order instead of the automatic identification.
//  	ATTENTNION: results incorrect evaluations on the little-endian architecture.
// =============================================================================
// ATTENTION:
// 1. Size should be considered either inside the hash or independently
// 2. Item should be limited from the bottom to the sqrt(max) otherwise collisions
// possible as shown below.
// 3. Arguably there should not be any collisions if the bottom boundary is
// considered, but there is no formal prove for this statement.
//
// Counter examples (in case the bottom boundary n.2 is violated):
//  S2   S   Set           Evaluation
// 150  18	 1,| 7, 10     49 - 25 - 4 = 20  (7 = 2 + 5)
// 150  18   2,| 5, 11     121 - 100 - 1 = 20  (11 = 10 + 1)
// -----------------------------------------------------------------------------
// 174  24   3,| 4, 7, 10  100 - (64 + 25 + 4) = 7
// 174  24   2,| 5, 8, 9   81 - (49 + 16 + 9) = 7
// =============================================================================
// Note: it is possible to perform also collision-free digital signing, which
// requires consideration of the items (signs) order. The order can be considered
// by enumerating positions of each item and hashing each item together with the
// position id. It makes sense to put position id to the low bytes to use some
// CPU register as a counter starting from the required bottom boundary (n.2).

//#include <string>  // uintX_t
//#include <functional>  // hash
//#include <cstddef>  // size_t
#include <cstring>  // memset, memcmp
#include <array>
#include <vector>
#include <stdexcept>  // logic_error
#include <limits>  // numeric_limits

#include "arithmetic.hpp"  // csum, square, VALIDATE


namespace daoc {

using std::string;
using std::array;
using std::vector;
using std::numeric_limits;
using std::logic_error;  // Note implemented
#if VALIDATE >= 1
using std::overflow_error;
using std::domain_error;
#if VALIDATE >= 2
using std::underflow_error;
#endif // VALIDATE 2
#endif // VALIDATE


// AgordiHash flags ------------------------------------------------------------
//! Base type for the HashItemCorr flag
using HashItemCorrBase = uint8_t;

//! HashItemCorr flag
//! Correction strategy of the hashing items to prevent collisions
enum class HashItemCorr: HashItemCorrBase {
	//! Do not correct the hashing items, useful for the fast processing of memory
	//! addresses in the user space (> 64 KB) if nullptr/0 NEVER occurs in the input
	NONE = 0,
	//! Do not correct the hashing items, useful for the fast processing of memory
	//! addresses in the user space (> 64 KB) if nullptr/0 may occur in the input
	COR0 = 0b001,
	//! Correct the hashing items to avoid collisions
	//! \attention The correction reduces upper range of the items to MAX - sqrt(MAX)
	CORALL = 0b011,
	//! Do not correct the hashing items, useful for the fast processing of memory
	//! addresses in the user space (> 64 KB) if nullptr/0 may occur in the input
	VLD0 = 0b100,
	//! Validate that the hashing items belong to the range, which does not require
	//! the items correction
	VLDALL = 0b110,  // Useful for debugging
	//!< Correct the hashing items and validate that corrected value belongs to the
	//!< valid range (does not exceed the upper bound)
	CORVLD = 0b111,  // Useful for debugging
//	WARNING: the adjustment could lead to collisions with longer set containing the items
//	equal to the result of the adjustment.
//	//!< Correct the hashing items automatically handling the case when the corrected
//	//!< value exceeds the upper bound (the item is split into two virtual items)
//	CORADJ = 7,
	// Masks -----
	MASK_CORANY = 0b001,  //!< Any correction is applied
	MASK_CORALL = 0b011,  //!< Correction of all items is applied
	MASK_VLDANY = 0b100  //!< Any validation is applied
};

//! \brief Bitwise & for HashItemCorr
//! \relates HashItemCorr
//!
//! \param self HashItemCorr  - flag to be checked
//! \param flag HashItemCorr  - operating flag
//! \return HashItemCorrBase operator  - resulting flag
constexpr HashItemCorrBase operator &(HashItemCorr self, HashItemCorr flag)
{
	return static_cast<HashItemCorrBase>(self) & static_cast<HashItemCorrBase>(flag);
}

//! \brief HashItemCorr flag matching to the mask
//!
//! \param flag HashItemCorr  - the flag to be matched
//! \param mask HashItemCorr  - the matching mask
//! \return constexpr bool  - the flag matches the mask
constexpr bool matches(HashItemCorr flag, HashItemCorr mask)
{
	return (static_cast<HashItemCorrBase>(flag) & static_cast<HashItemCorrBase>(mask))
		== static_cast<HashItemCorrBase>(mask);
}

// AgordiHash flags ------------------------------------------------------------
//! \brief Aggregating Order Invariant Hashing for the sets of objects (former AggHash).
//! 	This has function is applicable to the sets of fixed size items
//! 	like sets of ids to be matched or signed.
//! 	The hashing is items order invariant, incremental (and reversible),
//! 	parallelizable, exact, and strictly history independent.
//! 	It is a kind of polynomial-based unordered chain hashing / signing.
//! 	All API functions are executed in const time, the execution speed is limited
//! 	by the memory (RAM) reading speed on CISC architectures.
//! \note The aggregating hashing is likely to be (is, but has not been formally proved)
//! 	collision-free if either the id correction is applied or each processing ids >=
//! 	sqrt(ID_MAX) and the size of distinct hashes is separately matched in advance
//! 	(formally, a part of the extended hash, which is matched manually).
//!
//! 	Works for both little and big endian architecture. Explanation of some properties:
//! 	- INCREMENTAL: if a message x which was hashed is modified to x0 then rather than
//! 	having to re-compute the hash of x0 from scratch, a quick "update" of the old hash
//! 	value to the new one can be made in the time proportional to the amount of the made
//! 	modification in x to get x0. https://cseweb.ucsd.edu/~mihir/papers/inchash.pdf
//! 	- Strictly history independent: the goal of history independence is to design
//! 	a data structure so that an adversary who examines the computer memory will
//!		only discover the current contents of the data structure but will not learn
//! 	anything about the sequence of operations that led to the current state of
//!		the data structure). https://eprint.iacr.org/2016/134.pdf
//! \pre Template types should be fundamental, integral and unsigned
//!
//! \tparam UInt  - unsigned integral fundamental type of the processing items
//! \tparam Size  - unsigned integral fundamental type of the number of processing items
//! \tparam CORR  - correction strategy of the hashing items to prevent collisions
// The old notation to be refactored:
// template <typename Id=uint32_t, typename AccId=uint64_t> class AggHash;
template <typename UIntT=uint32_t, typename SizeT=uint32_t
	, HashItemCorr CORR=HashItemCorr::CORVLD>
class AgordiHash {  // AgoriHash  Agordi
	static_assert(is_integral<UIntT>::value && is_integral<SizeT>::value
		// Note: signed values affect the sum but can be gracefully processed as unsigned
		&& is_unsigned<UIntT>::value && is_unsigned<SizeT>::value
		//&& sizeof(SizeT) >= 2*sizeof(UIntT)
		, "AgordiHash, types constraints are violated");
public:
	// Export the internal types
	using Size = SizeT;  //!< Type of the number of the member items
	using UInt = UIntT;  //!< Type of the member item
	using AccUInt = AccInt<UInt>;  //!< Type of the member items squares
	//using Data = array<uint8_t, sizeof(AgordiHash)>;  //!< Internal data in the native endian format
	using Data = vector<uint8_t>;  //!< Internal data in the native endian format

	static const Size  sizeMax = numeric_limits<Size>::max();  // Max number of items
#ifndef UTEST  // Allow access to the internal state variables for the unit tests
protected:
#endif // UTEST
	//! Zero substitution value
	constexpr static UInt  zsval = static_cast<UInt>(-1) << (sizeof(UInt) << 2);
	//! Correction value
	constexpr static UInt  corval = sqrt(numeric_limits<UInt>::max());
private:
	// Note: size should be first as the most discriminative attribute, which
	// can be potentially used for the ordering
	// Note: the m_size is redundant and does not have any impact except for the structured
	// ordering if the sum does not exceed UInt_MAX or if the zero value of id is NOT allowed.
	// To omit m_size either 0 value should be prohibited in the input (or it can be used as
	// a stream termination flag) or 0 can be interpreted as {sum = 0, 2sum = 0xFF..F00..0}.
//	Size  m_size;  //!< Size of the processed stream (the number of hashed items)
	// ATTENTION: lsum should be the FIRST field for the fast comparison operations if m_size field absent
	UInt  m_lsum;  //!< Low part of the sum of the member items
	Size  m_hsum;  //!< High part of the sum of the member items
	Size  m_hv2sum;  //!< High part of the sum of the member items squares
	AccUInt  m_lv2sum;  //!< Low part of the sum of the member items squares
public:
    //! \brief Default constructor
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
	AgordiHash() noexcept
	//: m_size(0), m_hsum(0), m_lsum(0), m_hv2sum(0), m_lv2sum(0)
	// ATTENTION: clear() also resets the alignment padding bytes, which is
	// required for the hashing and comparisons.
	{ clear(); }
#pragma GCC diagnostic pop

    //! \brief Clear the hash
    //! \post All fields are reseted and the alignment pudding bytes are filled with zero
    //!
    //! \return void
	// ATTENTION: memset is essential to fill also the alignment padding with zero,
	// which is required for the deterministic results of hash() and comparisons.
	void clear() noexcept { memset(this, 0, sizeof *this); }

    //! \brief Add item to the hashing
    //! \attention Exception can be thrown if values correction (except COR0) is used
    //! 	or validation is applied, which will cause crash of the whole app
    //! 	since the function declared as noexcept.
    //!
    //! \param v UInt  - v to be included to the hashing
    //! \return void
	void add(UInt v) noexcept;

    //! \brief Add pointer to the hashing
    //!
    //! \param v const void*  - v to be included to the hashing
    //! \return void
	template <bool IsPtrSmall=sizeof(void*) <= sizeof(UInt)>
	enable_if_t<IsPtrSmall> add(const void* v) noexcept  { add(reinterpret_cast<UInt>(v)); }

//	template <bool IsPtrSmall=sizeof(void*) <= sizeof(UInt)>
//	enable_if_t<!IsPtrSmall> add(const void* v) noexcept;

    //! \brief Add item to the hashing
    //!
    //! \param v UInt  - v to be included into the hash
    //! \return AgordiHash& operator  - updated hash
	AgordiHash& operator <<(UInt v) noexcept  { add(v); return *this; }

    //! \brief Add hash chunk to the hashing
    //! \note  h(a + b) = h(a) + h(b)
    //!
    //! \param other const AgordiHash&  - the chunk to be included into the hash
    //! \return void
	void add(const AgordiHash& other) noexcept;

    //! \brief Add hash chunk to the hashing
    //! \note  h(a + b) = h(a) + h(b)
    //!
    //! \param other const AgordiHash&  - the chunk to be included into the hash
    //! \return AgordiHash& operator  - updated hash
	AgordiHash& operator +=(const AgordiHash& other) noexcept  { add(other); return *this; }

    //! \brief Add number of items to the hashing
    //!
    //! \param v UInt  - the item value
    //! \param num Size  - the number of items to be included to the hashing
    //! \return void
	void add(UInt v, Size num) noexcept;


    //! \brief Subtract item from the hashing
    //!
    //! \param v UInt  - v to be excluded from the hash
    //! \return void
	void sub(UInt v) noexcept;

    //! \brief Subtract hash chunk from the hashing
    //! \note  h(a - b) = h(a) - h(b)
    //!
    //! \param other const AgordiHash&  - the chunk to be excluded from the hash
    //! \return void
	void sub(const AgordiHash& other) noexcept;

    //! \brief Subtract hash chunk from the hashing
    //! \note  h(a - b) = h(a) - h(b)
    //!
    //! \param other const AgordiHash&  - the chunk to be excluded from the hash
    //! \return void
	AgordiHash& operator -=(const AgordiHash& other) noexcept  { sub(other); return *this; };

    //! \brief Subtract number of items from the hashing
    //!
    //! \param v UInt  - the item value
    //! \param num Size  - the number of items to be excluded from the hashing
    //! \return void
	void sub(UInt v, Size num) noexcept;


//    //! \brief The number of hashed items
//    //!
//    //! \return Size  - the number of items
//	Size size() const noexcept  { return m_size; }

    //! \brief The hash is empty
    //!
    //! \return bool  - the hash is empty
	bool empty() const noexcept;

    //! \brief Low part of the sum of the items
    //!
    //! \return UInt  - low part of the sum of the items
	UInt lsum() const noexcept  { return m_lsum; }

    //! \brief High part of the sum of the items
    //!
    //! \return Size  - high part of the sum of the items
	Size hsum() const noexcept  { return m_hsum; }

    //! \brief High part of the sum of the items squares
    //!
    //! \return Size  - high part of the sum of the items squares
	Size hv2sum() const noexcept  { return m_hv2sum; }

    //! \brief Low part of the sum of the items squares
    //!
    //! \return ValCRef<AccUInt>  - low part of the sum of the items squares
	ValCRef<AccUInt> lv2sum() const noexcept  { return m_lv2sum; }

//    //! \brief Pointer to the internal data
//    //!
//    //! \return const Data&  - pointer to the array of bytes representing internal memory
//	const Data& data() const noexcept  { return reinterpret_cast<const Data&>(*this); }
//
//    //! \brief Implicit conversion to the immutable raw representation
//	operator const Data&() const noexcept  { return reinterpret_cast<const Data&>(*this); }

#ifdef UTEST  // Allow internal state modification for the unit tests
    //! \brief Assignment of the low part of the sum of the items
    //!
    //! \param val UInt  - the value to be assigned
    //! \return void
	void lsum(UInt val) noexcept  { m_lsum = val; }

    //! \brief Assignment of the high part of the sum of the items
    //!
    //! \param val Size  - the value to be assigned
    //! \return void
	void hsum(Size val) noexcept  { m_hsum = val; }

    //! \brief Assignment of the high part of the sum of the items squares
    //!
    //! \param val Size  - the value to be assigned
    //! \return void
	void hv2sum(Size val) noexcept  { m_hv2sum = val; }

    //! \brief Assignment of the low part of the sum of the items squares
    //!
    //! \param val ValCRef<AccUInt>  - the value to be assigned
    //! \return void
	void lv2sum(ValCRef<AccUInt> val) noexcept  { m_lv2sum = val; }

//    //! \brief Implicit conversion to the mutable raw representation
//	explicit operator Data&() noexcept  { return reinterpret_cast<Data&>(*this); }
#endif // UTEST


    //! \brief Evaluate digest of the current hash state
    //! \attention The digest is not collision free unlike the AgordiHash itself
    //! \note This digest is not cryptographic
    //!
    //! \return size_t  - resulting hash digest
	size_t operator()() const noexcept;

    //! \brief Encrypted representation
    //! \note The encrypted version is still collision-free but becomes not iterative
    //!
    //! \return Data  - raw storage the encrypted version of the internal data
	Data encrypted() const;

    //! \brief Encrypted (cryptographic) digest
    //! \attention The digest is not collision free unlike the AgordiHash itself
    //!
    //! \return size_t  - encrypted hash digest
	size_t edigest() const noexcept;


    //! \brief Operator less
    //!
    //! \param other const AgordiHash&  - comparing object
    //! \return bool operator  - result of the comparison
	inline bool operator <(const AgordiHash& other) const noexcept;

    //! \brief Operator less or equal
    //!
    //! \param other const AgordiHash&  - comparing object
    //! \return bool operator  - result of the comparison
	inline bool operator <=(const AgordiHash& other) const noexcept;

    //! \brief Operator greater
    //!
    //! \param other const AgordiHash&  - comparing object
    //! \return bool operator  - result of the comparison
	bool operator >(const AgordiHash& other) const noexcept  { return !(*this <= other); }

    //! \brief Operator greater or equal
    //!
    //! \param other const AgordiHash&  - comparing object
    //! \return bool operator  - result of the comparison
	bool operator >=(const AgordiHash& other) const noexcept  { return !(*this < other); }

    //! \brief Operator equal
    //!
    //! \param other const AgordiHash&  - comparing object
    //! \return bool operator  - result of the comparison
	inline bool operator ==(const AgordiHash& other) const noexcept;

    //! \brief Operator unequal (not equal)
    //!
    //! \param other const AgordiHash&  - comparing object
    //! \return bool operator  - result of the comparison
	bool operator !=(const AgordiHash& other) const noexcept  { return !(*this == other); }
};

// Routines Definitions --------------------------------------------------------
template <typename UInt, typename Size, HashItemCorr CORR>
void AgordiHash<UInt, Size, CORR>::add(UInt v) noexcept
{
#if VALIDATE >= 2
	// Note: the function is called frequently, so VALIDATE affects the performance
	if(!matches(CORR, HashItemCorr::MASK_CORANY) && !v)
		throw domain_error("add(), 0 value is prohibited in the input\n");
#endif // VALIDATE
	// Correct value to avoid collisions
	if(matches(CORR, HashItemCorr::MASK_CORALL)) {  // CORALL, CORVLD
		v += corval;
		// Check for the overflow
		if(CORR == HashItemCorr::CORVLD && v < corval)
			throw overflow_error("add(), the corrected value of " + std::to_string(v-corval)
				+ " is too large and causes the overflow\n");
	}

//	++m_size;
	// Note: v = 0 is an extremely rare case which does not deserve the dedicated processing
	m_hsum += csum(m_lsum, v);
	m_hv2sum += csum(m_lv2sum, square(v));

	// Set bits to 1 in the high part of the squared value in case of v=0 to not discard the 0 values,
	// i.e. shift by the half size of v: <<2 = *4 = *(8 / 2): 0xFF..F00..0
	// ATTENTNION: the simple sum is not modified to avoid collisions.
	if(CORR == HashItemCorr::COR0 && !v)
		m_hv2sum += csum(m_lv2sum, zsval);  // Zero substitution

#if VALIDATE >= 2
	// Note: the first element always leaves hsums empty and the max number of elements
	// should have sums <= sizeMax - 1.
	if(m_hsum == sizeMax || m_hv2sum == sizeMax)
		throw overflow_error("add(), the number of hashed elements exceeded the specified Size\n");
#endif // VALIDATE
}

//template <typename UInt, typename Size, HashItemCorr CORR>
//template <bool IsPtrSmall>
//enable_if_t<!IsPtrSmall> AgordiHash<UInt, Size, CORR>::add(const void* v) noexcept
//{
//	static_assert(sizeof(void*) % sizeof(UInt) == 0
//		, "add(), pointer type should be multiple of UInt");
//
//	throw logic_error("add(void*) is not implemented\n");
////	// ATTENTION: a single .add() should be called of void*, otherwise it affects
////	// the number of processed items
////	constexpr uint8_t  num = sizeof(void*) / sizeof(UInt);
////	for(auto i = 0; i < num; ++i)
////		add(reinterpret_cast<UInt*>(&v)[i]);  // Note: vectorization is applicable here
//}

template <typename UInt, typename Size, HashItemCorr CORR>
void AgordiHash<UInt, Size, CORR>::add(const AgordiHash& other) noexcept
{
#if VALIDATE >= 1
	if(!matches(CORR, HashItemCorr::MASK_CORANY) && other.empty())
		throw domain_error("add() other, empty digest is prohibited in the input\n");
#endif // VALIDATE

	// Note: empty digest is an extremely rare case which does not deserve the dedicated processing
	m_hsum += csum(m_lsum, other.m_lsum);
	m_hv2sum += csum(m_lv2sum, other.m_lv2sum);
#if VALIDATE >= 1
	// Note: the first element always leaves hsums empty and the max number of elements
	// should have sums <= sizeMax - 1.
	if(m_hsum >= sizeMax - other.m_hsum
	|| m_hv2sum >= sizeMax - other.m_hv2sum)
		throw overflow_error("add() other, the number of hashed elements exceeded the specified Size\n");
#endif // VALIDATE
	m_hsum += other.m_hsum;
	m_hv2sum += other.m_hv2sum;

	// Set bits to 1 in the high part of the squared value in case of v=0 to not discard the 0 values,
	// i.e. shift by the half size of v: <<2 = *4 = *(8 / 2): 0xFF..F00..0
	// ATTENTNION: the simple sum is not modified to avoid collisions.
	if(CORR == HashItemCorr::COR0 && other.empty()) {
		m_hv2sum += csum(m_lv2sum, zsval);  // Zero substitution
#if VALIDATE >= 1
		// Note: the first element always leaves hsums empty and the max number of elements
		// should have sums <= sizeMax - 1.
		if(m_hv2sum == sizeMax)
			throw overflow_error("add() other, the number of hashed elements exceeded the specified Size\n");
#endif // VALIDATE
	}
}

template <typename UInt, typename Size, HashItemCorr CORR>
void AgordiHash<UInt, Size, CORR>::add(UInt v, Size num) noexcept
{
	// Note: use Karatsuba algorithm, see:
	// https://github.com/indy256/codelibrary/blob/master/cpp/bigint.cpp#L338
	// https://github.com/sercantutar/infint/blob/master/InfInt.h#L653
	// https://github.com/technophilis/BigIntegerCPP/blob/master/BigInteger.cpp#L97
	throw logic_error("add() num is not implemented\n");
}

template <typename UInt, typename Size, HashItemCorr CORR>
void AgordiHash<UInt, Size, CORR>::sub(UInt v) noexcept
{
#if VALIDATE >= 2
	if(empty())
		throw underflow_error("sub(), subtraction from the empty hash\n");
#endif // VALIDATE
	throw logic_error("sub() is not implemented\n");
////	--m_size;
//	m_hsum -= sub(m_lsum, v)
//	m_hv2sum -= sub(m_lv2sum, square(v));
}

template <typename UInt, typename Size, HashItemCorr CORR>
void AgordiHash<UInt, Size, CORR>::sub(const AgordiHash& other) noexcept
{
#if VALIDATE >= 2
	if(empty())
		throw underflow_error("sub() other, subtraction from the empty hash\n");
#endif // VALIDATE
	throw logic_error("sub() other is not implemented\n");
}

template <typename UInt, typename Size, HashItemCorr CORR>
void AgordiHash<UInt, Size, CORR>::sub(UInt v, Size num) noexcept
{
#if VALIDATE >= 2
	if(empty())
		throw underflow_error("sub() num, subtraction from the empty hash\n");
#endif // VALIDATE
	throw logic_error("sub() num is not implemented\n");
}

template <typename UInt, typename Size, HashItemCorr CORR>
bool AgordiHash<UInt, Size, CORR>::empty() const noexcept
{
	static_assert(sizeof *this == sizeof(Data), "empty(), Data size validation failed");
	//constexpr static Data  zero{0};  // Initialize with zero
	constexpr static array<uint8_t, sizeof(AgordiHash)>  zero{0};  // Initialize with zero

	return !memcmp(this, &zero, sizeof *this);
}

template <typename UInt, typename Size, HashItemCorr CORR>
size_t AgordiHash<UInt, Size, CORR>::operator()() const noexcept
{
	// ATTENTION: requires filling with zero memory alignment padding or avoid the padding
//	return std::hash<string>()(string(reinterpret_cast<const char*>(this), sizeof *this));
//	return StreamHash(this, sizeof *this)();

	// It is faster and we can provide less collisions by implementing own hash digest.
	// We use Xor and Rotations only to not care about the overflows.
	//
	// Note: type of m_lsum can be generalized to any arithmetic type of any length,
	// which requires implementation of the respective operations.
	static_assert(sizeof m_lsum <= sizeof(size_t) && sizeof(AccUInt) <= sizeof(size_t) << 1
		, "operator(), input types validation failed");

	// Note: AccUInt has larger size (2x) than UInt.
	// Perform xor m_lsum with the byte-reversed m_hv2sum (to increase the entropy and
	// then shift it to the left on [m_lsum] UInt size.
	if(sizeof(AccUInt) > sizeof(size_t)) {
		using ArgT = conditional_t<sizeof(UInt) >= sizeof(Size), UInt, Size>;
		// Xor (hight bytes of) the low part of val with the byte-reversed m_hsum
		AccUInt  val(m_lv2sum);
		xorlsh(val, m_lsum ^ rbytes<ArgT>(m_hv2sum), (sizeof(AccUInt) - sizeof(ArgT)) << 3);  // <<3 = *8
		val ^= rbytes<ArgT>(m_hsum);
		// Shrink to the required size
		return val.low ^ rbytes<size_t>(val.high);
	}

	size_t  res(m_lv2sum);
	res ^= rbytes<size_t>(m_lsum) ^
		(static_cast<size_t>(m_hsum ^ rbytes(m_hv2sum))
			<< ((sizeof(size_t) - sizeof(m_hsum)) << 2));  // <<2 = *8/2, i.e. half of the number of bits
	return res;
}

template <typename UInt, typename Size, HashItemCorr CORR>
auto AgordiHash<UInt, Size, CORR>::encrypted() const -> Data
{
	//Data  data(sizeof(AgordiHash));
	throw logic_error("encrypted() is not implemented\n");
}

template <typename UInt, typename Size, HashItemCorr CORR>
size_t AgordiHash<UInt, Size, CORR>::edigest() const noexcept
{
	throw logic_error("edigest() is not implemented\n");
}

template <typename UInt, typename Size, HashItemCorr CORR>
bool AgordiHash<UInt, Size, CORR>::operator <(const AgordiHash& other) const noexcept
{
	// ATTENTION: memcmp() requires filling with zero memory alignment padding or avoid the padding
	return
		// ATTENTION: only on the little-endian systems words per-byte comparison yields the same results
		// (value comparison is consistent with the per-byte memcmp-based comparison)
#if CPU_LITTLE_ENDIAN
		m_lsum < other.m_lsum ||
#endif // CPU_LITTLE_ENDIAN
		memcmp(this, &other, sizeof(AgordiHash)) < 0;
}

template <typename UInt, typename Size, HashItemCorr CORR>
bool AgordiHash<UInt, Size, CORR>::operator <=(const AgordiHash& other) const noexcept
{
	// ATTENTION: memcmp() requires filling with zero memory alignment padding or avoid the padding
	return
		// ATTENTION: only on the little-endian systems words per-byte comparison yields the same results
		// (value comparison is consistent with the per-byte memcmp-based comparison)
#if CPU_LITTLE_ENDIAN
		m_lsum < other.m_lsum ||
#endif // CPU_LITTLE_ENDIAN
		memcmp(this, &other, sizeof(AgordiHash)) <= 0;
}

template <typename UInt, typename Size, HashItemCorr CORR>
bool AgordiHash<UInt, Size, CORR>::operator ==(const AgordiHash& other) const noexcept
{
	// ATTENTION: memcmp() requires filling with zero memory alignment padding or avoid the padding
	return !memcmp(this, &other, sizeof(AgordiHash));  // Note: memcmp returns 0 on full match
}

}  // daoc

#endif // AGORDIHASH_HPP
