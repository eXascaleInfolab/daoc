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

#ifndef PARSER_CNL_HPP
#define PARSER_CNL_HPP

#if VALIDATE >= 2
#include <set>
#endif // VALIDATE

#include "operations.hpp"  // bsObjsDest
#include "fileio/rawparse.hpp"
#include "fileio/parser_cnl.h"


namespace daoc {

using std::common_type;
using std::min;
#if VALIDATE >= 2
using std::set;
#endif // VALIDATE


// CnlParser -------------------------------------------------------------------
template <typename LinksT>
shared_ptr<RawMembership<LinksT>> CnlParser::build(IdItems<Node<LinksT>>& idnodes)
{
	constexpr auto invalCid = [](Id val, char end) noexcept -> bool {
		return val == ID_NONE || end != '>';  // Next char should be '>'
	};
	auto invalNid = [](Id val, char end) noexcept -> bool {
		return val == ID_NONE || !strchr(" \t:", end);
	};
	auto invalShare = [](Share val, char end) noexcept -> bool {
		return val <= 0 || val > 1 || !strchr(m_spaces, end);  // Share E (0, 1]
	};

	RawMembership<LinksT>  msp;  // Forming raw (cluster links are not built) membership
	// Reserve the number of nodes if possible
	{
		Id  ndsnum = m_ndsnum;
		// Estimate the number of nodes if required
		if(!ndsnum && m_size) {
			//const float  membership = 1;  // Average expected membership of nodes in the clusters
			size_t  magn = 10;  // Decimal ids magnitude
			unsigned  img = 1;  // Index of the magnitude (10^1)
			size_t  reminder = m_size % magn;  // Reminder in bytes
			ndsnum = reminder / ++img;  //  img digits + 1 delimiter for each element
			while(m_size >= magn) {
				magn *= 10;
				ndsnum += (m_size - reminder) % magn / ++img;
				reminder = m_size % magn;
			}
		}
		if(ndsnum) {
			msp.ndshares.reserve(ndsnum);
#if TRACE >= 2
			fprintf(ftrace, "> build(), nodes number was not specified"
				", preallocated for %u estimated nodes\n", ndsnum);
#endif // TRACE
		}
	}
	// Parse CNL file
	// .cnl format:
	// [<cluster_id> >] <node1_id>[:<node1_share>] <node2_id>[:<node2_share> ...]
#if VALIDATE >= 2
	set<Id>  cids;  // Cluster ids for the verification
	//if(ndsnum)
	//	cids.reserve(ndsnum);
#endif // VALIDATE
#if TRACE >= 2
	Id lines = 0;  // The number of valuable lines processed
#endif // TRACE
	do {
		char* str = const_cast<char*>(m_line.c_str());
#if TRACE >= 3
		fprintf(ftrace, "> build(), parsing the line: %s\n", str);
#endif // TRACE_EXTRA
		// Skip empty and space lines, skip comments
		if(!skipSymbols(str, m_spaces) || *str == m_comment)  // EOL after skipping the spaces
			continue;

#if TRACE >= 2
		++lines;
#endif // TRACE
		// Parse cluster id if required
		if(m_numbered) {
			Id  cid = parseVal<Id>(str, strtoul, invalCid, "Cluster id is invalid");
			msp.clusters.emplace_back(0, cid, 0);
			++str;  // Skip '>'
		} else msp.clusters.emplace_back(0);

		// Parse node ids
		auto& cl = msp.clusters.back();
#if VALIDATE >= 2
		cids.insert(cids.end(), cl.id);  // Store in the set to verify the ids are unique
#endif // VALIDATE
		while(skipSymbols(str, m_spaces)) {
			Id  nid = parseVal<Id>(str, strtoul, invalNid, "Node id is invalid");
			// Add read node to the cluster and update node owners
			auto& node = *idnodes[nid];
			// Update node owners
			// ATTENTION: Owners must be ordered
			if(node.owners.empty())
				node.owners.emplace_back(&cl);
			else node.owners.emplace(linear_ifind(node.owners, &cl
				, bsObjsDest<Owners<LinksT>>), &cl);
			// Update cluster descendants
			cl.des.push_back(&node);  // Note: des are intentionally not ordered here

			// Check for the ':' and read node share if required
			if(*str == ':') {
#if VALIDATE >= 2
				assert(m_fuzzy && "build(), shares should be specified only for the"
					" fuzzy (unequal overlapping) clustering");
#endif // VALIDATE
				Share share = parseVal<Share>(++str, strtof, invalShare, "The share is invalid");  // Note: ++str to skip ':'
				// Save the share only if there are multiple shares exist and they might be inequal
				if(share != 1) {
					auto& nodeShares = msp.ndshares[&node];
					nodeShares.emplace(insorted(nodeShares, &cl
						, bsObjsDest<NodeShares<LinksT>>), &cl, share);
					assert(nodeShares.size() <= node.owners.size()
						&& "build(), unequal shares should be specified at most for all owners of the node");
				}
			}
		}
#if TRACE >= 2
		if(cl.des.empty())
			fprintf(ftrace, "> WARNING build(), member nodes are not specified for the #%u\n", cl.id);
#endif // TRACE
	} while(getline(m_infile, m_line));
#if VALIDATE >= 1
#if VALIDATE >= 2
	assert(msp.clusters.size() == cids.size()
		&& "build(), clusters size validation failed, cluster ids should be unique");
#endif // VALIDATE 2
	assert((!m_clsnum || msp.clusters.size() == m_clsnum) && "build(), clusters size validation failed");
	// Note: shares are specified and stored usually for the unequally shared nodes
	assert((!m_ndsnum || msp.ndshares.size() <= m_ndsnum) && "build(), nodes size validation failed");
#endif // VALIDATE
#if TRACE >= 2
	assert(lines == m_clsnum && "build(), clusters size validation failed"
		" (file header is inconsistent with the content)");
	fprintf(ftrace, "> build(), %u lines processed, %lu clusters loaded\n"
		, lines, msp.clusters.size());
#endif // TRACE

	//return msp;  // Use RVO optimization (avoid copying)
	return make_shared<RawMembership<LinksT>>(move(msp));
}

}  // daoc

#endif // PARSER_CNL_HPP
