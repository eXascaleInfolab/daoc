//! \brief Hashing routines.
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! > 	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2017-02-7

#ifndef HASHING_HPP
#define HASHING_HPP

// MACROSES:
//  - USE_STL_HASH  - use STL hash function instead of the custom one (xxHash).
// Usually, should not be touched ----------------------------------------------
//  - CPU_ARCH64 {0,1}  - use 64 bit words instead of the automatic architecture identification.
//  	ATTENTNION: results incorrect evaluations on the 32 bit architectures.

#include <string>  // to_string

#ifdef USE_STL_HASH  // ========================================================
#include <functional>  // hash
#else
//#include <cstddef>  // size_t
#include <cstring>  // memset

#include "arithmetic.hpp"  // ATTENTNION: must be included before the xxhash; contains ValCRef, CPU_LITTLE_ENDIAN, ...

// Macro definitions using macroses defined in arithmetic ---
#ifdef CPU_LITTLE_ENDIAN
	// Note: xxHash uses values in macro definitions
	#define XXH_CPU_LITTLE_ENDIAN  CPU_LITTLE_ENDIAN
#endif // CPU_LITTLE_ENDIAN

// Use native number representation on big-endian systems.
// Provides better performance but breaks consistency with little-endian results.
#define XXH_FORCE_NATIVE_FORMAT
#define XXH_STATIC_LINKING_ONLY
#include "xxhash/xxhash.h"

// Identify architecture word length.
//#if defined(__x86_64__) || defined(__aarch64__) || defined(__ppc64__) || defined(__LP64__)
//|| defined(_M_X64) || defined(_M_ARM64) || defined(_WIN64)
#ifndef CPU_ARCH64
	#if !defined(UINTPTR_MAX) || !defined(UINT64_MAX)
		#include <cstdint>  // uintX_t
		#if !defined(UINTPTR_MAX) || !defined(UINT64_MAX)
			#error "UINTPTR_MAX and UINT64_MAX should be defined (see cstdint)."
		#endif
	#endif  // UINTPTR_MAX, UINT64_MAX
	// Also can be used sizeof(void*) >= sizeof(uint64_t)
	#define CPU_ARCH64  (UINTPTR_MAX >= UINT64_MAX)
#endif // CPU_ARCH64
#if TRACE >= 2 && !CPU_ARCH64
	#warning "Compiling for the 32 bit architecture."
#endif // TRACE

#if CPU_ARCH64
	#define XXH_PREFIX(name)  XXH64 ## name
#else
	#define XXH_PREFIX(name)  XXH32 ## name
#endif // CPU_ARCH64

//! \brief Pure name of the xxHash hashing function
#define XXH_CALL  XXH_PREFIX()

//! \brief Architecture dependent naming
//!
//! \param name  - unified name
//! \return architecture-dependent name
#define XXH_NAME(name)  XXH_PREFIX(_ ## name)
// ----------------------------------------------------------
#endif // USE_STL_HASH =========================================================


namespace daoc {

using std::string;
using std::is_scalar;
#ifdef USE_STL_HASH
using std::hash;
#endif // USE_STL_HASH

constexpr static size_t  SEED = 0;  //!< Initial seed for the hashing
//constexpr static size_t  SEED = sizeof(void*) >= sizeof(uint64_t)
//	? 0x736f6d6570736575ULL : 0xcc9e2d51U;
// Initialization seed:
// 0x736f6d6570736575 (from SIP hash)
// 9650029242287828579ULL / x32: 2246822519U	(xxHash)
// 0xc4ceb9fe1a85ec53, 0x87c37b91114253d5ULL, 0xff51afd7ed558ccdULL / x32: 0xcc9e2d51, 0x38b34ae5  (MurMur3 hash)

// Stream hash routines --------------------------------------------------------
//! Stream hash
class StreamHash {
public:
	//! Hash digest type
	using Digest =
#ifdef USE_STL_HASH
		size_t
#else
		XXH_NAME(hash_t)
#endif // USE_STL_HASH
		;

	//! Internal incremental hashing state type
	using State =
#ifdef USE_STL_HASH
		Digest
#else
		XXH_NAME(state_t)
#endif // USE_STL_HASH
		;
private:
	State  m_state;  //!< Internal state of the incremental hashing
#ifdef USE_STL_HASH
protected:
    //! \brief Update the state of the incremental hashing
    //!
    //! \param digest size_t  - current hash digest
    //! \return void
	static void update(State& state, Digest digest) noexcept
	{
		constexpr uint8_t  nbshift = 3;  // Use 3 or 5 and always < bits num in state
		// Note: <<3 = *8
		static_assert(nbshift < 8 * sizeof state, "update(), nbshift is too large");
		static_assert(sizeof(State) >= sizeof(Digest), "update(), unexpected types");

		// Right rotation (cyclic shift; to increase the entropy) with xor
		state = (((state & 0x7) << (sizeof(state) << 3) - nbshift)  // Move lowest nbshift bits to the highest bits
			| (state >> nbshift))  // Shift remained bits to the nbshift to the right
		^ hash<Digest>()(digest);
	}
#endif // USE_STL_HASH
public:
	//! \brief Evaluate hash digest for the specified data in memory
	//!
	//! \param data const void*  - pointer to the data
	//! \param size size_t  - size of the data in bytes
	//! \param seed=SEED size_t  - seed for the hashing
	//! \return Digest  - resulting hash digest
	static Digest rawhash(const void* data, size_t size, size_t seed=SEED)
	{
#ifdef USE_STL_HASH
		//hash<string>()(string(reinterpret_cast<const char*>(data), size));
		const size_t  mwnum = size / sizeof(size_t);  // The number of ceil machine words
		State  state = SEED;
		for(size_t i = 0; i < mwnum; ++i)
			update(state, hash<size_t>(reinterpret_cast<size_t*>(data)[i]));
		// Process remained bytes of the data buffer
		const uint8_t  bnum = size - mwnum * sizeof(size_t);  // The number of remained bytes
		if(bnum) {
			data = reinterpret_cast<size_t*>(data) + mwnum;
			for(uint8_t i = 0; i < bnum; ++i)
				update(state, hash<uint8_t>(reinterpret_cast<uint8_t*>(data)[i]));
		}
#else
		return XXH_CALL(&data, size, seed);
#endif // USE_STL_HASH
	}

    //! \brief Default constructor
	StreamHash(): m_state{0}  {}

    //! \brief Internal state of the incremental hashing
    //!
    //! \return State const*  - internal state of the incremental hashing
	State const* state() const noexcept  { return &m_state; }

    //! \brief Internal state of the incremental hashing
    //!
    //! \return State const*  - internal state of the incremental hashing
	State* state() noexcept  { return &m_state; }

    //! \brief Clear the internal state of the incremental hashing
	void clear()
	{
#ifdef USE_STL_HASH
		m_state = 0;
#else
		memset(&m_state, 0, sizeof m_state);
#endif // USE_STL_HASH
	}

    //! \brief Add data to the incremental hashing
    //!
    //! \param data const void*  - pointer to the data
    //! \param size size_t  - size of the data in bytes
    //! \return void
	void add(const void* data, size_t size) noexcept
	{
#ifdef USE_STL_HASH
		update(m_state, rawhash(data, size))
#else
		XXH_NAME(update)(&m_state, data, size)
#endif // USE_STL_HASH
		;
	}

    //! \brief Add data element to the incremental hashing
    //!
    //! \tparam T  - type of the data element
    //!
    //! \param val T  - data element
    //! \return void
	template <typename T>
	void add(ValCRef<T> val) noexcept
	{
		static_assert(sizeof(T) <= sizeof(void*) || is_scalar<T>::value
			, "add(), the specialization is expected for the non-scalar template type");
#ifdef USE_STL_HASH
		update(m_state, hash<T>()(val))
#else
		add(&val, sizeof val)
#endif // USE_STL_HASH
		;
	}

    //! \brief Hash digest of the incremental hashing
    //!
    //! \return Digest  - hash digest of the incremental hashing
	Digest operator()() const noexcept
	{
		return
#ifdef USE_STL_HASH
			m_state
#else
			XXH_NAME(digest)(&m_state)
#endif // USE_STL_HASH
			;
	}
};

//!< \copydoc StreamHash::add<T>
template <>
inline void StreamHash::add<string>(const string& val) noexcept
{
#ifdef USE_STL_HASH
	update(m_state, hash<string>()(val))
#else
	add(val.data(), val.size())
#endif // USE_STL_HASH
	;
}

// SolidHash hash routines -----------------------------------------------------
//! Value hash
template <typename T>
struct SolidHash {
	// Note: arrays can be multi-dimensional and may contain pointers, compound
	// objects also may contain pointers, which prevents default any meaningful
	// behavior. The user should implement specialization to explicitly process
	// pointers either as addresses or resolve them to the actual data.
	static_assert(sizeof(T) <= sizeof(void*) || is_scalar<T>::value
		, "SolidHash, the specialization is expected for the non-scalar type");

    //! \brief Hash the value
    //!
    //! \param val T  - value
    //! \return size_t  - hash digest
	size_t operator()(ValCRef<T> val) const noexcept
	{
		return
#ifdef USE_STL_HASH
			hash<T>()(val)
#else
			XXH_CALL(&val, sizeof val, SEED)
#endif // USE_STL_HASH
		;
	}
};

// SolidHash specializations -----
template <>
struct SolidHash<string> {
    //! \brief Hash the object
    //!
    //! \param val const string&  - object
    //! \return size_t  - hash digest
	size_t operator()(const string& val) const noexcept
	{
		return
#ifdef USE_STL_HASH
			hash<string>()(val)
#else
			XXH_CALL(val.data(), val.size(), SEED)
#endif // USE_STL_HASH
		;
	}
};

}  // daoc

#endif // HASHING_HPP
