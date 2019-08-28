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
//! \date 2014-09-07

#ifndef GRAPH_H
#define GRAPH_H

#include <initializer_list>

#include "types.h"  // LinkWeight, IdItems (nodes mapping)


namespace daoc {

using std::initializer_list;


// External Interface for the Data Input ---------------------------------------
#ifdef SWIG
// Hide some accessory methods required only internally for the SWIG compilation
%ignore InpLink::InpLink();
#endif // SWIG

//! \brief External Input Link
//! External Weighted Input Link specialization
//!
//! \tparam WEIGHTED bool  - whether the link is weighted or not
template <bool WEIGHTED=true>
struct InpLink {
	using Weight = LinkWeight;  //!< \copydoc LinkWeight

	//! \copydoc WEIGHTED
	constexpr static bool  IS_WEIGHTED = WEIGHTED;

	Id  id;  //!< Dest node id
	Weight  weight;  //!< Link weight

    //! \brief Weighted InpLink constructor
    //!
    //! \param lid Id  - dest node id
    //! \param lweight=WEIGHT Weight  - link weight
	InpLink(Id lid, Weight lweight=SimpleLink<Weight>::weight)
	: id(lid), weight(lweight)  {}

#ifdef DAOC_SWIGPROC
	//! Default constructor, required by SWIG
	InpLink(): InpLink(-1) {}  // Note: set distinguishable id that is unlikely used
#endif // SWIG
};

//! External Unweighted (Simple) Input Link specialization
template <>
struct InpLink<false> {
	using Weight = LinkWeight;  //!< \copydoc LinkWeight

	//! Link is unweighted
	constexpr static bool  IS_WEIGHTED = false;
	//! \copydoc SimpleLink<Weight>::weight
	constexpr static Weight  weight = SimpleLink<Weight>::weight;

	Id  id;  //!< Dest node id

    //! \brief Unweighted InpLink constructor
    //!
    //! \param lid Id  - dest node id
	InpLink(Id lid): id(lid)  {}

#ifdef DAOC_SWIGPROC
	//! Default constructor, required by SWIG
	// Required by SWIG for the returning by value and usage in the STL containers
	InpLink(): InpLink(-1) {}  // Note: set distinguishable id that is unlikely used
#endif // DAOC_SWIGPROC
};

//! \brief Nodes Graph to couple nodes externally
//! \note Back links must always exist even with zero weight
//!
//! \tparam LINKS_WEIGHTED bool  - whether the links are weighted or not
template <bool LINKS_WEIGHTED=true>
class Graph {  // : public AbstractObject
// TODO: Implement for Bipartie graphs
//	// All this is Description or Extension that can be also moved to the hierarchy
//	struct Component {  // ~ concepts
//		Id  id;
//		float  weight;  // 0..1
//	};
//	list<Component>  Components;
//	unordered_map<Component*, string, SolidHash<Component*>>  ComponentTitles;
//	unordered_map<NodeT*, vector_ordered<Component*>, SolidHash<NodeT*>> NodeComponents;
//	unordered_map<NodeT*, string, SolidHash<NodeT*>>  NodeTitles;
public:
#ifdef DAOC_SWIGPROC
	// ATTENTION: link error occurs for Java in case of the:
	constexpr static bool  IS_WEIGHTED = LINKS_WEIGHTED;
	//constexpr static bool weighted()  { return LINKS_WEIGHTED; }
#endif // DAOC_SWIGPROC

	// Note: LinkT::IS_WEIGHTED should be used to check whether the graph links are weighted
    //! \copydoc Link<LinkWeight, LINKS_WEIGHTED>
    //! \note Use only Unsigned links
	using LinkT = Link<LinkWeight, LINKS_WEIGHTED>;
	using LinksT = Links<LinkT>;  //!< \copydoc Links<LinkT>
	using NodeT = Node<LinksT>;  //!< \copydoc Node<LinksT>
	using NodesT = Nodes<LinksT>;  //!< \copydoc Nodes<LinksT>

	using Ids = Items<Id>;  //!< \copydoc Items<Id>
	using InpLinkT = InpLink<LINKS_WEIGHTED>;  //!< \copydoc InpLink<LINKS_WEIGHTED>
	using InpLinksT = Links<InpLinkT>;  //!< \copydoc Links<InpLinkT>
	//using ClustersT = Clusters<LinksT>;  //!< \copydoc Clusters<LinksT>

    //! \brief Graph constructor
    //!
    //! \param nodesNum=0 Id  - estimated number of nodes, 0 - unknown estimation
    //! \param shuffle=false bool  - shuffle (rand reorder) nodes and links on creation
    //! 	NOTE: Nodes in the graph have always ordered links even when they are shuffled,
    //! 	manually edited (extended) links also should be ordered.
    //! \param sumdups=false bool  - accumulate weight of duplicated links or just skip them
    //! \param reduction=Reduction::NONE Reduction  - input graph reduction policy
    //! 	NOTE: Node weights, determinism and input order independence
    //! 	of the clustering are retained, accuracy is almost unaffected.
    //! 	ATTENTION:
    //! 	- The nodes must be preallocated and all links of each node must be
    //! 	specified at once, i.e. some graph extension functions become unavailable.
    //! 	- Applicable only for the WEIGHTED DIRECTED links.
    ////! \param reduce=false bool  - reduce the input graph by the non significant links.
    ////! 	- Can be used with the SEVERE | SKIP_NODE reduction policy of the clustering,
    ////! 	been faster and consuming less memory than just SEVERE, but dropping the accuracy more.
// Note: isdir should not be used as constructor template parameter or constructor parameter
// to allow passing of the Graph objects of the input file parsers
//    //! \param isdir=false bool  - the graph is directed (arcs exist)
	Graph(Id nodesNum=0, bool shuffle=false, bool sumdups=false //, bool reduce=false
		, Reduction reduction=Reduction::NONE);

	// ATTENTION: Incremental processing requites in-place modification of the
	// hierarchy, because the clusters are based on nodes and nodes updates
	// triggers updates of the corresponding clusters!
	// The same time there should be a guarantee to not modify the hierarchy
	// (including nodes) while clustering is performed => the hierarchy should
	// have methods of async lazy nodes modification.
	// This also implies that significant part of the client functionality
	// can be moved to the lib.
//    //! \brief Move constructor to create the graph from the existing nodes.
//    //! \post Supplied nodes will have unspecified state. Finalization state is
//    //! 	reseted.
//    //! \note Can be used for the incremental graph extension - processing workflow
//    //!
//    //! \param nodes NodesT&&  - nodes to be moved to the graph
//    //! \param shuffle=false bool  - shuffle (rand reorder) nodes and links
//	Graph(NodesT&& nodes, diff bool shuffle=false) noexcept;

	Graph(const Graph&)=delete;
	Graph(Graph&&)=default;

	Graph& operator=(const Graph&)=delete;
	Graph& operator=(Graph&&)=default;

    //! \brief Convert self to the shared_ptr
    //! \attention The former object becomes uninitialized (empty)
    //! \note Can be useful as an external interface for the languages lacking pointers
    //!
    //! \return shared_ptr<Hierarchy>  - shared_ptr wrapper of the hierarchy
	shared_ptr<Graph> toSharedPtr()  { return make_shared<Graph>(move(*this)); }

	//! \copydoc m_nodes
	const
#ifndef SWIG
	NodesT
#else
	Nodes(Links<Link<LinkWeight, LINKS_WEIGHTED>>)
#endif // SWIG
	& nodes() const  { return m_nodes; }

	//! \copydoc m_directed
	bool directed() const noexcept  { return m_directed; }

	//! The graph loading performed with reduction of the insignificant links
	// Note: directed can be set only on graph filling. Only directed weighted links
	// can be reduced on input
	bool reduced() const noexcept  { return m_rlsmin && m_directed; }

	//! \copydoc m_dclnds
	Id dclnds() const noexcept  { return m_dclnds; }

    //! \brief Reset the Graph
    //! \post Existing nodes and internal state are reseted
    //!
    //! \param nodesNum  - estimated (declared) number of nodes
    //! \param shuffle=false bool  - shuffle (rand reorder) nodes and links
    //! \param sumdups=false bool  - accumulate weight of duplicated links or just skip them
    //! \param reduction=Reduction::NONE Reduction  - input graph reduction policy
    //! 	NOTE: Node weights, determinism and input order independence
    //! 	of the clustering are retained, accuracy is almost unaffected.
    //! 	ATTENTION:
    //! 	- The nodes must be preallocated and all links of each node must be
    //! 	specified at once, i.e. some graph extension functions become unavailable.
    //! 	- Applicable only for the WEIGHTED DIRECTED links.
    ////! \param reduce=false bool  - reduce the input graph by the non significant links.
    ////! 	- Can be used with the SEVERE | SKIP_NODE reduction policy of the clustering,
    ////! 	been faster and consuming less memory than just SEVERE, but dropping the accuracy more.
	inline void reset(Id nodesNum=0, bool shuffle=false, bool sumdups=false //, bool reduce=false
		, Reduction reduction=Reduction::NONE);

    //! \brief Release nodes from the graph leaving it empty
    //! \post The graph becomes reseted to the state before the nodes filling
    //! \attention directed flag is reseted
    //!
    //! \param[out] idnodes=nullptr IdItems<NodeT>*  - mapping of the node ids to the nodes
    //! \param[out] directed=nullptr bool*  - whether the released nodes have directed links
    //! 	(might have distinct inbound and outbound weight, asymmetric)
    //! \return unique_ptr<NodesT>  - graph nodes (returned using RVO, i.e. resetting the graph)
    // Note: return by smart pointer is required for the efficient export API,
    // unique_ptr is not supported by SWIG because of the copy constructor absence
	//unique_ptr<
	shared_ptr<
#ifndef SWIG
		NodesT
#else
		Nodes(Links<Link<LinkWeight, LINKS_WEIGHTED>>)
#endif // SWIG
	> release(
#ifndef SWIG
		IdItems<Node<LinksT>>
#else
		IdItems(Node<Links<Link<LinkWeight, LINKS_WEIGHTED>>>)
#endif // SWIG
		* idnodes=nullptr, bool* directed=nullptr);

    //! \brief Node by id
    //!
    //! \param id  - node id
    //! \return NodeT*  - resulting node with specified id. Exception is thrown
    //! 	if the required node id does not exist
#ifndef SWIG
	NodeT
#else
	Node<Links<Link<LinkWeight, LINKS_WEIGHTED>>>
#endif // SWIG
	* node(Id id) const  { return m_idNodes.at(id); }

// Note: this is easy to track for the extending graph, but is too resource-consuming
// when the links / nodes can be deleted
//    //! \brief The graph has weighted nodes
//    //! \attention The graph might still have unweighted links
//    //!
//    //! \return constexpr bool  - the nodes are weighted (at least one node has non-zero weight)
//	bool nodesWeighted() const  { return m_nodeWeight; }

    //! \brief Add new nodes to the graph
    //! Required only to preallocate nodes and decrease number of reallocations
    //!
    //! \param number Id  - number of nodes to be constructed with sequential ids
    //! \param id0=0 Id - starting id
	//! \param errs=nullptr StructLinkErrors*  - occurred accumulated node errors
	//! 	to be reported by the caller (duplicated discarded nodes)
    //! \return void
	inline void addNodes(Id number, Id id0=0, StructNodeErrors* errs=nullptr);

    //! \brief Add new nodes to the graph
    //! Required only to preallocate nodes and decrease number of reallocations
    //!
    //! \param nodesIds const Ids&  - node ids to be added
	//! \param errs=nullptr StructLinkErrors*  - occurred accumulated node errors
	//! 	to be reported by the caller (duplicated discarded nodes)
    //! \return void
	inline void addNodes(const
#ifndef SWIG
		Ids
#else
		Items<Id>
#endif // SWIG
	& nodesIds, StructNodeErrors* errs=nullptr);

#ifndef SWIG
	// Note: SWIG does not fully support initializer_list
    //! \brief Add new nodes to the graph
    //! Required only to preallocate nodes and decrease number of reallocations
    //!
    //! \param nodesIds const initializer_list<Id>&  - list of node ids to be added
	//! \param errs=nullptr StructLinkErrors*  - occurred accumulated node errors
	//! 	to be reported by the caller (duplicated discarded nodes)
    //! \return void
	inline void addNodes(const initializer_list<Id>& nodesIds, StructNodeErrors* errs=nullptr);
#endif // SWIG

    //! \brief Add node links to the Graph
    //! \pre The node and links must refer to already existing nodes
    //! \note According to the specified option duplicated links either summed up,
    //! 	or yield warnings. Duplicated unweighted non-selflinks can't be summed up
    //! 	and always yield warnings.
    //! \attention Can be called only once per a node in the reduced graph.
    //! 	Addition of undirected non-selflink internally represented as
    //! 	two directed links having
    //! 	a) half of the original weigh for the weighted links, or
    //! 	b) default weight for the unweighted links
    //! 		=> Graph weight is doubled (including node weights via self-links)
    //!
    //! \tparam DIRECTED bool  - whether links are directed
    //! \param node Id  - source node id
    //! \param links InpLinksT&&  - node links
	//! \param lnerrs=nullptr StructLinkErrors*  - occurred accumulated link errors
	//! 	to be reported by the caller (duplicated discarded links)
    //! \return void
	template <bool DIRECTED>
	inline void addNodeLinks(Id node, Links<InpLink<LINKS_WEIGHTED>>&& links
		, StructLinkErrors* lnerrs=nullptr);

#ifndef SWIG
	// Note: SWIG does not fully support initializer_list
    //! \brief Add node links to the Graph
    //! \pre The node and links must refer to already existing nodes
    //! \note According to the specified option duplicated links either summed up,
    //! 	or yield warnings. Duplicated unweighted non-selflinks can't be summed up
    //! 	and always yield warnings.
    //! \attention Can be called only once per a node in the reduced graph.
    //! 	Addition of undirected non-selflink internally represented as
    //! 	a) half of the original weigh for the weighted links, or
    //! 	b) default weight for the unweighted links
    //! 		=> Graph weight is doubled (including node weights via self-links)
    //!
    //! \tparam DIRECTED bool  - whether links are directed
    //! \param node Id  - source node id
    //! \param links const initializer_list<InpLinkT>&  - node links
	//! \param lnerrs=nullptr StructLinkErrors*  - occurred accumulated link errors
	//! 	to be reported by the caller (duplicated discarded links)
    //! \return void
	template <bool DIRECTED>
	inline void addNodeLinks(Id node, const initializer_list<InpLinkT>& links
		, StructLinkErrors* lnerrs=nullptr);
#endif // SWIG

    //! \brief Add node links to the Graph
    //! \post Referred nodes are added if not existed
    //! \note According to the specified option duplicated links either summed up,
    //! 	or yield warnings. Duplicated unweighted non-selflinks can't be summed up
    //! 	and always yield warnings.
    //! \attention Can be called only once per a node in the reduced graph.
    //! 	Addition of undirected non-selflink internally represented as
    //! 	a) half of the original weigh for the weighted links, or
    //! 	b) default weight for the unweighted links
    //! 		=> Graph weight is doubled (including node weights via self-links)
    //!
    //! \tparam DIRECTED bool  - whether links are directed
    //! \param node Id  - source node id
    //! \param links InpLinksT&&  - node links
	//! \param lnerrs=nullptr StructLinkErrors*  - occurred accumulated link errors
	//! 	to be reported by the caller (duplicated discarded links)
    //! \return void
	template <bool DIRECTED>
	inline void addNodeAndLinks(Id node, Links<InpLink<LINKS_WEIGHTED>>&& links
		, StructLinkErrors* lnerrs=nullptr);

    //! \brief Add node link to the Graph
    //! \pre The node ids must refer to already existing nodes
    //! \note Can't be used on the graph reduction to add more than only link for the source node.
    //! 	Much less efficient than batch links extension via addNode[And]Links().
	//! \attention All self links (edges and arcs) are always treated as edges (doubled) to be
	//! 	to be consistent with the subsequent (self) weight aggregation on clusters formation.
    //!
    //! \tparam DIRECTED bool  - whether links are directed
    //! \param snode Id  - source node id
    //! \param dnode Id  - destination node id
    //! \param weight=SimpleLink<Weight>::weight LinkWeight  - link weight,
    //! 	ignored for unweighted links
	//! \param lnerrs=nullptr StructLinkErrors*  - occurred accumulated link errors
	//! 	to be reported by the caller (duplicated discarded links)
    //! \return void
	template <bool DIRECTED>
	inline void addLink(Id snode, Id dnode, LinkWeight weight=SimpleLink<LinkWeight>::weight
		, StructLinkErrors* lnerrs=nullptr);

    //! \brief Cluster the graph producing hierarchy of clusters
    //! \post Nodes are moved to the hierarchy (their addresses are remained) and
    //! 	become empty in the graph
    //! \attention Nodes are moved to the hierarchy being built
    //!
    //! \return Hierarchy<LinksT>& buildHierarchy(const ClusterOptions&
	Hierarchy<
#ifndef SWIG
		LinksT
#else
		Links<Link<LinkWeight, LINKS_WEIGHTED>>
#endif // SWIG
	>& buildHierarchy(const ClusterOptions& opts=ClusterOptions());

    //! \brief Stored hierarchy of clusters
    //!
    //! \return Hierarchy<LinksT>&
	Hierarchy<
#ifndef SWIG
		LinksT
#else
		Links<Link<LinkWeight, LINKS_WEIGHTED>>
#endif // SWIG
	>& hierarchy();
private:
    //! Graph nodes with links formed from input data to be clustered
    //! \post Node links are ORDERED by bsObjsDest() even when shuffled
    //! \attention Node links must be ordered by bsObjsDest() even if manually
    //! 	edited
	NodesT  m_nodes;
	Id  m_dclnds;  //!< Declared number of nodes (typically fetched from the file header)

	//! Whether nodes links are directed (arcs exist)
	//! Note: links weight processing on clustering can be simplified when the links
	//! have symmetric weights (always for the undirected graphs)
	//! \note This flag is set automatically on the links extension (addition)
	bool  m_directed;

	Reduction  m_reduction;  //!< Core reduction policy for the nodes

	//! \brief Minimal number of links after the reduction
	//! Evaluated according to the reduction policy, 0 means omit the reduction
    //! \attention The nodes must be preallocated and all links of each node
    //! must be specified at once, i.e. some graph extension functions become unavailable.
    //! Can be used only for the WEIGHTED DIRECTED links, otherwise links reduction
    //! policies on clustering can be considered.
    Id  m_rlsmin;

    IdItems<NodeT>  m_idNodes;  //!< Id/node mapping
	bool  m_shuffle;  //!< Shuffle nodes and links on creation
	bool  m_sumdups;  //!< Accumulate weight of duplicated links or just skip them
	shared_ptr<Hierarchy<LinksT>>  m_hier;
};

}  // daoc

#endif // GRAPH_H
