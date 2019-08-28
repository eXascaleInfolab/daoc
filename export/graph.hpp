//! \brief Input graph (network of pairwise relations, similarity matrix).
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! >	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2014-07-16

#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <stdexcept>
#include <random>
#include <unordered_set>
#include <algorithm>  // sort(), swap(), max(), move[container items]()

#if TRACE >= 1
#include <cstdio>
#endif // TRACE

#include "operations.hpp"
#include "functionality.h"
#include "graph.h"

using std::out_of_range;
using std::invalid_argument;
using std::runtime_error;
using std::logic_error;
using std::random_device;
using std::enable_if_t;
using std::unordered_set;
using std::sort;
using std::min;
using std::forward;
using namespace daoc;


// External Interface for Data Input ------------------------------------------
#ifdef __clang__
// Note: Required by CLang for the linking
template <bool WEIGHTED>
constexpr typename InpLink<WEIGHTED>::Weight  InpLink<WEIGHTED>::weight;  // = SimpleLink<typename InpLink<false>::Weight>::weight;
#endif // __clang__

// Accessory routines ---------------------------------------------------------
constexpr static double  rhszmul = 1.5;  // Rehashing size multiplier to reduce the frequency of subsequent rehashes;  1.5 or 1.25

static random_device  rdev;  // Random numbers generator

//! \brief Add Link(dst, weight) to the src links keeping the order
//! \pre src links are ordered
//! \pre The link should not be a selflink: src != dst
//!
//! \param src NodeT*  - source node which links are extended
//! \param dst NodeT*  - dest node
//! \param weight WeightT  - links weight to the dest node
//! \param sumdups bool  - accumulate (sum) weight of duplicated links or just skip them
//! \param errs StructLinkErrors*  - occurred accumulated errors to be reported by the caller
//! \return void
template <typename NodeT, typename WeightT>
enable_if_t<NodeT::links_type::value_type::IS_WEIGHTED>
insertLink(NodeT* src, NodeT* dst, WeightT weight, bool sumdups, StructLinkErrors* errs)
{
#if VALIDATE >= 2
	assert(src != dst && "insertLink(), a non-selflink is expected");
#endif
	auto& links = src->links;
	if(!links.empty() && cmpBase(dst, links.back().dest)) {
		const auto iend = --links.end();
		auto iln = fast_ifind(links.begin(), iend, dst, bsObjsDest<decltype(src->links)>);
		if(iln == iend || iln->dest != dst)
			links.emplace(iln, dst, weight);
		else {
			if(sumdups)
				iln->weight += weight;
			else if (errs)
				errs->add(LinkSrcDstId(src->id, dst->id));  // Duplicated link
		}
	} else if(links.empty() || cmpBase(links.back().dest, dst))  // The appending link is unique
		links.emplace_back(dst, weight);
	else if (errs)
		errs->add(LinkSrcDstId(src->id, dst->id));  // Duplicated link
}

//! \copydoc insertLink
template <typename NodeT, typename WeightT>
enable_if_t<!NodeT::links_type::value_type::IS_WEIGHTED>
insertLink(NodeT* src, NodeT* dst, WeightT weight, bool sumdups, StructLinkErrors* errs)
{
#if VALIDATE >= 2
	assert(src != dst && (weight == SimpleLink<WeightT>::weight)
		&& "insertLink(), an unweighted non-selflink is expected");
#endif
	auto& links = src->links;
	if(!links.empty() && cmpBase(dst, links.back().dest)) {
		const auto iend = --links.end();
		auto iln = fast_ifind(links.begin(), iend, dst, bsObjsDest<decltype(src->links)>);
		if(iln == iend || iln->dest != dst)
			links.emplace(iln, dst);
		// Note: sumdups is applicable only for the weighted links, not here
		else if (errs)
			errs->add(LinkSrcDstId(src->id, dst->id));  // Duplicated link
	} else if(links.empty() || cmpBase(links.back().dest, dst))  // The appending link is unique
		links.emplace_back(dst);
	else if (errs)
		errs->add(LinkSrcDstId(src->id, dst->id));  // Duplicated link
}

//! \brief Add a new node if not exists yet
//!
//! \param nodes NodesT&  - internal nodes that can be extended
//! \param idNodes IdNodesT&  - external id / internal nodes mapping
//! \param nid Id  - node id
//! \param errs=nullptr StructNodeErrors*  - occurred accumulated errors to be reported by the caller
//! \return typename IdNodesT::iterator  - mapping iterator of the node
template <typename NodesT, typename IdNodesT>
inline typename IdNodesT::iterator acsAddNode(NodesT& nodes, IdNodesT& idNodes
	, Id nid, StructNodeErrors* errs=nullptr)
{
	// Note: the node most likely have not been added to the nodes yet, but could
	// be already added. In advance in place construction can't be used, because
	// requires &nodes.back() for the nodes that might not been added yet.
	auto ind = idNodes.find(nid);
	if(ind == idNodes.end()) {
		nodes.emplace_back(nid);
		ind = idNodes.emplace(nid, &nodes.back()).first;
	} else if(errs) {
		errs->add(nid);
		//fprintf(ftrace, "WARNING acsAddNode(), #%u already exists, the duplicate is skipped\n", nid);
	}
	return ind;
}

//! \brief Add nodes from the user input
//!
//! \param nodes NodesT&  - internal nodes to be extended
//! \param idNodes IdNodesT&  - mapping of the node Ids into the nodes
//! \param nodesIds const NodesIdsT&  - external node ids to be used for the nodes creation
//! \param shuffle bool  - shuffle (rand reorder) nodes and links
//! \param errs=nullptr StructNodeErrors*  - occurred accumulated errors to be reported by the caller
//! \return void
template <typename NodesT, typename IdNodesT, typename NodesIdsT>
void acsAddNodes(NodesT& nodes, IdNodesT& idNodes, const NodesIdsT& nodesIds
	, bool shuffle, StructNodeErrors* errs=nullptr)
{
	if(!nodesIds.size()) {  // Note: initializer_list does not have .empty(), it has only the size()
		assert(0 && "acsAddNodes(), node ids are empty");
		return;
	}
	// Fill nodes and mapping id -> nodePtr
	// Note: reserve ALWAYS rehashes the unordered map
	const size_t  idnsCapacity = idNodes.bucket_count() * idNodes.max_load_factor();
	if(idnsCapacity < idNodes.size() + nodesIds.size()) {  // Note: idNodes might already contain some of nodesIds
		// Check the number of new among the being added nodes
		Id  newNum = 0;
		for(auto nid: nodesIds)
			if(idNodes.count(nid))
				++newNum;
		if(idnsCapacity < idNodes.size() + newNum)
			idNodes.reserve(nodesIds.size() * rhszmul + newNum);  // Note: use + to reduce the number of rehashes on subsequent calls
	}

	if(shuffle) {
		// Create nodes not subsequently in time to have different orders of
		// addresses in memory and ids
		auto ib = nodesIds.begin();  // Iterator to begin
		// ATTENTION: initializer_list iterators doesn't support decrement, so -1 is used.
		auto il = nodesIds.end();  // Iterator to the last item

		while(ib != il) {  // Note: nodesIds are guaranteed to be non-empty
			auto ic = rdev() % 2 ? ib++ : --il;  // Iterator to the current item
			acsAddNode(nodes, idNodes, *ic, errs);
		}
	} else for(auto nid: nodesIds)
		acsAddNode(nodes, idNodes, nid, errs);
}

//! \brief Add link to the node
//! \attention All self links (edges and arcs) are always treated as edges (doubled) to be
//! 	to be consistent with the subsequent (self) weight aggregation on clusters formation
//!
//! \tparam DIRECTED  - whether links are directed
//!
//! \param nd NodeT*  - the node to be updated
//! \param dst NodeT*  - destination node for the link
//! \param weight WeightT  - link weight
//! \param sumdups bool  - accumulate weight of duplicated links or just skip them
//! \param errs StructLinkErrors*  - occurred accumulated errors to be reported by the caller
//! \return bool  - directed non-self link added
// ATTENTION: interpretation of the directed links should be synced with acsReduceLinks()
template <bool DIRECTED, typename NodeT, typename WeightT>
bool acsAddNodeLink(NodeT* nd, NodeT* dst, WeightT weight, bool sumdups, StructLinkErrors* errs)
{
	static_assert(is_floating_point<WeightT>::value
		, "acsAddNodeLink(), WeightT should be a floating point type");
	// Consider self-link
	if(dst == nd) {
		// Note: self-weight can be specified via arc and edges in a unified way,
		// edges does not double it
		if(!dst->weight() || sumdups)
			// Note: node weight exists even for the unweighted links,
			// *2 because self-weight is doubled.
			// Required to specify selfweight equally via the edges and arcs
			dst->addWeight(weight * static_cast<AccWeight>(2));
		// Note: sumdups is applicable only for the weighted links and self-links
		else if(errs)
			errs->add(LinkSrcDstId(nd->id, dst->id));  // Duplicated self-link
		return false;
	}
	if(!DIRECTED) {
		static_assert(is_floating_point<WeightT>::value
			, "acsAddNodeLink(), WeightT should be a floating point type");
		// Note: bidirectional links must be defined anyway and the original weight
		// should be retained. So original weight for the links (including self-link) is set
		insertLink(dst, nd, weight, sumdups, errs);
		insertLink(nd, dst, weight, sumdups, errs);
		return false;
	} else insertLink(nd, dst, weight, sumdups, errs);
#if VALIDATE >= 3
	if(!sorted(nd->links.begin(), nd->links.end(), bsDest<typename NodeT::links_type::value_type>, true)) {
		if(errs)
			errs->show();
		fprintf(ftrace, "> acsAddNodeLink(), #%u(%p) has duplicates among ", nd->id, nd);
		traceItems(nd->links, nullptr, "links", destId, destAddr);
		throw invalid_argument("acsAddNodeLink(), node links should be sorted and unique\n");
	}
	if(!sorted(dst->links.begin(), dst->links.end(), bsDest<typename NodeT::links_type::value_type>, true)) {
		if(errs)
			errs->show();
		fprintf(ftrace, "> acsAddNodeLink() dst, #%u(%p) has duplicates among ", dst->id, dst);
		traceItems(dst->links, nullptr, "links", destId, destAddr);
		throw invalid_argument("acsAddNodeLink() dst, node links should be sorted and unique\n");
	}
#endif

	return true;
}

//! \brief Reduce input links moving non significant links to the node weight
//!
//! \tparam DIRECTED  - whether links are directed
//!
//! \param node NodeT&  - node, source of the links
//! \param links InpLinksT&  - the input links to be reduced
//! \param idNodes const IdNodesT&  - external id / internal nodes mapping
//! \param reduction Reduction  - core node links reduction policy
//! \param rlsmin Id  - min number of links to retain, 0 means omit the reduction
//! \param sumdups bool  - accumulate weight of duplicated links or just skip them
//! \param errs StructLinkErrors*  - occurred accumulated errors to be reported by the caller
//! \return void
////! \return typename InpLinksT::iterator  - end iterator of the reduction range
template <bool DIRECTED, typename NodeT, typename InpLinksT, typename IdNodesT>
enable_if_t<!(DIRECTED && InpLinksT::value_type::IS_WEIGHTED)>
acsReduceLinks(NodeT& node, InpLinksT& links, const IdNodesT& idNodes
	, Reduction reduction, Id rlsmin, bool sumdups, StructLinkErrors* errs)
{
	// Note: unweighted links can't be reduced
	throw logic_error("acsReduceLinks() should not be called for the unweighed or undirected node links\n");
}

// NOTE: only directed weighted links can be reduced by the input graph
template <bool DIRECTED, typename NodeT, typename InpLinksT, typename IdNodesT>
enable_if_t<DIRECTED && InpLinksT::value_type::IS_WEIGHTED>
acsReduceLinks(NodeT& node, InpLinksT& links, const IdNodesT& idNodes
	, Reduction reduction, Id rlsmin, bool sumdups, StructLinkErrors* errs)
{
	// Note: this function should be synced with reduceLinks()
	// Note: the links are weighted
	// Note: typically rlsmin = 0 if reduction == Reduction::NONE
	if(!rlsmin || links.size() <= rlsmin || reduction == Reduction::NONE) {
#if VALIDATE >= 2
		assert(0 && "acsReduceLinks(), redundant function call");
#endif // VALIDATE
		return;
	}
	// Sort links by the increasing weights
	sort(links.begin(), links.end(), cmpObjsWeight<InpLinksT, LinkWeight>);
	const bool accurate = reduction == Reduction::ACCURATE;
	const Id lsnum = links.size();
	// Heavy weights accumulation functions
	// Note: SEVERE policy requires ratio closer to 1 (larger)
	// CONCEPT: the function should change ratio from ~0 starting from the second item
	// up to 1 for the item in the middle of the rage
	const auto rhf =
		//! \brief Highest weight ratio
		//!
		//! \param i Id  - index of the link E [1, lsnum / 2)
		//! \param lsnum Id  - the total number of links
		//! \return LinkWeight  - weight ratio E (0, 1]
		//// Note: if rlsmin > linksNum / 2 then it is fine to have larger weights,
		//// anyway the remaining links can be skipped if any, the reduction will be
		//// determined by the back side
		//[](Id i, Id lsnum) noexcept -> LinkWeight { return min<LinkWeight>(sqrtf(i) / log2f(lsnum), 1); };
		reduction != Reduction::SEVERE
		//? [](Id i, Id lsnum, Id rlsmin) noexcept -> LinkWeight { return min<LinkWeight>(2 * LinkWeight(i) / (lsnum - 2), 1); }
		? [](Id i, Id lsnum, Id rlsmin) noexcept -> LinkWeight {
			return 2 * LinkWeight(i) / (lsnum - 2);  // Note: we don't care about the values for i >= lsnum / 2
		}
		: [](Id i, Id lsnum, Id rlsmin) noexcept -> LinkWeight {
			return min<LinkWeight>(i / (rlsmin + 2 * sqrtf(lsnum - rlsmin)), 1);
		};
	const Id sid = node.id;  // Source node id
	auto ih = --links.end();  // Index of the heavy items (from the end)
	// Ratio of the heavy links weight drop E (0, 1], typically >= 0.5
	const LinkWeight  rwh = reduction != Reduction::SEVERE ? 0.5f : 0.85f;
	LinkWeight  wh0d = 0;  // Decreased weight of the head link
	LinkWeight  wcur = 0;  // Weight of the current link
	AccWeight  wh = 0;  // Accumulated weight of the retaining heavy links
	Id  hnum = 0;  // Index of the last processed link from the end
	Id  skips = 0;  // The number of occurred self-links

	// Take heavy (last) links with j=2 distinct weights skipping self-links
	for(Id i = 0, j = 0; (i < rlsmin + skips || j < 2) && i < lsnum; ++i)
		if((--ih)->id != sid) {
			// For the accurate case use only the highest weight as a margin
			// instead of the sliding window over the largest weights
			if(!accurate && wh)
				wh += ih->weight * rhf(++hnum, lsnum, rlsmin);
			else if(!wh)
				wh0d = (wcur = wh = ih->weight) * rwh;  // Note: weight of the last link is always taken as it is
			if(less(ih->weight, wcur)) {
				wcur = ih->weight;
				++j;
			}
		} else ++skips;
	// Remove only more lightweight links than retained
	// (otherwise results might be input order dependent, not deterministic)
	// Check for the early interruption by the permanent weight
	const auto&  ilb = links.begin();  // Begin of links
	if(!less<LinkWeight>(ilb->weight, min(wh0d, ih->weight)))
		return;
#if VALIDATE >= 2
	assert(distance(ih, links.end()) >= rlsmin + skips && "acsReduceLinks(), ih verification failed");
#endif // VALIDATE
	auto  il = ilb;  // Index of the lightweight elements (from the begin)
	AccWeight  wl = 0;  // Accumulated weight of the reducing lightweight links
	// Accumulate weight in to have lightweight weights sum a bit lower than heavy weights sum
	// traversing over all items
	if(accurate)
		// Note: don't take tail links larger than half of the head link
		for(; less<LinkWeight>(wl, wh * rwh) && wl < wh && il != ih; ++il)
			wl += il->weight;
	else while(il != ih) {
		// Note: don't take tail links larger than half of the head link
		wcur = min(wh0d, ih->weight);
		for(; less<LinkWeight>(il->weight, wcur) && wl < wh && il != ih; ++il)
			wl += il->weight;
		if(!less<LinkWeight>(wl, wh))
			break;
		if(il != ih && (--ih)->id != sid)  // Skip self link
			wh += ih->weight * rhf(++hnum, lsnum, rlsmin);
	}
	// Consider deterministic reduction independent on the order of the processed items
	wcur = il->weight;  // Border weight of the lightweight weights
	while(il != ilb && !less<LinkWeight>((--il)->weight, wcur));  // Note: the body is intentionally empty
	if(il != ilb)
		++il;  // Increment iterator to point to the first whc, i.e. past the last reducing link
	else return;
	// Update heavy weights border
	ih = il;
	// Reduce links [ilb, ih) transforming to the node weights, check for the self-link is not required
	// NOTE: should be stored in the TLS in case of the parallel implementation
	static unordered_set<Id, SolidHash<Id>>  ids;  // Destination ids to control duplicates
#if VALIDATE >= 2
	assert(ids.empty() && "acsReduceLinks(), an empty container is expected");
#endif // VALIDATE
	if(!sumdups)
		ids.reserve(distance(ilb, ih));
	Id  did = ID_NONE;  // Required for the exception description
	try {
		for(il = ilb; il != ih; ++il) {
			// Check for the duplicates
			did = il->id;
			if(!sumdups && !ids.insert(did).second) {
				// Note: sumdups is applicable only for the weighted links
				if(errs)
					errs->add(LinkSrcDstId(sid, did));  // Duplicated link
				continue;
			}
			// ATTENTION: interpretation of the directed links should be synced with acsAddNodeLink()
			// Note: self-weight can be specified via arc and edges in a unified way,
			// edges does not double it
			AccWeight weight = il->weight;
			if(did != sid) {
				// Replace link with self-weights
				auto dest = idNodes.at(did);
				if(DIRECTED)
					weight /= 2;
				node.addWeight(weight);
				dest->addWeight(weight);
			} else {
				// Note: node weight exists even for the unweighted links,
				// *2 because self-weight is doubled
				node.addWeight(weight * 2);
			}
		}
		ids.clear();
		// Remove the links converted to self-weights
#if TRACE >= 2
		fprintf(ftrace, "acsReduceLinks(), #%u reduced %lu / %lu links\n"
			, sid, distance(ilb, ih), links.size());
#endif // TRACE
		links.erase(ilb, ih);
	} catch(out_of_range& err) {
		ids.clear();
		throw out_of_range(string("acsReduceLinks(), the link with non-existent node is used: #")
			.append(std::to_string(did)).append("\n") += err.what());
	}
}

//! \brief Add node links from the user input
//!
//! \tparam DIRECTED  - whether links are directed
//!
//! \param idNodes const IdNodesT&  - external id / internal nodes mapping
//! \param src Id  - external node id, source of the links
//! \param links InpLinksT&&  - external node links to be added
//! \param sumdups bool  - accumulate weight of duplicated links or just skip them
//! \param reduction Reduction  - core node links reduction policy
//! \param rlsmin Id  - minimal number of links after the reduction, 0 means omit the reduction
//! \param errs StructLinkErrors*  - occurred accumulated link errors to be reported by the caller
//! \return bool  - directed links added among others
template <bool DIRECTED, typename IdNodesT, typename InpLinksT>
bool acsAddNodeLinks(const IdNodesT& idNodes, Id src, InpLinksT&& links
	, bool sumdups, Reduction reduction, Id rlsmin, StructLinkErrors* errs)
{
#if VALIDATE >= 2
	// Note: initializer links doesn't empty
	assert(links.size() != 0 && "acsAddNodeLinks(), adding links should not be empty");
#endif // VALIDATE
	// Append node links
	bool directed = false;
	Id  dstId = ID_NONE;  // Required for the exception description
	try {
		auto nd = idNodes.at(src);
		// Add links using different input containers
		auto addlinks = [&](const auto& links)
		{
			for(auto& ln: links)
				directed = acsAddNodeLink<DIRECTED>(nd, idNodes.at(dstId = ln.id)
					, ln.weight, sumdups, errs) || directed;
		};
		// Reduce the input links if required
		if(DIRECTED && InpLinksT::value_type::IS_WEIGHTED && rlsmin && links.size() > rlsmin)
			acsReduceLinks<DIRECTED>(*nd, links, idNodes, reduction, rlsmin, sumdups, errs);
		addlinks(links);
	} catch(out_of_range& err) {
		throw out_of_range(string("acsAddNodeLinks(), the link with non-existent node is used: #")
			.append(std::to_string(dstId != ID_NONE ? dstId : src)).append("\n") += err.what());
	}
	return directed;
}

//! \brief Add node links adding with the nodes (if did not exist)
//!
//! \param nodes NodesT&  - internal nodes that can be extended
//! \param idNodes IdNodesT&  - external id / internal nodes mapping
//! \param src Id  - external node id, source of the links
//! \param links InpLinksT&&  - external node links to be added
//! \param shuffle bool  - shuffle (rand reorder) nodes and links
//! \param sumdups bool  - accumulate weight of duplicated links or just skip them
//! \param reduction Reduction  - core node links reduction policy
//! \param rlsmin Id  - minimal number of links after the reduction, 0 means omit the reduction
//! \param errs StructLinkErrors*  - occurred accumulated link errors to be reported by the caller
//! \return bool  - directed links added among others
template <bool DIRECTED, typename NodesT, typename IdNodesT, typename InpLinksT>
bool acsAddNodeAndLinks(NodesT& nodes, IdNodesT& idNodes, Id src, InpLinksT&& links
	, bool shuffle, bool sumdups, Reduction reduction, Id rlsmin, StructLinkErrors* errs)  // , StructNodeErrors* nderrs
{
	// Construct & fill nodeIds to be [randomly] instantiated
	Items<Id>  nodeIds;
	nodeIds.reserve(1 + links.size());
	nodeIds.push_back(src);
	for(const auto& ln: links)
		nodeIds.push_back(ln.id);

	// Randomize and instantiate the nodes
	// Note: nderrs are not supplied, because it is expected that most of the nodes
	// already present and we do not need the corresponding warnings
	acsAddNodes(nodes, idNodes, nodeIds, shuffle);
	// Fill the links validating existence of nodes if required
#if VALIDATE >= 3
	assert(idNodes.find(src) != idNodes.end()
		&& "acsAddNodeAndLinks(), the src node must be already constructed");
#endif
	auto snode = idNodes[src];
	// Reduce the input links if required
	InpLinksT  rdls;  // Container for the reduced links
	if(DIRECTED && InpLinksT::value_type::IS_WEIGHTED && rlsmin && links.size() > rlsmin)
		acsReduceLinks<DIRECTED>(*snode, links, idNodes, reduction, rlsmin, sumdups, errs);
	bool directed = false;
	for(auto& ln: links) {
#if VALIDATE >= 3
		assert(idNodes.find(ln.id) != idNodes.end()
			&& "acsAddNodeAndLinks(), the dest node must be already constructed");
#endif
		directed = acsAddNodeLink<DIRECTED>(snode, idNodes[ln.id], ln.weight
			, sumdups, errs) || directed;
	}
	return directed;
}

// External Input interfaces implementation -----------------------------------
template <bool LINKS_WEIGHTED>
Graph<LINKS_WEIGHTED>::Graph(Id nodesNum, bool shuffle, bool sumdups, Reduction reduction)
: m_nodes(), m_dclnds(nodesNum), m_directed(false), m_reduction(reduction & *Reduction::SKIP_NODES
	? Reduction::NONE : reduction & Reduction::CORE_MASK)
, m_rlsmin(0), m_idNodes(), m_shuffle(shuffle), m_sumdups(sumdups), m_hier()
{
	if(*m_reduction && !(LINKS_WEIGHTED && nodesNum))
		throw invalid_argument("Graph(), the reduction can be performed only for"
			" the weighed directed links and requires predefined nodesNum");
	// Note: links reduction in the input graph is more severe than on the following iterations
	// SEVERE links reduction policy on clustering
	m_rlsmin = m_reduction != Reduction::NONE ? reducedLinksMarg(nodesNum, m_reduction) : 0;
	if(nodesNum)  // Note: hash map is rehashed on reserve()
		m_idNodes.reserve(nodesNum);
}

template <bool LINKS_WEIGHTED>
void Graph<LINKS_WEIGHTED>::reset(Id nodesNum, bool shuffle, bool sumdups, Reduction reduction)
{
	if(reduction & *Reduction::SKIP_NODES)
		m_reduction = Reduction::NONE;
	else m_reduction = reduction & Reduction::CORE_MASK;
	if(*m_reduction && !(LINKS_WEIGHTED && nodesNum))
		throw invalid_argument("reset(), the reduction can be performed only for"
			" the weighed directed links and requires predefined nodesNum");
	m_nodes.clear();
	m_dclnds = nodesNum;
	m_directed = false;
	// Note: links reduction in the input graph is more severe than the
	// SEVERE links reduction policy on clustering
	m_rlsmin = m_reduction != Reduction::NONE ? reducedLinksMarg(nodesNum, m_reduction) : 0;
	m_idNodes.clear();
	const size_t  idnsCapacity = m_idNodes.bucket_count() * m_idNodes.max_load_factor();
	if(nodesNum && (idnsCapacity < nodesNum || idnsCapacity > nodesNum * rhszmul))
		m_idNodes.reserve(nodesNum);  // Note: hash map is rehashed on reserve()
	m_shuffle = shuffle;
	m_sumdups = sumdups;
}

template <bool LINKS_WEIGHTED>
auto Graph<LINKS_WEIGHTED>::release(IdItems<NodeT>* idnodes, bool* directed)
//-> unique_ptr<NodesT>
-> shared_ptr<NodesT>
{
	//NodesT  nodes;  // Used for the RVO (return value optimization)
	m_dclnds = 0;
	if(directed)
		*directed = m_directed;
	m_directed = false;
	m_reduction = Reduction::NONE;
	m_rlsmin = 0;

	if(idnodes)
		*idnodes = move(m_idNodes);  // Note: m_idNodes might not be cleared after the move() according to the standard
	m_idNodes.clear();  // Note: m_idNodes should ALWAYS become empty in the end

	//swap(nodes, m_nodes);  // Now m_nodes are empty
	return make_shared<NodesT>(move(m_nodes));
}

template <bool LINKS_WEIGHTED>
void Graph<LINKS_WEIGHTED>::addNodes(Id number, Id id0, StructNodeErrors* errs)
{
	if(!number) {
		assert(0 && "addNodes(), the number of nodes to instantiate should be positive");
		return;
	}
	// Fill nodes and mapping id -> nodePtr
	// Note: reserve ALWAYS rehashes the unordered map
	const size_t  idnsCapacity = m_idNodes.bucket_count() * m_idNodes.max_load_factor();
	if(idnsCapacity < m_idNodes.size() + number)
		m_idNodes.reserve(m_idNodes.size() * rhszmul + number);  // Note: + and * used to reduce the number of rehashes on subsequent calls

	Id  ide = id0 + number;  // Id after the last instantiating
	if(m_shuffle) {
		// Create nodes not subsequently in time to have different orders of
		// addresses in memory and ids
#if TRACE >= 3
		fputs("Randomized node ids: ", ftrace);
#endif // TRACE
		while(id0 != ide) {
			auto nid = rdev() % 2 ? id0++ : --ide;
#if TRACE >= 3
			fprintf(ftrace, " %u", nid);
#endif // TRACE
			acsAddNode(m_nodes, m_idNodes, nid, errs);
		}
#if TRACE >= 3
		fputc('\n', ftrace);
#endif // TRACE
	} else for(Id nid = id0; nid < ide; ++nid)
		acsAddNode(m_nodes, m_idNodes, nid, errs);
}

template <bool LINKS_WEIGHTED>
void Graph<LINKS_WEIGHTED>::addNodes(const Ids& nodesIds, StructNodeErrors* errs)
{
	acsAddNodes(m_nodes, m_idNodes, nodesIds, m_shuffle, errs);
}

template <bool LINKS_WEIGHTED>
void Graph<LINKS_WEIGHTED>::addNodes(const initializer_list<Id>& nodesIds, StructNodeErrors* errs)
{
	acsAddNodes(m_nodes, m_idNodes, nodesIds, m_shuffle, errs);
}

template <bool LINKS_WEIGHTED>
template <bool DIRECTED>
void Graph<LINKS_WEIGHTED>::addNodeLinks(Id node, InpLinksT&& links
	, StructLinkErrors* lnerrs)
{
	m_directed = acsAddNodeLinks<DIRECTED>(m_idNodes, node, forward<InpLinksT>(links)
		, m_sumdups, m_reduction, m_rlsmin, lnerrs) || m_directed;
}

template <bool LINKS_WEIGHTED>
template <bool DIRECTED>
void Graph<LINKS_WEIGHTED>::addNodeLinks(Id node, const initializer_list<InpLinkT>& links
	, StructLinkErrors* lnerrs)
{
	m_directed = acsAddNodeLinks<DIRECTED>(m_idNodes, node, Links<InpLink<LINKS_WEIGHTED>>(links)
		, m_sumdups, m_reduction, m_rlsmin, lnerrs) || m_directed;
}

template <bool LINKS_WEIGHTED>
template <bool DIRECTED>
void Graph<LINKS_WEIGHTED>::addNodeAndLinks(Id node, InpLinksT&& links
	, StructLinkErrors* lnerrs)
{
	m_directed = acsAddNodeAndLinks<DIRECTED>(m_nodes, m_idNodes, node, forward<InpLinksT>(links)
		, m_shuffle, m_sumdups, m_reduction, m_rlsmin, lnerrs) || m_directed;
}

template <bool LINKS_WEIGHTED>
template <bool DIRECTED>
void Graph<LINKS_WEIGHTED>::addLink(Id snode, Id dnode, LinkWeight weight, StructLinkErrors* lnerrs)
{
	try {
		auto snd = m_idNodes.at(snode);
		if(m_rlsmin && !snd->links.empty())
			throw logic_error("addLink(), links can be added only ones for each node on graph reduction\n");
		m_directed = acsAddNodeLink<DIRECTED>(snd, m_idNodes.at(dnode)
			, weight, m_sumdups, lnerrs) || m_directed;
	} catch(out_of_range& err) {
#if TRACE >= 3
		fprintf(ftrace, "addLink(), adding link #%u to #%u\n", snode, dnode);
#endif // TRACE
		throw out_of_range(string("addLink(), the link with non-existent node is used: #")
			.append(std::to_string(snode)).append(".").append(std::to_string(dnode))
			.append("\n") += err.what());
	}
}

template <bool LINKS_WEIGHTED>
auto Graph<LINKS_WEIGHTED>::buildHierarchy(const ClusterOptions& opts) -> Hierarchy<LinksT>&
{
	m_hier = cluster(m_nodes, !m_directed, opts);
	return hierarchy();
}

template <bool LINKS_WEIGHTED>
auto Graph<LINKS_WEIGHTED>::hierarchy() -> Hierarchy<LinksT>&
{
	if(!m_hier)
		throw runtime_error("The hierarchy has not been constructed\n");
	return *m_hier;
}
#endif // GRAPH_HPP
