//! \brief Hierarchy printer in the .rcg-like format: Readable Hierarchy from Bottom (.rhb).
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! > 	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2016-10-12

#ifndef PRINTER_RHB_H
#define PRINTER_RHB_H

#include "fileio/iotypes.h"

namespace daoc {

template <typename LinksT>
class RhbPrinter {
	const Hierarchy<LinksT>&  m_hier;
public:
	// Note: SWIG uses transparent wrapper with shared_ptr
    //! \brief Hierarchy printer in the RHB format
    //!
    //! \param hier const Hierarchy<LinksT>&  - the hierarchy to be outputted
    //! \return
	RhbPrinter(const Hierarchy<LinksT>& hier): m_hier(hier)  {}

    //! \brief Hierarchy printer in the RHB format
    //!
    //! \param hier shared_ptr<Hierarchy<LinksT>>  - the hierarchy to be outputted
    //! \return
	RhbPrinter(shared_ptr<Hierarchy<LinksT>> hier): m_hier(*hier)  {}

    //! \brief Output the hierarchy
    //!
    //! \param fouts FileWrappers&&  - output file
    //! \return void
	void output(FileWrapper& fout) const;
};

}  // daoc

#endif // PRINTER_RHB_H
