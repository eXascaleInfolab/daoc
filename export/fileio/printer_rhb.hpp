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
//! \date 2016-10-13

#ifndef PRINTER_RHB_HPP
#define PRINTER_RHB_HPP

#include <cstdio>
#include "fileio/printer_rhb.h"

namespace daoc {

// Accessory Operations --------------------------------------------------------
//#ifdef MEMBERSHARE_BYCANDS
////! \brief Evaluates whether the owner shares of the el are unequal (fuzzy overlap)
////!
////! \param el const ItemT&  - the element (cluster / node) to be checked
////! \return bool  - the shares are unequal
//template <typename ItemT>
//bool neqshare(const ItemT& el)
//{
//	if(el.owners.size() >= 2) {
//		const auto numac = el.owners.front().numac;
//		for(const auto& ow: el.owners)
//			if(ow.numac != numac)
//				return true;
//	}
//	return false;
//}
//#endif // MEMBERSHARE_BYCANDS

//! \brief Output the element's (cluster / node) ownership to the file
//!
//! \param el const ItemT&  - element to be outputted
//! \param fout FileWrapper&  - output file
//! \return void
template <typename ItemT>
void outpel(const ItemT& el, FileWrapper& fout)
{
	// Output el owners with the shares
	// el1_id> owner1_id[:share1] owner2_id[:share2] ...
	// Note: some elements might not have owners, but still should be traced
	fprintf(fout, "%u>", el.id);
#ifdef MEMBERSHARE_BYCANDS
	// Check whether all shares to owners are equal
	bool neqshare = false;  // Shares are not equal
	if(el.owners.size() >= 2) {
		const auto numac = el.owners.front().numac;
		// Note: the number of owners is small
		for(const auto& ow: el.owners)
			if(ow.numac != numac) {
				neqshare = true;
				break;
			}
	}
#endif // MEMBERSHARE_BYCANDS
	// Output the owners with the optional share (output shares only if they are unequal)
	for(const auto& ow: el.owners)
#ifdef MEMBERSHARE_BYCANDS
		if(neqshare)
			fprintf(fout, " %u:%G", ow.dest->id, Share(ow.numac) / el.totac);
		else
#endif // MEMBERSHARE_BYCANDS
		fprintf(fout, " %u", ow.dest->id);
	fputc('\n', fout);
}

// RHB Printer -----------------------------------------------------------------
template <typename LinksT>
void RhbPrinter<LinksT>::output(FileWrapper& fout) const
{
#if TRACE >= 2
	fprintf(ftrace, " > output(), Starting hierarchy output in the RHB format\n");
#endif // TRACE
	// Output the hierarchy header
	// [/Hierarchy [levels:<levels_number>] [clusters:<clusters_number>]]
	fprintf(fout, "/Hierarchy levels:%lu clusters:%lu\n", m_hier.levels().size()
		, m_hier.score().clusters);

	// Output Nodes section
	// /Nodes [<nodes_number>]
	fprintf(fout, "\n/Nodes %lu\n", m_hier.nodes().size());
	// Output node owners with shares
	// <node1_id> > <owner1_id>[:<share1>] <owner2_id>[:<share2>] ...
	// Add reference format for the readability
	fputs("# node1_id> owner1_id[:share1] owner2_id[:share2] ...\n", fout);
	for(const auto& nd: m_hier.nodes())
		outpel(nd, fout);

	// Output Level sections
	LevelNum lid = 0;  // Level id (index)
	for(const auto& lev: m_hier.levels()) {
		// /Level <level_id> [pure:<clusters>] [extended:<fullsize>]
		fprintf(fout, "\n/Level %u pure:%lu extended:%u\n", lid++
			, lev.clusters.size(), lev.fullsize);
		// Output cluster owners with the shares
		// <cluster_id> > [<cluster1_id>[:<share1>] <cluster2_id>[:<share2>] ...]
		for(const auto& cl: lev.clusters)
			outpel(cl, fout);
	}
#if TRACE >= 2
	fprintf(ftrace, " > output(), Hierarchy output in the RHB format completed\n");
#endif // TRACE
}

}  // daoc


#endif // PRINTER_RHB_HPP
