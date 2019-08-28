//! \brief Cluster (community) Nodes List parser.
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

#ifndef PARSER_CNL_H
#define PARSER_CNL_H

#include "fileio/iotypes.h"
#include "types.h"  // common types


namespace daoc {

using std::ifstream;

//! Cluster nodes list format parser
class CnlParser {
public:
    //! \brief Parser constructor
    //!
    //! \param infile const string&   - input file to be processed, network
	CnlParser(const string& filename);

	//! \copydoc m_fuzzy
	bool fuzzy() const  { return m_fuzzy; }

    //! \brief Build clusters from the underlying file
    //! Constructs clusters and initializes their attributes:
    //! links, des (based on graph nodes) and etc.
    //! \attention des of clusters are NOT ordered
    //! \post Updates owners of the graph nodes
    //!
    //! \param idnodes IdItems<Node<LinksT>>&  - mapping of the node ids into the nodes
    //! \return unique_ptr<RawMembership<LinksT>>  - parsed raw membership
    //! 	(cluster links are not built)
	template <typename LinksT>
    // Note: unique_ptr is not supported by SWIG because of the copy constructor absence
	//unique_ptr<RawMembership<LinksT>> build(
	shared_ptr<RawMembership<LinksT>> build(
#ifndef SWIG
		IdItems<Node<LinksT>>
#else
		IdItems(Node<LinksT>)
#endif // SWIG
		& idnodes);
protected:
	// File parsing related
	//! Spaces symbols in the file (just skipped if not delimiters or if repeated)
	//! \note '\0' is included implicitly
	static const char  m_spaces[];
	//! Comment line mark
	//! \attention Only whole line comments are supported, not in-line
	constexpr static char  m_comment = '#';

    //! \brief Parse header of the input file to load meta-information
    //! \post Initializes: m_clsnum, m_ndsnum, m_fuzzy, m_numbered
	//!
    //! \return void
	void header();
private:
	size_t  m_size;  //!< File size in bytes
	ifstream  m_infile;  //!< Input file, network
	string  m_line;  //! Parsed line (required to hold the line after the header to start the build() with it)
	Id  m_clsnum;  //!< The number of clusters according to the header
	Id  m_ndsnum;  //!< The number of nodes according to the header
	bool  m_fuzzy;  //!< Whether node shares are specified (overlaps exists and are not equal)
	bool  m_numbered;  //!< Whether the clusters are numbered
};

}  // daoc

#endif // PARSER_CNL_H
