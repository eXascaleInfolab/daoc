//! \brief Readable Compact Graph (former Hirecs Input Graph) parser.
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

#ifndef PARSER_RCG_H
#define PARSER_RCG_H

#include "fileio/iotypes.h"


namespace daoc {

using std::ifstream;

//! Readable Compact Graph (former Hirecs Input Graph) format
//! \note Parser of any another input format should just duplicate the public
//! 	interface of RcgParser to be applied just being passed as a template
//! 	parameter for Client::execute()
class RcgParser {
public:
    //! \brief Parser constructor
    //!
    //! \param inpopts const InpOptions&   - input network (graph) options
    // Note: inpopts is a constructor parameters to be able to parse
    // similarly multiple files -> graphs
	RcgParser(const InpOptions& inpopts);

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

    //! \brief Whether the input network specified as validated
    //!
    //! \return bool - specified as validated
	bool validated() const  { return m_validated; }
protected:
    //! .hig file sections, similar to Pajec format, but more compact and readable
	enum class FileSection
	{
		NONE,
		GRAPH,
		NODES,
		EDGES,  //  Undirected links => symmetric weights
		ARCS  //  Directed links, weights still might be symmetric and might be not
	};

	// File parsing related
	//! Spaces symbols in the file (just skipped if not delimiters or if repeated)
	//! \note '\0' is included implicitly
	static const char  m_spaces[];
	//! Comment line mark
	//! \attention Only whole line comments are supported, not in-line
	constexpr static char  m_comment = '#';
	//! Section mark
	constexpr static char  m_sectmark = '/';

    //! \brief Parse header of the input file to load meta-information
    //! \post Initializes: m_bodypos, m_bodysect, m_weighted, m_directed, m_nodes, m_idstart
	//!
    //! \return void
	void header();

	//! \brief Extend the Graph by links parsing
	//!
    //! \param graph Graph<WEIGHTED>&  - target graph to be extended
	//! \param str const char*  - input string to be parsed for the links creation
    //! \param directed bool  - directed (arcs) / undirected (edges) links
	//! \param lnerrs=nullptr StructLinkErrors*  - occurred accumulated link errors
	//! 	to be reported by the caller (duplicated discarded links)
	//! \param nderrs=nullptr StructNodeErrors*  - occurred accumulated link errors
	//! 	to be reported by the caller (nodes without any links)
	//! \return void
	template <typename GraphT>
	void parseLinks(GraphT& graph, char* str, bool directed, StructLinkErrors* lnerrs=nullptr, StructNodeErrors* nderrs=nullptr);
private:
	ifstream  m_infile;  //!< Input file, network
	const bool  m_shuffle;  //!< Shuffle links and nodes on construction
	const bool  m_sumdups;  //!< Accumulate weight of duplicated links or just skip them
	FileSection  m_bodysect;  //!< Starting section of the body
	bool  m_weighted;  //!< Whether the input network is weighted
	bool  m_validated;  //!< Whether the input network specified as validated
	bool  m_directed;  //!< Whether the input network is directed (arcs) or underected (only edges)
	Id  m_nodes;  //!< The number of nodes in the network, 0 if unknown
	Id  m_idstart;  //!< Starting id of the nodes, ID_NONE if unknown. Note: triggers nodes preallocation and link ids validation if specified
};

}  // daoc

#endif // PARSER_RCG_H
