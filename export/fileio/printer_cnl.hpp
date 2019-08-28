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

#ifndef PRINTER_CNL_HPP
#define PRINTER_CNL_HPP

#include <cstdio>
#include <stdexcept>  // Exception (for Arguments processing)

#include "types.h"
#include "fileio/printer_cnl.h"


namespace daoc {

using std::advance;
using std::invalid_argument;
using std::logic_error;


// CNL Printer ----------------------------------------------------------------
template <typename LinksT>
void CnlPrinter<LinksT>::output(FileWrapper& fvec, const NodeVecCoreOptions& nvo
	, FileWrappers& fouts, ClsOutFmtBase clsfmt, bool fltMembers, LevelNum blev
	, LevelNum elev, const float levStepRatio, const SignifclsOptions* signif
#if	VALIDATE >= 2
	, const vector<LevelNum>& ilevs
#endif // VALIDATE
)
{
	// Output unwrapped clusters to the files
	if(fouts.empty() || m_hier.levels().empty()) {
#if TRACE >= 2
		fprintf(ftrace, " > WARNING output(), levels output is sipped;  fouts size: %lu"
			", hier levels: %lu\n", fouts.size(), m_hier.levels().size());
#endif // TRACE
		return;
	}
#if TRACE >= 2
	fprintf(ftrace, " > output(), Starting hierarchy output in the CNL format: %s\n"
		, strClsOutFmt(clsfmt).c_str());
	if(fvec)
		fprintf(ftrace, " > output(), Starting node vectorization:  dclnds: %u, valfmt: %s, compr: %s"
			", numbered: %u, wdimrank: %u, brief: %u, valmin: %G\n", nvo.dclnds, to_string(nvo.value).c_str()
			, to_string(nvo.compr).c_str(), nvo.numbered, nvo.wdimrank, nvo.brief, nvo.valmin);
#endif // TRACE
	const bool  withhdr = !isset(clsfmt, ClsOutFmt::PURE);  // Whether to output cnl header
	if(!withhdr)
		clsfmt = *ClsOutFmt::SIMPLE;  // To simplify processing, because these formats are the same except the header
	const bool outpnums = isset(clsfmt, ClsOutFmt::EXTENDED);  // Numbered output (prefix with cluster ids)
	const bool outpshares = outpnums || isset(clsfmt, ClsOutFmt::SHARED);  // Output unequal shares (fuzzy overlaps)
	const Id ndsnum = m_hier.nodes().size();
	const Id fltmask = 1 << sizeof(Id) * 8 - 1;  // Node id filtering mask (the highest bit)
	const LevelNum  levsnum = m_hier.levels().size();
	// Node Vectorization constants
	using DimWeight = LinkWeight;  // Node projection value (weight)
	struct DimInfo {
		Id id;  //!< Dimension (representative cluster) id
		LevelNum levid;  //!< Level id (0 for the bottom level, max for the root)
		DimWeight rdens;  //!< Ratio of the density step relative to the possibly indirect super cluster, 1 for the root
		DimWeight rweight;  //!< Ratio of the weight step relative to the possibly indirect super cluster, 1 for the root
		DimWeight wsim;  //!< Dimension significance ratio (weight) for the similarity
		DimWeight wdis;  //!< Dimension significance ratio (weight) for the dissimilarity
		bool root;  //!< Whether the cluster is root (does not have any owners/super-clusters)
	};
	using DimInfos = Items<DimInfo>;
	using DimsNum = uint16_t;
	constexpr DimsNum  dimsmax = numeric_limits<DimsNum>::max();
	const auto dclnds = max(nvo.dclnds, ndsnum);  // Declared number of nodes (fetched from the input file), >= ndsnum
	const auto valfmt = nvo.value;
	const string valfmtstr = to_string(valfmt);
	const string comprstr = to_string(nvo.compr);
	const auto valmin = nvo.valmin;
	const bool numbvec = nvo.numbered;

	// CRUCIAL Concept: Significance of the clusters in the hierarchy for the similarity of nodes
	// can be defined based on the hierarchy level ~= clusters size in nodes.
	// Nodes belonging to various root clusters and highest dissimilarity and lowest similarity,
	// and vise verse for the nodes on the bottom levels of the hierarchy (fine-grained clusters).
	// The highest similarity between the nodes have clusters consisting of 1 and 2 nodes =>
	// their significance is 1, larger clusters have lower significance.
	// (cnodes.size() / 2) ^ -0.2 allows to cover up to 2E12 cluster nodes using 1 byte
	// for the significance result and provides smooth drop of values from 3:0.922, ..., 10:0.725,
	// 100:0.457, 1000:0.289, 100'000:0.115 to 2E12:1/255=0.0039
	//! \brief Dimension (representative cluster) significance weight (0, 1]
	//!
	//! \param clndsnum Id  - number of nodes in the cluster
	//! \return DimWeight  - significance weight of the dimension (representative cluster), E [1.f / ndsnum, 1]
	auto wdimNdsnum = [](Id clndsnum) noexcept -> DimWeight {
		return clndsnum >= 3 ? powf(clndsnum / 2.f, -0.2f) : 1.f;
	};

	//! \brief Dimension (representative cluster) significance weight (0, 1]
	//!
	//! \param orank LevelNum  - rank of the cluster owners, >= 0, 0 for the root-level clusters
	//! \return DimWeight  - significance weight of the dimension (representative cluster) for the dissimilarity
	auto wdimOwnrank = [levsnum](LevelNum orank) noexcept -> DimWeight {
		//return 1.f / sqrtf(orank + 1ui + levsnum - desrank);  // Works not the best for the clusters having low desrank without wdimDesrank
		return 1.f / sqrtf(orank + 1.f);  // Works not the best for the clusters having low desrank without wdimDesrank
	};

	//! \brief Dimension (representative cluster) significance weight (0, 1]
	//! \pre desrank >= 1
	//!
	//! \param desrank LevelNum  - descendants rank (max number of descendant unfolds till the nodes), >= 1
	//! \return DimWeight  - significance weight of the dimension (representative cluster) for the similarity
	auto wdimDesrank = [](LevelNum desrank) noexcept -> DimWeight {
		//constexpr float  vadj = 1.f - log(3.f);  // ~= 0.098612289
#if VALIDATE >= 2
		assert(desrank >= 1 && "wdimDesrank(), desrank validation failed");  // should be positive
#endif // VALIDATE
		////return 1.f / sqrt(static_cast<DimWeight>(desrank));  // 0.045 for rdes=500; 0.71 for rdes=2
		//return 1.f / (log(static_cast<DimWeight>(desrank) + 2.f) + vadj);  // 0.16 for rdes=500; 0.72 for rdes=2; 0.076 for rdes=500'000
		return powf(static_cast<DimWeight>(desrank), -1.f / 3);  // 0.13 for rdes=500; 0.79 for rdes=2; 0.013 for rdes=500'000
	};

	//! \brief Output the node vectorization header
	//!
	//! \param rootdims LevelNum  - the number of dimensions (clusters) on the root level
	//! \return idimsnum size_t  - position of the dimensions number value in the file
	//! 	having reserved number of spaces to fit max dimsnum
	auto outpVecHeader = [&fvec, dclnds, &valfmtstr, &comprstr, valmin
	, numbvec](LevelNum rootdims) noexcept -> size_t {
		// Note: NVC header contains declared number of nodes in the graph,
		// which is >= actual number ndsnum  (having either weight or at least one link)
		fprintf(fvec, "# Nodes: %u, Dimensions: ", dclnds);
		auto ipos = ftell(fvec);
		// Max number of decimal digits in the number of dimensions
		// +1 to consider the value cut and +1 to consider a placeholder for the ','
		constexpr uint8_t  imax = numeric_limits<DimsNum>::digits10 + 2;
		for(uint8_t i = 0; i < imax; ++i)
			fputc(' ', fvec);
		fprintf(fvec, " Rootdims: %u, Value: %s, Compression: %s, Valmin: %G, Numbered: %u\n"
			, rootdims, valfmtstr.c_str(), comprstr.c_str(), valmin, numbvec);
		return ipos;
	};

	//! \brief Output the node vectorization footer
	//!
	//! \param dimws const DimInfos&  - dimentions (clusters) weights
	auto outpVecFooter = [&fvec](const DimInfos& dinfos) noexcept {
#if VALIDATE >= 2
		assert(!dinfos.empty() && "outpVecFooter(), non-empty dinfos is expected");
#endif // VALIDATE
		fputs("# Diminfo>", fvec);
		for(const auto& dinf: dinfos) {
			fprintf(fvec, " %u#%u%%%G/%G:%G-%G", dinf.id, dinf.levid, dinf.rdens
				, dinf.rweight, dinf.wsim, dinf.wdis);
			if(dinf.root)
				fputc('!', fvec);
		}
		fputc('\n', fvec);
	};

	//! \brief Output node projection value
	//! \pre wproj >= valmin
	//!
	//! \param nid Id  - the node id
	//! \param wproj DimWeight  - node
	//! \return bool  - the node is outputted or omitted
	auto outpNodeProj = [&fvec, valfmt, valmin](Id nid, DimWeight wproj) noexcept -> bool {
#if VALIDATE >= 2
		assert(wproj > 0 && !less<DimWeight>(wproj, valmin)
			&& "outpNodeVecVal(), node projection should be positive");
#endif // VALIDATE
		//if(!less<DimWeight>(wproj, valmin))
		//	return;
		switch(valfmt) {
		case NodeVecFmtVal::BIT:
			if(!less<DimWeight>(wproj, 0.5)) {
				fprintf(fvec, "%u ", nid);
				return true;
			}
			break;
		case NodeVecFmtVal::UINT8: {
			using VType = uint8_t;
			constexpr auto  vmax = numeric_limits<VType>::max();
			const DimWeight  corr = max<DimWeight>(valmin - 0.5f / vmax, 0);
			const VType  v = round((wproj - corr) / (1 - corr) * vmax);
			if(v) {
				fprintf(fvec, "%u:%u ", nid, vmax - v + 1);  // Required to recover val as 1./venc
				return true;
			}
		} break;
		case NodeVecFmtVal::UINT16: {
			using VType = uint16_t;
			constexpr auto  vmax = numeric_limits<VType>::max();
			const DimWeight  corr = max<DimWeight>(valmin - 0.5f / vmax, 0);
			const VType  v = round((wproj - corr) / (1 - corr) * vmax);
			if(v) {
				fprintf(fvec, "%u:%u ", nid, vmax - v + 1);  // Required to recover val as 1./venc
				return true;
			}
		} break;
		case NodeVecFmtVal::FLOAT32:
			if(!equal<DimWeight>(wproj)) {
				fprintf(fvec, "%u:%G ", nid, wproj);
				return true;
			}
			break;
		default:
			assert(0 && "outpNodeVecVal(), invalid value of the NodeVecFmtVal flag");
		}
		return false;
	};

	//! \brief Output the level header
	//!
	//! \param ifile LevelNum  - index of the target file
	//! \param clsnum Id  - the number of clusters (including propagated) in the level,
	//! 	-1 means postponed initialization, 0 means omission of the clusters number output
	//! \return iclsnum size_t  - position of the clusters number value in the file
	//! 	having reserved number of spaces to fit max clsnum or size_t(-1) if clsnum is omitted
	auto outpHeader = [&fouts, ndsnum, outpshares, outpnums](LevelNum ifile, Id clsnum) noexcept -> size_t {
		// Note: CNL header contains actual number of nodes in the graph having either weight or at least one link
		//
		//fprintf(fouts[ifile], "# Clusters: %u, Nodes: %u, Fuzzy: %u, Numbered: %u\n"
		//	, clsnum, ndsnum, outpshares, outpnums);
		auto&  fout = fouts[ifile];
		fputc('#', fout);

		size_t  ipos = -1;
		if(clsnum) {
			fputs(" Clusters: ", fout);
			ipos = ftell(fout);
			if(clsnum != static_cast<Id>(-1)) {
				fprintf(fout, "%u,", clsnum);
			} else {
				// Max number of decimal digits in the number of clusters (Id)
				// +1 to consider the value cut and +1 to consider a placeholder for the ','
				constexpr uint8_t  imax = numeric_limits<Id>::digits10 + 2;
				for(uint8_t i = 0; i < imax; ++i)
					fputc(' ', fout);
			}
		}

		fprintf(fout, "  Nodes: %u, Fuzzy: %u, Numbered: %u\n", ndsnum, outpshares, outpnums);
		return ipos;
	};

	//! \brief Output cluster to the specified file
	//!
	//! \param cl const Cluster<LinksT>&  - the cluster to be outputted
	//! \param cnodes const ClusterNodes<LinksT>&  - member nodes with shares
	//! \param fout FileWrapper&  - output file
	//! \return void
	auto outpCluster = [outpnums, outpshares, fltMembers, fltmask](const Cluster<LinksT>& cl
	, const ClusterNodes<LinksT>& cnodes, FILE* fout=stdout) noexcept {
		// Cluster id
		if(outpnums)
			fprintf(fout, "%u> ", cl.id);
		// Node shares
		bool filtered = false;  // The cluster is filtered as nonempty
		for(const auto& icn: cnodes) {  // first = dest, second = share
			// Filter out marked nodes if required
			if(fltMembers && icn.first->id & fltmask)
				continue;
			filtered = true;
			if(outpshares
			// Save share of all overlaps (0 <= share < 1)
			//&& less(icn.second, Share(1))
			// Save only unequal shares of the overlaps (fuzzy overlaps)
			&& !equalx(icn.second, Share(1) / icn.first->owners.size()
			, icn.first->owners.size()))  // Note: the number of owners is small
				fprintf(fout, "%u:%G ", icn.first->id, icn.second);
			else fprintf(fout, "%u ", icn.first->id);
		}
		if(filtered)
			fputc('\n', fout);
	};

//	//! \brief Output cluster to the specified file
//	//!
//	//! \param cl const Cluster<LinksT>&  - the cluster to be outputted
//	//! \param cnodes const ClusterNodes<LinksT>&  - member nodes with shares
//	//! \param fout FileWrapper&  - output file
//	//! \return void
//	auto outpClusterSimple = [outpnums, fltMembers, fltmask](const Cluster<LinksT>& cl
//	, const SimpleClusterNodes<LinksT>& cnodes, FILE* fout=stdout) noexcept {
//		// Cluster id
//		if(outpnums)
//			fprintf(fout, "%u> ", cl.id);
//		// Node shares
//		bool filtered = false;  // The cluster is filtered as nonempty
//		for(auto icn: cnodes) {  // first = dest, second = share
//			// Filter out marked nodes if required
//			if(fltMembers && icn->id & fltmask)
//				continue;
//			filtered = true;
//			fprintf(fout, "%u ", icn->id);
//		}
//		if(filtered)
//			fputc('\n', fout);
//	};

	const bool maxshare = isset(clsfmt, ClsOutFmt::MAXSHARE);  // Max share only from the overlapping node is required
	const ClsOutFmt  clsoutfmt = toClsOutFmt(clsfmt & ClsOutFmt::MASK_OUTSTRUCT);
	switch(clsoutfmt) {
	case ClsOutFmt::PERLEVEL:
		if(blev) {
//			throw invalid_argument(string("output(), blev (").append(std::to_string(blev))
//				.append(") should be equal to 0 for ClsOutFmt::PERLEVEL\n"));
#if VALIDATE >= 2
			assert(0 && "output(), blev = 0 is expected in PERLEVEL to output all levels");
#endif // VALIDATE
			blev = 0;  // Start from the bottom
		}  // Note: break is intentionally omitted
	case ClsOutFmt::CUSTLEVS:
	case ClsOutFmt::CUSTLEVS_APPROXNUM:
	{
		// VAlidate that levStepRatio E [0, 1), where 0 means skip this parameter
		if(levStepRatio < 0 || levStepRatio > 1)
			throw invalid_argument(string("output(), levStepRatio validation failed: ")
				.append(std::to_string(levStepRatio)) += '\n');
		const LevelNum  levnum = fouts.size();  // The number of levels to be outputted
#if VALIDATE >= 2
		assert((elev == LEVEL_NONE || (elev >= blev + levnum  // Note: >= because stepRatio is < 1
			 && elev <= levsnum)) && "output(), elev validation failed");
#endif // VALIDATE
		// Start from the bottom level and output each cluster to
		// all corresponding files tracing propagated items
		if(blev >= levsnum || levnum > levsnum)
			throw invalid_argument(string("output(), blev (").append(std::to_string(blev))
				.append(") or the number of fouts (").append(std::to_string(levnum))
				.append(") is too large (max = ").append(std::to_string(levsnum)).append(")\n"));
		// Define target levels if required
		vector<LevelNum>  targlevs;  // Note: usually levnum is small
		if(levStepRatio < 1) {
			targlevs.reserve(levnum);
			auto levi = blev;  // Push levels from the bottom
			auto ihl = m_hier.levels().begin();
			advance(ihl, levi);  // First outputting level from the hierarchy bottom
			targlevs.push_back(levi++);
			auto lsizeMarg = ihl++->fullsize * levStepRatio;
			auto levsrem = levnum - 1;  // Remained number of levels to be outputted
			if(elev == LEVEL_NONE)
				elev = levsnum;
			for(; levsrem && levi < elev; ++levi, ++ihl)
				if(ihl->fullsize <= lsizeMarg) {
					targlevs.push_back(levi);
					lsizeMarg = ihl->fullsize * levStepRatio;
					--levsrem;
				}
			// ATTENTION: last level might be reserved for the top margin and does not
			// correspond to the levStepRatio
			if(levsrem && targlevs.back() != --levi) {  // Note: -- to compensate last ++
				targlevs.push_back(levi);
				--levsrem;
			}
			if(levsrem)
				throw logic_error("output(), fouts size exceeds the number of"
					" the hierarchy levels\n");
#if VALIDATE >= 2
			assert(targlevs == ilevs && "output(), targlevs validation failed");
#endif // VALIDATE
		} else if(elev == LEVEL_NONE)
			elev = blev + levnum;  // End index (past the last) of the outputting level from the bottom
//#if VALIDATE >= 2
//		assert(elev >= blev + levnum && elev <= levsnum
//			&& "output() 2, elev validation failed");
//#endif // VALIDATE
		if(withhdr) {
			// Output the header to the respective files
            if(targlevs.empty()) {
				auto ihl = m_hier.levels().begin();
				advance(ihl, blev);  // First outputting level from the hierarchy bottom
				for(auto levi = blev; levi < elev; ++levi)
					outpHeader(levi - blev, ihl++->fullsize);
            } else {
            	LevelNum  ilevPrev = 0;  // Index of the previously outputted level
            	auto  ihl = m_hier.levels().begin();
            	LevelNum  filei = 0;
            	for(auto levi: targlevs) {
#if VALIDATE >= 2
					assert(levi >= ilevPrev && "output(), levels index validation failed");
#endif // VALIDATE
					advance(ihl, levi - ilevPrev);
					ilevPrev = levi;
					outpHeader(filei++, ihl->fullsize);
            	}
            }
		}
		// Index of the processing level starting from the bottom to not loose the propagated items
		LevelNum  levi = 0;
		const auto ietargLev = targlevs.end();
		auto ibtargLev = targlevs.begin();  // Iterator to the target level
		LevelNum bfilei = 0;  // First file index
#if VALIDATE >= 2
		assert((ibtargLev == ietargLev || *ibtargLev == blev)
			&& "output(), ibtargLev validation failed");
#endif // VALIDATE
		for(auto& lev: m_hier.levels()) {
			// Omit levels out of the available files
			if(levi >= elev)
				break;
			if(!targlevs.empty()) {
				while(*ibtargLev < levi) {
					++ibtargLev;
					++bfilei;
#if VALIDATE >= 2
					assert(ibtargLev != targlevs.end() && distance(targlevs.begin(), ibtargLev) == bfilei
						&& "output(), targlevs are not synced with elev");
#endif // VALIDATE
				}
			}
			for(auto& cl: lev.clusters) {
#if VALIDATE >= 2
				if(cl.levnum != levi) {
					fprintf(ftrace, "  > output(), #%u  %lu owners, levnum: %u, levi: %u\n"
						, cl.id, cl.owners.size(), cl.levnum, levi);
					assert(0 && "output(), levnum should correspond to the"
						" currently traversing level");
				}
				if(!targlevs.empty())
					assert(*ibtargLev >= cl.levnum && "output(), ibtargLev validation failed");
#endif // VALIDATE
				// Output the cluster starting from its levnum and stopping before the elevCur
				LevelNum  elevCur = elev;
				if(!cl.owners.empty() && elevCur > cl.owners.front().dest->levnum) {
					elevCur = cl.owners.front().dest->levnum;
					// Omit levels before the available files
					if(elevCur <= blev)
						continue;
				}
				// Fetch nodes shares and output them
				if(targlevs.empty()) {
					// Omit levels before the available files
					for(LevelNum clev = levi >= blev ? levi : blev; clev < elevCur; ++clev)  // Note: levi == cl.levnum
						outpCluster(cl, m_hier.unwrap(cl, maxshare), fouts[clev - blev]);
				} else {
					// Omit levels before the available files
					// NOTE: above is verified that:  *ibtargLev >= blev
					auto filei = bfilei;
					for(auto itargLev = ibtargLev; itargLev != ietargLev && *itargLev < elevCur; ++itargLev, ++filei)
						outpCluster(cl, m_hier.unwrap(cl, maxshare), fouts[filei]);
				}
			}
			++levi;
		}
	} break;
	case ClsOutFmt::ALLCLS:
		// Output all the hierarchy into the single file skipping the node wrappers and
		// propagated (except the root level)
		if(withhdr) {
			// Note: including wrapped non-clustered nodes
			outpHeader(0, m_hier.score().clusters);
		}
		for(auto& lev: m_hier.levels()) {
			for(auto& cl: lev.clusters) {
				if(cl.des.size() >= 2 || cl.des.front()->owners.size() >= 2  // not a node wrapper
				|| cl.owners.empty())  // root cluster
					outpCluster(cl, m_hier.unwrap(cl, maxshare), fouts.front());
			}
		}
		break;
	case ClsOutFmt::SIGNIF_OWNSDIR:
	case ClsOutFmt::SIGNIF_OWNADIR:
	case ClsOutFmt::SIGNIF_OWNSHIER:
	case ClsOutFmt::SIGNIF_OWNAHIER:
	case ClsOutFmt::SIGNIF_DEFAULT:
	{
		assert(signif && "output(), significant clusters output options should be specified");
		const SignifclsOptions  sgdfl(clsoutfmt == ClsOutFmt::SIGNIF_DEFAULT);  // Default options for the significant clusters output
		if(!signif)
			signif = &sgdfl;
		// Note: SIGNIF_DEFAULT is threated as SIGNIF_OWNSDIR (the least resource consuming)
		const ClsOutFmt  cofmt = clsoutfmt != ClsOutFmt::SIGNIF_DEFAULT
			? clsoutfmt : ClsOutFmt::SIGNIF_OWNSDIR;
		// Validate signif cls options
#if VALIDATE >= 1
		signif->validate();  // Throws exception on failed validation
#endif // VALIDATE 1
		const bool  owall = cofmt == ClsOutFmt::SIGNIF_OWNADIR
			|| cofmt == ClsOutFmt::SIGNIF_OWNAHIER;
		const bool  owhier = cofmt == ClsOutFmt::SIGNIF_OWNSHIER
			|| cofmt == ClsOutFmt::SIGNIF_OWNAHIER;
		//! Density of the cluster: self-weight ratio related to the full weight
		using Density = LinkWeight;
		//! Weight margin
		using Weight = LinkWeight;
		//! Constraints of the cluster
		struct OwnerConstraints {
			Density  dens;  //!< Minimal density constraint of the cluster
			// Note: the maximal weight is required to filter out outliers (skip some dense clusters) by demand
			Weight  weight;  //!< Maximal weight constraint of the cluster (weight of the outputted [indirect] owner)
			Id  reqs;  //!< The number of request from distinct des, <= des.size()

            //! OwnerConstraints constructor
			OwnerConstraints(Density dens, Weight weight)
			: dens(dens), weight(weight), reqs(0)  {}
		};
		//! Clusters constraints
		using ClusterCsts = unordered_map<const Cluster<LinksT>*, OwnerConstraints, SolidHash<const Cluster<LinksT>*>>;
		// Cluster density
		// Preallocate taking second level from the bottom if exists, otherwise root.
		// Second level is taken because we are interested in the owners of the widest
		// (bottom) level.
		ClusterCsts  clcsts(levsnum >= 2
			? (++m_hier.levels().begin())->clusters.size() : m_hier.root().size());

		const auto densdrop = signif->densdrop;  // Density drop for the desc clusters
		const auto densbound = signif->densbound;  // Density drop is bottom bounded and interpreted differently
		const auto wrstep = signif->wrstep;  // Weight ratio step for the desc clusters
		const auto wrange = signif->wrange;  // Use wrstep as weight drop range E [1-wrstep, wrstep], wrstep E (0.5, 1)
		const auto sowner = signif->sowner;  // A single owner only is required for the representative non-root clusters
#if VALIDATE >= 2
		// Note: verified by signif->validate(), assert is just additional verification
		assert(wrstep > (wrange ? 0.5 : 0) && wrstep <= 1 && "output(), wrstep is invalid");
#endif // VALIDATE

		//! \brief Check whether the cluster is representative (significant)
		//! \note rdens and rweight are evaluated ONLY for the representative clusters,
		//! 	otherwise their values are not defined
		//!
		//! \param[in] cl const Cluster<LinksT>&  - the cluster to be analyzed
		////! \param[in] orank LevelNum  - owners rank (the longest number of owners till the root), <= levsnum
		//! \param[in] levind LevelNum  - level index starting from the root as 0
		//! \param[out] rdens DimWeight&  - ratio of the cluster density step relative
		//!		to the possibly indirect super cluster, >= densdrop
		//! \param[out] rweight DimWeight&  - ratio of the cluster weight step relative
		//!		to the possibly indirect super cluster, >= wrstep
		auto reprcl = [&clcsts, owall, owhier, densdrop, densbound, wrstep, wrange, sowner, levsnum]
		(const Cluster<LinksT>& cl, LevelNum levind, DimWeight& rdens, DimWeight& rweight) -> bool {
			//static const auto  densdrop1 = 1 - densdrop;
			bool res = cl.owners.empty();  // The cluster is representative (significant)
			const Weight  weight = cl.weight();  // Internal (self-weight) of the cluster
			// Note: weight / cl.nnodes()  - density of links in the cluster,
			// cl.ctxWeight(false) - Full weight of the cluster
			//// ATTENTION: ctxWeight(false) to load saved lower accuracy weight for the tidied context
			//const Density  dens = weight / cl.ctxWeight(false);  // Density of the current cluster
			const Density  dens = weight / cl.nnodes();  // Density of the current cluster
			Density  savdens = 0;  // Saving density for the current cluster (or its hier owners)
			Weight  savwgh = 0;  // Saving weight for the current cluster (or its hier owners)
			if(!res) {
				Coupling  matched = 0;  // The number of times the cluster matched the output constraints, <= owners.size()
#if VALIDATE >= 2
				assert(cl.nnodes() > 0 && "reprcl(), cluster member nodes number is invalid");
#endif // VALIDATE
				// Note: traversing over all owners is required at least to mark them as requested
				for(const auto& ow: cl.owners) {
					auto &ocst = clcsts.at(ow.dest);  // Owner cluster density and requests number
					// NOTE: saveX should be always defined to initialize rX outside this cycle
					// Save the highest / lowest density of the owner clusters and considering
					// the weight constraints (but giving priority to the density constraints)
					// Fetch max owners density for owall, and min density for the single owner,
					// min owners weight for owall, and max weight for the single owner
					if((!savdens || (owall ? !less(ocst.dens, savdens) : !less(savdens, ocst.dens)))
					// For all owners select the most lightweight, which is the strictest policy
					&& (!savwgh || (owall ? !less(savwgh, ocst.weight) : !less(ocst.weight, savwgh)))) {
						savdens = ocst.dens;
						savwgh = ocst.weight;
					}
					// Compare the cluster density with the owner density if required
					if(!owhier && (owall || !matched) && !less(dens, ocst.dens) && !less(ocst.weight, weight)
					&& (!wrange || !less(weight, ocst.weight * ((1 - wrstep) / wrstep))))
						++matched;
					// Remove from clcsts [owner] clusters having all des traversed
					if(++ocst.reqs == ow.dest->descs()->size())
						clcsts.erase(ow.dest);
				}
				if((cl.owners.size() == 1 || !sowner) && (!owhier
				? matched == (owall ? cl.owners.size() : 1) : !less(dens, savdens) && !less(savwgh, weight)
				&& (!wrange || !less(weight, savwgh * ((1 - wrstep) / wrstep))))) {
					rdens = dens / savdens;
					rweight = weight / savwgh;
					res = true;
				}
#if TRACE >= 2
				fprintf(ftrace, "  >> reprcl(), #%d dens: %G (w: %G, n: %G) [%G], res: %d (matches: %d / %lu)\n"
					, cl.id, dens, weight, cl.nnodes(), cl.weight() / cl.ctxWeight(false)
					, res, matched, owall ? cl.owners.size() : 1);
#endif // TRACE
			} else {
				if(densbound)
					savdens = dens;
				rdens = 1;
				rweight = 1;
#if TRACE >= 2
				fprintf(ftrace, "  >> reprcl(), root #%d dens: %G (w: %G, n: %G) [%G], res: %d\n"
					, cl.id, dens, weight, cl.nnodes(), cl.weight() / cl.ctxWeight(false), res);
#endif // TRACE
			}
			// Correct saving density considering the cluster density
			if(densbound) {
				savdens *= (1 - (levind * (1 - densdrop) / levsnum));
#if VALIDATE >= 2
				assert(levind < levsnum
					&& !less<Density>(max<Density>(1, densdrop), (1 - (levind * (1 - densdrop) / levsnum)))
					&& !less<Density>((1 - (levind * (1 - densdrop) / levsnum)), min<Density>(densdrop, 1))
#if VALIDATE >= 3
					&& (levind || equal<Density>(1, savdens))
#endif // VALIDATE
					&& "reprcl(), levind or savdens is out of the range");
#endif // VALIDATE
			}
			if(!owhier || res) {
				// Change interpretation of densdrop if required
				if(!densbound)
					savdens = dens * densdrop;
				savwgh = weight * wrstep;
			}
			// Insert current cluster to the clcsts
			if(cl.des.size() >= 2) {
#if VALIDATE >= 2
				if(!((savdens || !densdrop) && savwgh)) {
					fprintf(ftrace, "  >> reprcl(), #%u  savdens: %G, savwgh: %G, owhier: %u, owall: %u\n"
						, cl.id, savdens, savwgh, owhier, owall);
					throw logic_error("reprcl(), positive savdens && savwgh are expected\n");
				}
				bool inserted =
#endif // VALIDATE
				clcsts.emplace(&cl, OwnerConstraints(savdens, savwgh))
#if VALIDATE >= 2
				.second;
				assert(inserted && "reprcl(), each cluster should be inserted only once")
#endif // VALIDATE
				;
			}
			return res;
		};

		// Output all the hierarchy into the single file skipping the node wrappers and
		// propagated (except the root level) starting from the root level
		size_t  clspos = -1;  // Position of the clsnum value in the header
		if(withhdr) {
			// Note: including wrapped non-clustered nodes
			clspos = outpHeader(0, -1);  // Note: -1 means put a placeholder spaces instead of the actual clsnum
		}
		Id  clsnum = 0;  // The number of outputted clusters

		// Node Vectorization related variables
		//! Clusters owner rank (max number of owners till the root level)
		using ClusterRanks = unordered_map<Id, LevelNum>;
		const bool  wdimranked = nvo.wdimrank;  // Weight dimensions by the cluster owners rank or by the cluster weight
		size_t  dimspos = 0;  // Position of the dimensions number in the header
		if(fvec)
			dimspos = outpVecHeader(m_hier.root().size());
		ClusterRanks  cranks;  // Cluster ranks, required to evaluate the local rank for each representative cluster
		DimInfos  dinfos;  // Dimension (cluster) information
		if(fvec && !nvo.brief) {
			dinfos.reserve(sqrtf(m_hier.nodes().size()) / 2);
			if(wdimranked)
				cranks.reserve(m_hier.score().clusters - m_hier.levels().back().clusters.size());
		}
		ClusterNodes<LinksT>  lnds;  // Projections of the linked external nodes to the processing cluster
		LevelNum  levind = 0;  // Index of the processing level
		DimsNum  dimsnum = 0;  // The number of dimensions
		DimWeight  rdens, rweight;  // Resulting ratios of the cluster density (>= densdrop) and weight (<= wrstep)

		const auto& erlev = m_hier.levels().rend();
		const auto& lrlev = erlev - 1;
#if TRACE >= 2
		fputs("  > output(), filtered out as non-significant #: ", ftrace);
		Id szfltcs = 0;  // The number of clusters omitted from the output by the min size constraint
#endif // TRACE
		for(auto ilev = m_hier.levels().rbegin(); ilev != erlev; ++ilev, ++levind) {
			for(auto& cl: ilev->clusters) {
				LevelNum  orank = 0;  //!< Owners rank (the longest number of owners till the root), <= levsnum
				if(fvec && wdimranked && !nvo.brief) {
					for(auto& ocl: cl.owners)
						if(orank < cranks.at(ocl.dest->id))
							orank = cranks[ocl.dest->id];
					// Do not store cluster ranks of the bottom level because they can not be owners of other clusters
					if(ilev != lrlev)
						cranks[cl.id] = !cl.owners.empty() ? ++orank : 0;
				}
				// Not a node wrapper or root cluster
				// NOTE:
				// - reprcl() is executed also for the root clusters to save their
				// density to be used for the lower levels output
				// - Just shared wrapped nodes are not considered as representative
				// clusters except the root clusters.
				if((cl.des.size() >= 2 || cl.owners.empty()) && reprcl(cl, levind, rdens, rweight)) {
					LevelNum  desrank = 0;  //!< Descendant rank (the longest number of descendant till the bottom level), <= levsnum
					const ClusterNodes<LinksT>& cnodes = m_hier.unwrap(cl, maxshare, &desrank);
					// Consider min nodes cluster size constraint for the non-root clusters
					if(cnodes.size() >= signif->szmin || cl.owners.empty()) {
						outpCluster(cl, cnodes, fouts.front());
						++clsnum;
						if(fvec && dimsnum < dimsmax) {
							bool  outpdim = false;  // The dimension contains significant node projections and will be outputted
							// The expected number of linked external nodes is:
							// |c| * d * (1 - |c| / N) / d * (1 - Q_c),
							// where Q_c ~= Q_root * (nlevs - ilev) / nlevs given that ilev_root = 0
							lnds.reserve(cnodes.size() * (1 - static_cast<AccWeight>(cnodes.size()) / ndsnum)
								* (1 - m_hier.score().modularity * static_cast<AccWeight>(levsnum - levind) / levsnum));
							// The node represents a root cluster as a node wrapper
							const bool  rootnode = cnodes.size() == 1;
#if VALIDATE >= 2
								assert((!rootnode || cl.owners.empty())
									&& "output(), only a root cluster may represent a wrapped node");
#endif // VALIDATE
							AccWeight  wcorr = 0;  // Weight correction present in the wrapped root nodes
							// Evaluate projection of the cluster node to the cluster as the relative
							// in-cluster weight (self-weight + links) and accumulate projections of the linked nodes
							for(const auto& cnd: cnodes) {
								const auto& nd = *cnd.first;
								// Note: doubled node self weight is used since the links counted twice later
								AccWeight  wproj = nd.weight();  // Projected in-cluster weight of the node
#if VALIDATE >= 2
								AccWeight  wrem = 0;  // Remained external weight of the node
#endif // VALIDATE

								for(const auto& ln: nd.links) {
									if(cnodes.count(ln.dest)) {
										wproj += ln.weight;
									} else {
#if VALIDATE >= 2
										wrem += ln.weight;
#endif // VALIDATE
										lnds[ln.dest] += ln.weight;
									}
								}

								// Output node
#if VALIDATE >= 2
								assert(equal<DimWeight>(wproj + wrem, nd.ctxWeight())
									&& "output(), node weight validation failed");
#endif // VALIDATE
								// NOTE: projection of the wrapped root nodes should be nonzero even if their weight is 0
								if(rootnode && !wproj) {
									// Take wproj as 0.5 if links are present to share half of it among other links
									// and dedicate round(Xmin) = 1 to the cluster => Xmin = 0.5
									wproj = wcorr = nd.links.empty() ? 1 : 0.5;
								} else wproj /= nd.ctxWeight();
								if(nvo.compr != NodeVecFmtCompr::CLUSTER) {
									assert(0 && "output(), non Cluster compression of the node vectorization is not implemented");
									continue;
								}
#if TRACE >= 3
								if(cnodes.size() == 1)
									fprintf(ftrace, "> output(), wrapped node #%u, wproj: %G, valmin: %G, outp: %u\n"
										, nd.id, wproj, valmin, !less<DimWeight>(wproj, valmin));
#endif // TRACE
								if(!less<DimWeight>(wproj, valmin)) {
									// Output dimension (cluster) id if required
									if(numbvec && !outpdim)
										fprintf(fvec, "%u> ", cl.id);
									// Note: outpdim should be set to true also for !numbvec
									outpdim |= outpNodeProj(nd.id, wproj);
								}
							}
							// Add accumulated external projections
#if VALIDATE >= 2
							assert((lnds.empty() || outpdim) && "output(), external projections are expected"
								" to be outputted only with the internal projections");
#endif // VALIDATE
							for(const auto& lnd: lnds) {
								const DimWeight  weight = lnd.second / (lnd.first->ctxWeight() + wcorr);
								if(!less<DimWeight>(weight, valmin))
									outpdim |= outpNodeProj(lnd.first->id, weight);
							}
							lnds.clear();
							if(outpdim) {
								fputc('\n', fvec);
								++dimsnum;
								// Add cluster to the vector to identify its index;
								// Note: levels are enumerated from 1, so num - rindex corresponds to the correct levid
								if(!nvo.brief) {
									if(wdimranked) {
										dinfos.push_back({cl.id, levsnum - levind, rdens, rweight
											, wdimDesrank(desrank), wdimOwnrank(orank), cl.owners.empty()});
									} else {
										const DimWeight  wsim = wdimNdsnum(cnodes.size());
										const DimWeight  wdis = 1 - wsim + 1.f / ndsnum;
#if VALIDATE >= 2
										assert(wdis <= 1 && "output(), wdis is invalid");
#endif // VALIDATE
										dinfos.push_back({cl.id, levsnum - levind, rdens, rweight
											, wsim, wdis*wdis, cl.owners.empty()});
									}
								}
							}

						}
					}
#if TRACE >= 2
					else {
						++szfltcs;
						fprintf(ftrace, " %u", cl.id);
					}
#endif // TRACE
				}
				// Put correct number of clusters to the header
				if(clspos != static_cast<decltype(clspos)>(-1)) {
					auto&  fout = fouts[0];
#if VALIDATE >= 2
					assert(fout && "output(), output file descriptor should be valid");
#endif // VALIDATE
					fseek(fout, clspos, SEEK_SET);
					fprintf(fout, "%u,", clsnum);
					fseek(fout, 0, SEEK_END);
				}
			}
		}
#if TRACE >= 2
		fprintf(ftrace, "\n> output(), %u significant cls filtered out from the output\n", szfltcs);
#endif // TRACE
		if(fvec) {
#if VALIDATE >= 2
			assert(dinfos.size() == dimsnum && "output(), validation of the number of dimensions failed");
#endif // VALIDATE
			if(!nvo.brief)
				outpVecFooter(dinfos);
			// Fill the dimensions number placeholder
			if(dimspos) {
				fseek(fvec, dimspos, SEEK_SET);
				fprintf(fvec, "%u,", dimsnum);
				fseek(fvec, 0, SEEK_END);
				assert(dimsnum >= m_hier.root().size()
					&& "output(), the total number of dimensions should not be less than the number of root dimensions");
			}
		}
	} break;
	case ClsOutFmt::ROOT:
		// Output root hierarchy clusters
		if(withhdr)
			outpHeader(0, m_hier.root().size());
		for(auto cl: m_hier.root())
			outpCluster(*cl, m_hier.unwrap(*cl, maxshare), fouts.front());
		break;
	default:
		throw invalid_argument(string("output(), undefined ClsOutFmt: ")
			.append(to_string(toClsOutFmt(clsfmt), true)) += '\n');
	}
#if TRACE >= 2
	fprintf(ftrace, " > output(), Hierarchy output in the CNL format completed\n");
#endif // TRACE
}

}  // daoc

#endif // PRINTER_CNL_HPP
