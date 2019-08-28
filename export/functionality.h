//! \brief Exporting functionality.
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! >	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2016-09-29

#ifndef FUNCTIONALITY_H
#define FUNCTIONALITY_H

#include "types.h"  // Clusters, ...


namespace daoc {

// Clustering interface -------------------------------------------------------
//! \brief Perform clustering and build the hierarchy
//! \pre Node links are ordered by bsDest() and UNIQUE
//! \post Nodes are moved to the hierarchy
//!
//! \tparam LinksT  - type of items links
//!
//! \param nodes Nodes<LinksT>&  - nodes with ORDERED links to be clustered
//! \param edges bool  - whether links are edges with symmetric weights (the
//! 	network is undirected, use simplified calculations), or arcs with possibly
//! 	distinct in/out weights.
//! 	NOTE: Skipped (handled as edges, i.e. symmetric weights) for the unweighted links
//! \param opts=ClusterOptions() const ClusterOptions&  - clustering options
//! \return unique_ptr<Hierarchy<LinksT>>  - resulting hierarchy
// Note: unique_ptr<> in wrappers wrapped with equivalent of shared_ptr for the external langs;
// unique_ptr is not supported by SWIG because of the copy constructor absence
template <typename LinksT>
//unique_ptr<Hierarchy<LinksT>> cluster(
shared_ptr<Hierarchy<LinksT>> cluster(
#ifndef SWIG
		Nodes<LinksT>
#else
		Nodes(LinksT)
#endif // SWIG
	& nodes, bool edges, const ClusterOptions& opts=ClusterOptions());

template <typename LinksT>
//unique_ptr<Hierarchy<LinksT>> cluster(
shared_ptr<Hierarchy<LinksT>> cluster(
//	unique_ptr<
	shared_ptr<
#ifndef SWIG
		Nodes<LinksT>
#else
		Nodes(LinksT)
#endif // SWIG
	> nodes, bool edges, const ClusterOptions& opts=ClusterOptions())
{
	return cluster(*nodes, edges, opts);
}

// External interface implemented in processing.hpp ----------------------------
//! \brief Evaluate specified intrinsic clustering measures
//! \note Clusters might contain selflinks and undefined context
//!
//! \param ins Intrinsics&  - resulting intrinsic clustering measures
//! 	with the flags of what should be evaluated
//! \param cls Clusters<LinksT>&  - clusters to be evaluated
//! \param weight AccWeight  - total weight of the supplied clusters (both dir, i.e. doubled)
//! \param gamma=1 AccWeight - resolution parameter for the modularity evaluation, gamma >= 0
//! \return void
template <bool NONSYMMETRIC, typename LinksT>
void intrinsicMeasures(Intrinsics& ins,
#ifndef SWIG
	Clusters<LinksT>
#else
	Clusters(LinksT)
#endif // SWIG
	& cls
	, AccWeight weight, AccWeight gamma=1);

//! \brief Load and initialize clusters using the specified parser and graph nodes
//! \pre Node links are ordered by bsDest()
//! \post Created clusters contain self links and have undefined context, graph
//! 	nodes are reseted
//!
//! \tparam ParserT  - parser type, typically CnlParser
//! \tparam GraphT  - graph type
//!
//! \param[out] clusters Clusters<typename GraphT::LinksT>&  - loaded clusters
//! 	(with the corresponding graph nodes)
//! \param graph Graph<WEIGHTED>&  - input graph to define the nodes membership
//! 	in the clusters
//! \param filename const string&  - filename of the clustering to be loaded
//! \param validation=Validation::STANDARD Validation  - links consistency
//! 	validation policy for the input graph.
//! \return AccWeight  - weight of the loaded clusters (both dir, i.e. doubled)
template <typename ParserT, typename GraphT>
AccWeight loadClusters(
#ifndef SWIG
	Clusters<typename GraphT::LinksT>
#else
	Clusters(Links<Link<LinkWeight, GraphT::IS_WEIGHTED>>)
#endif // SWIG
	& clusters, GraphT& graph
	, const string& filename, Validation validation=Validation::STANDARD);

template <typename ParserT, typename GraphT>
AccWeight loadClusters(
#ifndef SWIG
	Clusters<typename GraphT::LinksT>
#else
	Clusters(Links<Link<LinkWeight, GraphT::IS_WEIGHTED>>)
#endif // SWIG
	& clusters, shared_ptr<GraphT> graph
	, const string& filename, Validation validation=Validation::STANDARD)
{
	return loadClusters<ParserT, GraphT>(clusters, *graph, filename, validation);
}

// Note: used to externally load clusters from the file
//! \brief Add link to the links
//! 	Accumulate weight for the existing link, otherwise insert the new link
//! \pre Links are ordered and unique.
//! 	[links.begin(), bln) should not contain dest.
//! \post Links are ordered and unique.
//!
//! \tparam LinksT  - cluster's links type
//!
//! \param links AccLinksT&  - links to be updated
//! \param bln AccLinksIT  - begin iterator of the updating links to reduce
//! 	the search space on subsequent inserts of the ordered items
//! \param dest ClusterT*  - link destination
//! \param weight AccWeight  - link weight (considering the sharing)
//! \return typename Links<AccLink<LinksT>>::iterator  - iterator of the inserted link
template <typename LinksT>
typename Links<AccLink<LinksT>>::iterator addLink(Links<AccLink<LinksT>>& links
	, typename Links<AccLink<LinksT>>::iterator bln, Cluster<LinksT>* dest, AccWeight weight);

//! \brief Half of the bidir links weight
//!
//! \param el const ItemT&  - item to be evaluated
//! \return AccWeight  - resulting weight (half of both directions)
template <bool NONSYMMETRIC, typename ItemT>
AccWeight linksWeight(const ItemT& el);

//! \brief Validate Node's links, show and FIX errors if exist
//! \pre If a node A has some link to the node B then node B must have
//! 	the link to the node A even if link's weight is 0.
//! 	Links should be ordered by bsDest().
//! \post Links are ordered and unique, weight might be updated.
//!
//! \note
//! - throws exception in case of inconsistent links
//! - zero link is not allowed as compliment for nonzero symmetric source link
//! - node's links should be preliminary sorted to be ready for the validation
//!
//! \tparam NONSYMMETRIC  - inbound/outbound weights of the links are not the same
//! \tparam NodesT  - nodes type
//!
//! \param nodes Nodes<LinksT>&  - network nodes to be validated
//! \param weight AccWeight&  - total bidirectional network weight to be updated
//! \param linksNum Size&  - nodes links number to be updated
//! \param severe bool  - severe validation, check ordering of the node links
//! \return void
template <bool NONSYMMETRIC, typename NodesT>
void validate(NodesT& nodes, AccWeight& weight, Size& linksNum, bool severe);

// TODO: Make C interface in parallel with C++:
// https://isocpp.org/wiki/faq/mixing-c-and-cpp
// http://www.oracle.com/technetwork/articles/servers-storage-dev/mixingcandcpluspluscode-305840.html
// or use SWIG for the cross-lang lib wrapping:
// https://zacg.github.io/blog/2013/06/06/calling-c-plus-plus-code-from-go-with-swig/
// http://swig.org/

}  // daoc

#endif // FUNCTIONALITY_H
