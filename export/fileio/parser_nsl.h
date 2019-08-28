//! \brief Network Specified by Links (Nodes Specifying Links) parser.
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! > 	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2016-11-24

#ifndef PARSER_NSL_H
#define PARSER_NSL_H

#include "fileio/iotypes.h"


namespace daoc {

using std::ifstream;

//! Network Specified by Links (Nodes Specifying Links) parser
class NslParser {
public:
    //! \brief Parser constructor
    //!
    //! \param inpopts const InpOptions&   - input network (graph) options
    // Note: inpopts is a constructor parameters to be able to parse
    // similarly multiple files -> graphs
	NslParser(const InpOptions& inpopts);

    //! \brief Whether the input network is weighted
    //!
    //! \return bool - input network is weighted
	bool weighted() const  { return m_weighted; }

    //! \brief Build the input graph from the underlying file of the input network
    //!
    //! \return shared_ptr<GraphT>  - resulting input graph
	template <typename GraphT>
    // Note: unique_ptr is not supported by SWIG because of the copy constructor absence
	//unique_ptr<GraphT> build();
	shared_ptr<GraphT> build();
protected:
	// File parsing related
	//! Spaces symbols in the file (just skipped if not delimiters or if repeated)
	// Note: constexpr can't be used from the outside of the library
	static const char  m_spaces[];
	//! Comment line mark
	//! \attention Only whole line comments are supported, not in-line
	constexpr static char  m_comment = '#';

    //! \brief Parse header of the input file to load meta-information
    //! \post Initializes: m_weighted, m_directed, m_nodes, m_links, m_line
	//!
    //! \return void
	void header();
private:
	size_t  m_size;  //!< File size in bytes
	ifstream  m_infile;  //!< Input file, network
	string  m_line;  //! Parsed line (required to hold the line after the header to start the build() with it)
	const bool  m_shuffle;  //!< Shuffle links and nodes on construction
	const bool  m_sumdups;  //!< Accumulate weight of duplicated links or just skip them
	bool  m_weighted;  //!< Whether the input network is weighted
	bool  m_directed;  //!< Whether the input network is directed (arcs) or underected (only edges)
	Id  m_nodes;  //!< The number of nodes in the network, 0 if unknown
	Size  m_links;  //!< The number of links
};

}  // daoc

#endif // PARSER_NSL_H
