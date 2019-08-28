//! \brief Hierarchy printer in the Cluster Nodes List format.
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! > 	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2016-10-06

#ifndef PRINTER_CNL_H
#define PRINTER_CNL_H

#include "fileio/iotypes.h"

namespace daoc {

template <typename LinksT>
class CnlPrinter {
	const Hierarchy<LinksT>&  m_hier;
public:
	using ILev = typename Levels<LinksT>::iterator;  //!< Levels iterator

	// Note: SWIG uses transparent wrapper with shared_ptr
    //! \brief Hierarchy printer in the CNL format
    //!
    //! \param hier const Hierarchy<LinksT>&  - the hierarchy to be outputted
    //! \return
	CnlPrinter(const Hierarchy<LinksT>& hier): m_hier(hier)  {}

    //! \brief Hierarchy printer in the CNL format
    //!
    //! \param hier shared_ptr<Hierarchy<LinksT>>  - the hierarchy to be outputted
    //! \return
	CnlPrinter(shared_ptr<Hierarchy<LinksT>> hier): m_hier(*hier)  {}

    //! \brief Output the hierarchy
//	//! \note The number of outputting levels is implicitly specified by the number of fouts files
//    //! \note Indexes are used for the custom levels output
//    //! \pre The number of output files should be synced with the number of outputting
//    //! 	levels
    //!
    //! \param fvec FileWrapper&  - nodes vectorization output file
    //! \param nvo const NodeVecCoreOptions&  - nodes vectorization options
    //! \param fouts FileWrappers&  - output files
    //! \param clsfmt ClsOutFmtBase  - cluster output format
    //! \param fltMembers=false bool  - use the highest bit of the node id as a filtering out
    //! 	flag from the clustering results
    //! \param blev=0 LevelNum  - index of the first outputting level from the hierarchy bottom
    //! \param elev=LEVEL_NONE LevelNum  - index past the last outputting level from the bottom
    //! 	Note: elev is required only when levStepRatio is specified and we want to
    //! 	output the upper margin strictly
    //! \param levStepRatio=0 const float  - step ratio of the following level relative to
    //! 	the latest outputted level, E [0, 1]. 1 means that this parameter is not used,
    //! 	i.e. each following level should be outputted without omission. 0 means output
    //! 	only the bottom level.
    //! 	Note: applicable only for the ClsOutFmt::CUSTLEVS
    //! \param signif=nullptr const SignifclsOptions*  - options for the significant clusters output
    //! \return void
	void output(FileWrapper& fvec, const NodeVecCoreOptions& nvo, FileWrappers& fouts
		, ClsOutFmtBase clsfmt, bool fltMembers=false, LevelNum blev=0, LevelNum elev=LEVEL_NONE
		, const float levStepRatio=1, const SignifclsOptions* signif=nullptr
#if	VALIDATE >= 2
		, const vector<LevelNum>& ilevs={}
#endif // VALIDATE
		);
};

// Accessory functions ---------------------------------------------------------
//! \brief Pad the string to have at least the specified length using symb adding
//!
//! \param str string&&  - initial string to be padded (aligned)
//! \param maxpad uint8_t  - maximal padding (minimal target len of the string)
//! \param symb char  - the padding(aligning) symbol
//! \param left=true bool  - whether to alight from the left of from the right
//! \return string&&  - resulting padded input string
inline string&& pad(string&& str, uint8_t maxpad, char symb=' ', bool left=true)
{
	// Check the necessity of padding
	if(str.size() < maxpad) {
		if(left)  // Left padding
			str.insert(0, maxpad - str.size(), symb);
		else str.resize(maxpad, symb);  // Right padding
	}

	return std::forward<string>(str);  // Note: return by RVAL if possible, otherwise ref of the ref
}

}   // daoc

#endif // PRINTER_CNL_H
