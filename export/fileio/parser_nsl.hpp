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
//! \date 2016-11-28

#ifndef PARSER_NSL_HPP
#define PARSER_NSL_HPP

#include <stdexcept>
#include "fileio/rawparse.hpp"
#include "fileio/parser_nsl.h"


namespace daoc {

using std::domain_error;


// NslParser -------------------------------------------------------------------
template <typename GraphT>
shared_ptr<GraphT> NslParser::build()
{
	// Create the graph
	// Note: to apply graph reduction the exact number of nodes should be known
	// and single batch links specification for each node should be used
	// Note: nodes are reduced on clustering if required, not on the graph construction
	GraphT  graph(m_nodes, m_shuffle, m_sumdups, Reduction::NONE);  // , m_directed

	if(!m_nodes && m_size) {
		// Estimate the number of nodes to the least expected one
		size_t  magn = 10;  // Decimal ids magnitude
		unsigned  img = 1;  // Index of the magnitude (10^1)
		size_t  reminder = m_size % magn;  // Reminder in bytes
		size_t  elsnum = reminder / ++img;  //  img digits + 1 delimiter for each element
		while(m_size >= magn) {
			magn *= 10;
			elsnum += (m_size - reminder) % magn / ++img;
			reminder = m_size % magn;
		}
		if(m_directed)
			elsnum /= 2;
		// Expected number of nodes
		if(elsnum) {
			elsnum = pow(elsnum, 0.78f);  // ~ 5K -> 1.2K, 20K -> 2.3K, 50K -> 4.6K, 100K -> 8K
			// Note: to apply graph reduction the exact number of nodes should be known
			// and single batch links specification for each node should be used
			graph.reset(elsnum, m_shuffle, m_sumdups, Reduction::NONE);
#if TRACE >= 2
			fprintf(ftrace, "> build(), nodes number was not specified"
				", preallocated for %lu estimated nodes\n", elsnum);
#endif // TRACE
		}
	}

	// Parse links in the format:
	// <src_id> <dst_id> [<weight>]
	using Weight = typename GraphT::InpLinkT::Weight;

	constexpr char  invalIdMsg[] = "id == ID_NONE or the terminating symbol is invalid";
	// Note: invalidate should be a bool (*func)(val, pose)
	auto invalId = [](Id val, char end) noexcept -> bool {
		return val == ID_NONE
			// Next char should be a space or '\0'
			|| !strchr(m_spaces, end);  // Note: ending '\0 ' is considered by the strchr
	};

	typename GraphT::InpLinksT  links;
	Id  nodeId = ID_NONE;  // Current node id
#if VALIDATE >= 1
	Size  linksSize = 0;  // The number of links
#endif // VALIDATE
	StructLinkErrors  lnerrs("WARNING build(), the duplicated links are skipped: ");
	errno = 0;
	do {
		char* str = const_cast<char*>(m_line.c_str());
		// Skip empty and space lines, skip comments
		if(!skipSymbols(str, m_spaces) || *str == m_comment)  // EOL after skipping the spaces
			continue;

		// Parse src id and dst id
		auto sid = parseVal<Id>(str, strtoul, invalId, invalIdMsg);
		if(!skipSymbols(str, m_spaces))  // End of str
			throw domain_error(m_line.insert(0, "ERROR build(), The dest id is expected: ") += '\n');  // Note: m_line doesn't have ending "\n"
		auto did = parseVal<Id>(str, strtoul, invalId, invalIdMsg);

		// Add links accumulated for the node to the graph
		if(sid != nodeId && !links.empty()) {
			if(m_directed)
				graph.template addNodeAndLinks<true>(nodeId, move(links), &lnerrs);
			else graph.template addNodeAndLinks<false>(nodeId, move(links), &lnerrs);
			links.clear();
		}
		nodeId = sid;

		// Set the weight only if it explicitly specified for the weighted network
		if(GraphT::InpLinkT::IS_WEIGHTED && skipSymbols(str, m_spaces))
			//links.emplace_back(did, parseVal<Weight>(str, strtof));
			addLink(links, did, parseVal<Weight>(str, strtof));
		else links.emplace_back(did);
#if VALIDATE >= 1
		++linksSize;
#endif // VALIDATE
	} while(getline(m_infile, m_line));

	// Add remained links
	if(!links.empty()) {
		if(m_directed)
			graph.template addNodeAndLinks<true>(nodeId, move(links), &lnerrs);
		else graph.template addNodeAndLinks<false>(nodeId, move(links), &lnerrs);
	}
#if TRACE >= 1
	lnerrs.show();
#if VALIDATE >= 1
	if(m_links && m_links != linksSize)
		fprintf(ftrace, "The number of links specified in the header (%lu) does not"
			" correspond to the actual number of links(%lu)\n", m_links, linksSize);
#endif // VALIDATE
#endif // TRACE

	return make_shared<GraphT>(move(graph));
}

}  // daoc

#endif // PARSER_NSL_HPP
