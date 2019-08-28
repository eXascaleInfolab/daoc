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

#ifndef PARSER_RCG_HPP
#define PARSER_RCG_HPP

#include <stdexcept>
#include "fileio/rawparse.hpp"
#include "fileio/parser_rcg.h"


namespace daoc {

using std::domain_error;


// RcgParser -------------------------------------------------------------------
template <typename GraphT>
shared_ptr<GraphT> RcgParser::build()
{
	// Create the graph
	// Note: to apply graph reduction the exact number of nodes should be known
	// and single batch links specification for each node should be used
	// Note: nodes are reduced on clustering if required, not on the graph construction
	GraphT  graph(m_nodes, m_shuffle, m_sumdups, Reduction::NONE);
	if(m_idstart != ID_NONE) {
		StructNodeErrors  nderrs("WARNING build(), the duplicated nodes are skipped: ");
		graph.addNodes(m_nodes, m_idstart, &nderrs);
#if TRACE >= 1
		nderrs.show();
#endif // TRACE
	}

	// Parse links
	StructLinkErrors  lnerrs("WARNING build(), the duplicated links are skipped: ");
	StructNodeErrors  nderrs("WARNING build(), the nodes specified without any links: ");
	string  line;
	while(getline(m_infile, line)) {
		char* str = const_cast<char*>(line.c_str());
		// Skip empty and space lines, skip comments
		if(!skipSymbols(str, m_spaces) || *str == m_comment)  // EOL after skipping the spaces
			continue;

		if(*str != m_sectmark) {
			// Parse the payload (section body)
			if(m_bodysect != FileSection::EDGES && m_bodysect != FileSection::ARCS)
				continue;
#if TRACE >= 3
			fprintf(ftrace, "> Parsing links of:  %s\n", str);
#endif // TRACE_EXTRA
			parseLinks(graph, str, m_bodysect == FileSection::ARCS, &lnerrs, &nderrs);
		} else {
			// Define current section
			if(lowerAndSkip(str, "/edges")) {
				m_bodysect = FileSection::EDGES;
			} else if(lowerAndSkip(str, "/arcs")) {
				m_bodysect = FileSection::ARCS;
				m_directed = true;  // Note: at least one arcs section results in the directed graph
			} else throw domain_error(string("Unknown section is used: ").append(str) += '\n');
		}
	}
#if TRACE >= 1
	lnerrs.show();
	nderrs.show();
#endif // TRACE
	// Note: m_nodes number correction has no any sense even if m_nodes did not correspond to the
	// actual number of nodes in the graph, because the graph is already created

	return make_shared<GraphT>(move(graph));
}

template <typename GraphT>
void RcgParser::parseLinks(GraphT& graph, char* str, bool directed, StructLinkErrors* lnerrs, StructNodeErrors* nderrs)
{
	typename GraphT::InpLinksT  links;
	using Link = typename GraphT::InpLinkT;
	using Weight = typename Link::Weight;
	constexpr bool weighted = Link::IS_WEIGHTED;

	auto invalDstId = [weighted](Id val, char end) noexcept -> bool {
		return val == ID_NONE
			// Next char should be a space or '\0' if unweighted, otherwise ':' could be also
			// Note: weight specification is optional for the weighted network
			|| (!strchr(m_spaces, end) && (!weighted || end != ':'));  // Note: terminating '\0' is considered by strchr
	};
	auto invalWeight = [](Weight val, char end) noexcept -> bool {
		// Next char should be space or ',', or '\0' (considered to be part of str)
		return val < 0 || !strchr(m_spaces, end);  // Note: terminating '\0' is considered by strchr
	};

	// Fetch node id
	// Note: invalidate should be a bool (*func)(val, pose)
	constexpr auto invalSrcId = [](Id val, char end) noexcept -> bool {
		return val == ID_NONE || end != '>';  // Next char should be '>'
	};
	// Checks also the control '>' delimiter between the src node id and dest node ids
	auto nid = parseVal<Id>(str, strtoul, &invalSrcId, "Node id is invalid");

	// Fetch links
	++str;  // skip the '>' separator
	while(skipSymbols(str, m_spaces)) {
		// Fetch dest id
		// NOTE: spaces are discarded automatically by the strtoul()
		auto did = parseVal<Id>(str, strtoul, &invalDstId
			, "Parsed dst id is invalid (or equals to ID_NONE)");
		// NOTE: format for the weight is  id:weight without spaces

		// Fetch link weight
		// NOTE: non-specified weight for the weighted graph results default link weight
		// ATTENTION: the weight should be passed only if it is specified explicitly
		if(weighted && *str == ':') {
			// Next is space or '\0'
			Weight weight = parseVal<Weight>(++str, strtof, invalWeight  // Note: ++str to skip ':'
				, "Parsed weight is invalid (should be a non-negative float)");
			//links.emplace_back(did, weight);
			addLink(links, did, weight);
		}
		// Note: default values is used if weight is not specified
		else links.emplace_back(did);
#if TRACE >= 3
		fprintf(ftrace, ">> #%u.%lu: %G added, %lu links\n"
			, nid, did, weight, links.size());
#endif // TRACE_EXTRA
	}

	// Store links in the Graph
	// Note: nodes links are sorted for each node and optionally validated on hierarchy building
	if(!links.empty()) {
// Note: this is taken into account on the links extension inside the graph
//		// Threat looped arc as an edge to still have undirected links
//		// when a node weight is specified via the arc
//		directed = directed && (links.size() >= 2 || links.front().id != nid);
		if(m_idstart != ID_NONE) {
			if(directed)
				graph.template addNodeLinks<true>(nid, move(links), lnerrs);
			else graph.template addNodeLinks<false>(nid, move(links), lnerrs);
		} else if(directed)
			graph.template addNodeAndLinks<true>(nid, move(links), lnerrs);
		else graph.template addNodeAndLinks<false>(nid, move(links), lnerrs);
	} else {
		if(nderrs)
			nderrs->add(nid);
//		fprintf(ftrace, "WARNING parseLinks(), links are not specified for the node:  %s\n", str);
//		throw domain_error(string("parseLinks(), Links are not specified for the node: ")
//		.append(str) += '\n');
		graph.addNodes(1, nid);
	}
}

}  // daoc

#endif // PARSER_RCG_HPP
