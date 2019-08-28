//! \brief Exporting common processing routines.
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! >	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2016-09-29

#ifndef PROCESSING_H
#define PROCESSING_H

#include "types.h"  // Id, etc.


namespace daoc {

// Declaration of common item-related functions -------------------------------
//! \brief Calculate bidir (inbound + outbound) link weight
//!
//! \tparam NONSYMMETRIC  - inbound/outbound weights of the links are not the same
//! \tparam ItemT  - item type
//!
//! \param cl const ItemT*  - source item of the link
//! \param ln const typenameItemT::LinkT&  - target link
//! \return AccWeight  - total weight of the link
template <bool NONSYMMETRIC, typename ItemT>
AccWeight linkWeight(const ItemT* cl, const typename ItemT::LinkT& ln) noexcept;

//#if VALIDATE >= 2
////! \brief Calculate accumulative (inbound + outbound) link weight
////! 	(equal to DOUBLE weight for unordered links) to the specified cluster (node)
////! \pre
////! 	- Links should be ordered corresponding to bin_search and unique.
////! 	- Dest cluster can be null in case if inverse (total weight calculation).
////!
////! \tparam NONSYMMETRIC  - inbound/outbound weights of the links are not the same
////! \tparam INVERSE bool  - whether condition is inverse (all links except)
////! \tparam ItemT  - cluster type
////!
////! \param cl1 ItemT*  - source cluster
////! \param cl2 ItemT*  - target cluster
////! \return AccWeight  - resulting weight
////! 	ATTENTION: WEIGHT_NONE is returned for the non-connected items
//template <bool NONSYMMETRIC, bool INVERSE, typename ItemT>
//AccWeight accWeight(const ItemT* cl1, const ItemT* cl2=nullptr);
//#endif // VALIDATE

}  // daoc

#endif // PROCESSING_H
