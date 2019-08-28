//! \brief The Dao of Clustering library interface - Robust & Fine-grained Deterministic Clustering for Large Networks
//! 	DAOC - Deterministic Aglomertive Overlapping Clustering
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! > 	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//! or commercial license (please contact the author for the details)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2017-07-26
//!
//! SWIG version for the build: 4.0.0 (ba45861)
// NOTE: the main issues were fixed by  e27a606 (17-09-30), so after the tuning this
// interface file should be applicable also for the SWIG 4 e27a606+ revisions.
// Required changes include explicit namespace specification for the SWIG directives
// including the instantiating templates.

%module daoc


%include <std_common.i>
%include <std_except.i>
%include <stdint.i>  // Required for Id, etc.
%include <std_shared_ptr.i>
//%include <std_unique_ptr.i>
%include <std_vector.i>
#if defined(SWIGJAVA) || defined(SWIGOCTAVE) || defined(SWIGPYTHON) ||  defined(SWIGRUBY)
%include <std_list.i>
#endif  // LANGS
#ifndef SWIGJAVA
// ATTENTION: SWIG Version 4.0.0, rev 897fe67 generates "File name too long"  error
// for the Java compilations with defined std_unordered_map
%include <std_unordered_map.i>
#endif  // SWIGJAVA
%include <std_string.i>  // Required for the output file specification
//%include <std_pair.i>  // pair() is not used explicitly in the code

// ATTENTION: %shared_ptr declarations should appear before the underlying type / template
// declaration (before the %include for the target type) and after the %include<std_shared_ptr.i>
// Note: Transparent shared_ptr made for the type that are not used without the shared_ptr

// ATTENTION: Transparent shared_ptr of the Graph causes suppression of the Graph definition as Graph<true>
//! Transparent shared_ptr to the graph
// Note: Transparent shared_ptr in SWIG can't be compiled for the returning value before the SWIG 4 of Sept-2017
%shared_ptr(Graph<false>);
%shared_ptr(Graph<true>);
//%unique_ptr(Graph<false>);

// ATTENTION: SWIG shared_ptr must be declared for all the inheritance hierarchy,
// so it is not applicable for the Hierarchy template because of the hidden subclasses
////! Transparent shared_ptr to the hierarchy of clusters
//%shared_ptr(Hierarchy<>);
//%unique_ptr(Hierarchy<>);

//template <typename LinksT>
//using Nodes = StoredItems<Node<LinksT>>;
%define SimpleNodes
StoredItems<Node<SimpleLinks>>
%enddef

%define WeightedNodes
StoredItems<Node<WeightedLinks>>
%enddef

//// ATTENTION: %sharep_ptr should be declared before any types being used
////! Transparent shared_ptr for the nodes
%shared_ptr(SimpleNodes);
%shared_ptr(WeightedNodes);
////%unique_ptr(Nodes<>);
// Note: Transparent shared_ptr of the template causes suppression of the type definition having the same name as the template
//! Transparent  shared_ptr for the RawMembership
%shared_ptr(RawMembership<false>);  // ! Has the same effect as %shared_ptr(SRawMembership)
%shared_ptr(RawMembership<true>);  // ! Has the same effect as %shared_ptr(SRawMembership)
////%unique_ptr(RawMembership<>);

%{
	//#include "types.h"  // Defines all main types. Includes flags.h
	#include "types.hpp"  // Defines all main types. Includes flags.h
	#include "functionality.h"  // Defines functional interface of the library
	//#include "processing.h"  // Defines accessory functions of the library
	#include "processing.hpp"  // Defines accessory functions of the library
	//#include "graph.h"  // Input graph
	#include "graph.hpp"  // Input graph
	#include "fileio.hpp"  // Parsers & Printers
	// ATTENTION: namespace declaration here (if not included in inside the included headers)
	// is required for the wrappers compilation
	using namespace daoc;
%}

%{
#ifndef MACROVALUE_STR
	#define MACROVALUE_STR(x)  MACRONAME_STR(x)
	#define MACRONAME_STR(x)  #x
#endif  // MACROVALUE_STR
%}
%inline %{
	const char *const swigRevision()
	{
		return MACROVALUE_STR(SWIG_REVISION);
	}
%}

//%ignore IntrinsicsFlagsBase;  // Ignore wrapping of the base types of enums

// Preprocessing macroses see in http://swig.org/Doc3.0/Preprocessor.html#Preprocessor
// Rename some functions
// Note: *:: is applied to both classes and namespaces
%rename(assign)  *::operator =;  // assign, reset, acquire;  Note: even for D and Java it should be renamed
// Rename to acquire the CLSNAME::operator =(CLSNAME&&);

#if defined(SWIGCSHARP) || defined(SWIGD) || defined(SWIGJAVA) || defined(SWIGOCTAVE) || defined(SWIGRUBY)
	%rename(defined)  *::operator bool() const;  // nonempty, notnull, available, initialized, defined
#if defined(SWIGCSHARP) || defined(SWIGD) || defined(SWIGJAVA) || defined(SWIGRUBY)
	%rename(unequal)  *::operator !=;  // Note: even for D and Java it should be renamed
#endif
#if defined(SWIGCSHARP) || defined(SWIGJAVA)
	%rename(equal)  *::operator ==;
	%rename(bitOrAssign)  *::operator |=;
	%rename(bitAndAssign)  *::operator &=;
	%rename(bitAnd)  *::operator &;
	%rename(bitOr)  *::operator |;
	%rename(bitNot)  *::operator ~;  // Inverse
#endif
#endif  // Many langs

#ifdef SWIGJAVA
	// ATTENTION: The wrapper generator internally creates Java proxy classes with
	// anoter internal constructor that uses (long, bool), other signatures are not affected
	%ignore daoc::Graph::Graph(Id nodesNum, bool shuffle);
	// Java can't handle move assignment
	%ignore daoc::Graph::operator =;
#endif

//// TODO: Implement unique_ptr handling on passing as a parameter
//%ignore daoc::cluster(unique_ptr<Nodes(LinksT)> nodes, bool edges, const ClusterOptions& opts=ClusterOptions());

//// Rename some methods for explicit
//%rename(addNodeList) daoc::Graph::addNodes(const Items<Id>& nodesIds, StructNodeErrors* errs=nullptr);

//// Hide some accessory methods required only internally for the SWIG compilation

// Automatically generate doc strings for some target languages
%feature("autodoc","1");
// Note: anyway the package is generated, so a single namespace has no sence,
// but complicates compilation (requires extra macroses) for the target langs
//%feature("nspace") MyWorld::Material::Color;

%import "macrodef.h"  // Apply macro definitions, but do not make wrappers for them
%include "flags.h"
%include "types.h"
%include "functionality.h"
%include "processing.h"
%include "graph.h"
//%include "graph.hpp"
%include "fileio/iotypes.h"
%include "fileio/parser_rcg.h"
%include "fileio/parser_nsl.h"
%include "fileio/parser_cnl.h"
%include "fileio/printer_cnl.h"
%include "fileio/printer_rhb.h"


// ATTENTION: SWIG does not recognize template aliases in the interface, macroses
// can be used as a workaround
// Note: C defines might be more dangereous
%define SimpleLinks
Links<Link<LinkWeight, false>>
%enddef

//using WeightedLink = Link<LinkWeight, true>;
%define WeightedLinks
Links<Link<LinkWeight, true>>
%enddef

//template <typename LinksT>
//using Clusters = StoredItems<Cluster<LinksT>>;
%define SimpleClusters
StoredItems<Cluster<SimpleLinks>>
%enddef

%define WeightedClusters
StoredItems<Cluster<WeightedLinks>>
%enddef

// ATTENTION: the namespace should be specified before the first %import/include of the lib interface
using std::vector;
using std::list;
using std::unordered_map;
using std::string;
using namespace daoc;

// Core Types ------------------------------------------------------------------
// Input Interface ---
//#ifdef SWIGJAVA
//// Extend vector with emplace() methods
//%extend std::vector {
//	%rename(doEmplace) emplace;
//}
//#endif

// ATTENTION: SWIG does not support template aliases in the interface file, but
// understands them in the implementation.
// ATTENTION: Templates should be instantiated before the first use, i.e.
// %template(NewName) TplName<Params>
// should be declared before TplName<Params> is used anywhere even as an argument !!!

//template <bool WEIGHTED=false> struct InpLink;
// ATTENTION: prevents compilation on Java because the template specializations
// consume vaious amount of memory
#if !(defined(SWIGCSHARP) || defined(SWIGD) || defined(SWIGJAVA))
// Note: distinct naming fails compilation of the Java wrapper
// ATTENTION: SWIG requires explicit namespace qualification on first instatiation of the template
%template(SInpLink) InpLink<false>;  //!< Simple input link
#else
%inline %{
	InpLink<false> makeSInpLink(Id id)  { return InpLink<false>(id); }
%}
#endif  // Not statical typing langs
%template(InpLink) InpLink<true>;  //!< Weighted input link

//#ifndef SWIGJAVA
//!< Simple input links
// ATTENTION: prevents compilation on Java because the template specializations
// consume vaious amount of memory
%template(SInpLinks) Items<InpLink<false>>;
//!< Weighted input links
%template(InpLinks) Items<InpLink<true>>;

%template(Ids) Items<Id>;

// Wrap member template functions of the Graph
%extend daoc::Graph {
	//! Add links of the node
	// template <bool DIRECTED> inline void addNodeLinks;
	%template(addNodeArcs) addNodeLinks<true>;
	%template(addNodeEdges) addNodeLinks<false>;
	
	//! Add node and links
	//template <bool DIRECTED> inline void addNodeAndLinks;
	%template(addNodeAndArcs) addNodeAndLinks<true>;
	%template(addNodeAndEdges) addNodeAndLinks<false>;

	//! Add link of the node
	//! \note This function is much less efficient than addNodeLinks
	// template <bool DIRECTED> inline void addNodeLinks;
	%template(addArc) addLink<true>;
	%template(addEdge) addLink<false>;
}

// ATTENTION: The template should be declared before any of it's specializations
// are used anywhere (including being used as arguments to the shared_ptr<>).
//! Graph specifying the input network
//template <bool LINKS_WEIGHTED=true> class Graph;
//#ifndef SWIGJAVA
%template(SGraph) Graph<false>;  //!< Graph
//#endif // SWIGJAVA
%template(Graph) Graph<true>;  //!< Weighted graph

//! Non-transparent shared_ptr to the graph
//#ifndef SWIGJAVA
%template(SGraphPtr) shared_ptr<Graph<false>>;  //!< Graph
//#endif // SWIGJAVA
%template(GraphPtr) shared_ptr<Graph<true>>;  //!< Weighted graph
////! Non-transparent unique_ptr to the graph
//%template(SGraphUPtr) unique_ptr<Graph<false>>;  //!< Graph
//%template(GraphUPtr) unique_ptr<Graph<true>>;  //!< Weighted graph

//! ATTENTION: SWIG does not recognize template aliases (defined via using):
//! SimpleLink, WeightedLink, Links, Nodes, Clusters, ClusterNodes, NodeShares, ...
// Core Types ---
// template <typename WeightT, bool WEIGHTED> struct Link;
%template(SArc) Link<LinkWeight, false>;  //!< Simple link (SimpleLink)
%template(Arc) Link<LinkWeight, true>;  //!< Weighted link (WeightedLink)

%template(SArcs) SimpleLinks;  //!< Simple links
%template(Arcs) WeightedLinks;  //!< Weighted links

//%template(Context) ...

// ATTENTION: Python & Ruby wrappers generation hangs in SWIG 4 on attempt to wrap
// the abstract class or it's descendants
#if defined(SWIGCSHARP) || defined(SWIGD) || defined(SWIGJAVA)
%template(SNodeI) NodeI<SimpleLinks>;  //!< Simple Item (Node or Cluster)
%template(NodeI) NodeI<WeightedLinks>;  //!< Weighted Item (Node or Cluster)

%template(SNode) Node<SimpleLinks>;  //!< Simple Node
%template(Node) Node<WeightedLinks>;  //!< Weighted Node

%template(SCluster) Cluster<SimpleLinks>;  //!< Simple Cluster
%template(Cluster) Cluster<WeightedLinks>;  //!< Weighted Cluster
#endif  // Statical typing langs

//// SWIG 4 does not support abstract types as container elements
//%template(SNodesI) Items<NodeI<SimpleLinks>>;  //!< Simple Item (Node or Cluster)
//%template(NodesI) Items<NodeI<WeightedLinks>>;  //!< Simple Item (Node or Cluster)

//// SWIG 4 requires copy constructor for the items to be used in the containers
////template <typename LinksT>
////using Nodes = list<Node<LinksT>>;
//%template(SNodes) SimpleNodes;  //!< Simple nodes
//%template(Nodes) WeightedNodes;  //!< Weighted nodes
//
////template <typename LinksT>
////using Clusters = StoredItems<Cluster<LinksT>>;
//%template(SClusters) SimpleClusters;  //!< Simple clusters
//%template(Clusters) WeightedClusters;  //!< Weighted clusters

//! Non-transparent shared_ptr to the Nodes
// NodesPtr is returned by the Graph::release
// Note: %shared_ptr(NodesT) would change semantics of NodesT and prevent their usage without the shared_ptr,
// it's like %template(NodesT) shared_ptr<NodesT>
//template <typename LinksT>
//using Nodes = StoredItems<Node<LinksT>>;  // StoredItems is a list
%template(SNodesPtr) shared_ptr<SimpleNodes>;
%template(NodesPtr) shared_ptr<WeightedNodes>;

////! Non-transparent unique_ptr to the Nodes
//%template(SNodesUPtr) unique_ptr<SimpleNodes>;
//%template(NodesUPtr) unique_ptr<WeightedNodes>;

//! Owner cluster
//template <typename ItemT> struct Owner;
%template(SOwner) Owner<Cluster<SimpleLinks>>;
%template(Owner) Owner<Cluster<WeightedLinks>>;

//// SWIG 4 requires copy constructor for the items to be used in the containers
////! Owner clusters
////template <typename LinksT>
////using Owners = Items<Owner<Cluster<LinksT>>>;
////daoc_python2.cxx:5951:31: error: use of deleted function ‘daoc::Owner<ItemT>& daoc::Owner<ItemT>::operator=(const daoc::Owner<ItemT>&) [with ItemT = daoc::Cluster<std::vector<daoc::Link<float, false>, std::allocator<daoc::Link<float, false> > > >]’
////       *(swig::getpos(self,i)) = x;
//%template(SOwners) Items<Owner<Cluster<SimpleLinks>>>;
//%template(Owners) Items<Owner<Cluster<WeightedLinks>>>;

// template <typename LinksT> struct Level;
//! Hierarchy level of clusters
%template(SLevel) Level<SimpleLinks>;
//! Hierarchy level of weighted clusters
%template(Level) Level<WeightedLinks>;

//// SWIG 4 requires copy constructor for the items to be used in the containers
////template <typename LinksT>
////using Levels = vector<Level<LinksT>>;
////! Hierarchy levels of clusters
//%template(SLevels) Items<Level<SimpleLinks>>;
////! Hierarchy levels of weighted clusters
//%template(Levels) Items<Level<WeightedLinks>>;

////template <typename LinksT>
////using ClusterNodes = unordered_map<Node<LinksT>*, Share>;
////! Nodes with simple links of the unwrapped cluster, unordered
//%template(SClusterNodes) unordered_map<Node<SimpleLinks>*, Share>;
////! Nodes with weighted links of the unwrapped cluster, unordered
//%template(ClusterNodes) unordered_map<Node<WeightedLinks>*, Share>;

// Main Interface (core functionality) -----------------------------------------
//template <typename LinksT>
//shared_ptr<Hierarchy<LinksT>> cluster(Nodes<LinksT>&& nodes, bool edges, const ClusterOptions& opts=ClusterOptions());
//! Cluster nodes having simple links yielding the hierarchy of clusters
%template(scluster) cluster<SimpleLinks>;
//! Cluster nodes having weighted links yielding the hierarchy of clusters
%template(cluster) cluster<WeightedLinks>;

// ATTENTION: Hierarchy template should be declared before that template specializations
// are used anywhere else (including being used as arguments to the shared_ptr<>).
//! Hierarchy of clusters for nodes with simple links
//template <typename LinksT> class Hierarchy;
%template(SHierarchy) Hierarchy<SimpleLinks>;
//! Hierarchy of clusters for nodes with weighted links
%template(Hierarchy) Hierarchy<WeightedLinks>;

//! Non-transparent shared_ptr to the hierarchy of clusters
%template(SHierarchyPtr) shared_ptr<Hierarchy<SimpleLinks>>;
%template(HierarchyPtr) shared_ptr<Hierarchy<WeightedLinks>>;

////! Non-transparent unique_ptr to the hierarchy of clusters
//%template(SHierarchyUPtr) unique_ptr<Hierarchy<SimpleLinks>>;
//%template(HierarchyUPtr) unique_ptr<Hierarchy<WeightedLinks>>;

//! Evaluate intrinsic measures (modularity, conductance) for the specified clustering (nodes membership)
%template(intrinsicMeasuresSArcs) intrinsicMeasures<true, SimpleLinks>;
%template(intrinsicMeasuresArcs) intrinsicMeasures<true, WeightedLinks>;
%template(intrinsicMeasuresSEdges) intrinsicMeasures<false, SimpleLinks>;
%template(intrinsicMeasuresEdges) intrinsicMeasures<false, WeightedLinks>;
//template <bool NONSYMMETRIC, typename LinksT>
//void intrinsicMeasures(Intrinsics& ins, Clusters<LinksT>& cls, AccWeight weight, AccWeight gamma=1);

// File I/O routines and types -------------------------------------------------
//! Build nodes graph (input graph for the clustering) from the network file having NSL (nsa/e) format
// template <typename GraphT> GraphT build();
%template(sbuild) NslParser::build<Graph<false>>;  //!< Buid simple graph
%template(build) NslParser::build<Graph<true>>;  //!< Buid weighted graph

//! Build nodes graph (input graph for the clustering) from the network file having RCG format
%template(sbuild) RcgParser::build<Graph<false>>;  //!< Buid simple graph
%template(build) RcgParser::build<Graph<true>>;  //!< Buid weighted graph

// CnlParser related types
//! Share (part) of the node caused by overlaps
%template(SNodeShare) ItemShare<SimpleLinks>;	// ItemShare<Cluster<LinksT>>
//! Share (part) of the weighted node caused by overlaps
%template(NodeShare) ItemShare<WeightedLinks>;

//! Shares of the node
#ifndef SWIGJAVA
// ATTENTION: SWIG Version 4.0.0, rev 897fe67 generates "File name too long"  error
// for the Java compilation.
//template <typename LinksT> using NodeShares;
%template(SNodeShares) NodeShares<SimpleLinks>;
%template(NodeShares) NodeShares<WeightedLinks>;
#endif  // SWIGJAVA

//! Shares of the nodes
//template <typename LinksT> using NodesShares;
%template(SNodesShares) NodesShares<SimpleLinks>;
%template(NodesShares) NodesShares<WeightedLinks>;

//! Raw membership of nodes in clusters
//template <typename LinksT> struct RawMembership;
%template(SRawMembership) RawMembership<SimpleLinks>;
%template(RawMembership) RawMembership<WeightedLinks>;

//! Build nodes membership from the clustering file having CNL format
//template <typename LinksT> RawMembership<LinksT> CnlParser::build;
%template(sbuild) CnlParser::build<SimpleLinks>;
%template(build) CnlParser::build<WeightedLinks>;

//! Clustering printer used to output the resulting level(s) of clusters hierarchy in the CNL format
//template <typename LinksT> class CnlPrinter;
%template(SCnlPrinter) CnlPrinter<SimpleLinks>;
%template(CnlPrinter) CnlPrinter<WeightedLinks>;

//! Hierarchy printer used to output the hierarchy in the RHB format
//template <typename LinksT> class RhbPrinter;
%template(SRhbPrinter) RhbPrinter<SimpleLinks>;
%template(RhbPrinter) RhbPrinter<WeightedLinks>;

//! Load clusters from the file
//template <typename ParserT, typename GraphT> AccWeight loadClusters;
%template(sloadClusters) loadClusters<CnlParser, Graph<false>>;
%template(loadClusters) loadClusters<CnlParser, Graph<true>>;

// Accessory Functions ---------------------------------------------------------
// template <typename LinksT> struct AccLink;
%template(SCArc) AccLink<SimpleLinks>;  //!< Link of the cluster
%template(CArc) AccLink<WeightedLinks>;  //!< Link of the cluster having weighted nodes

%template(addSCArc) addLink<SimpleLinks>;
%template(addCArc) addLink<SimpleLinks>;

//template <bool NONSYMMETRIC, typename NodesT>
//void validate(NodesT& nodes, AccWeight& weight, Size& linksNum, bool severe);
%template(validateSArcs) validate<true, SimpleNodes>;
%template(validateArcs) validate<true, WeightedNodes>;
%template(validateSEdges) validate<false, SimpleNodes>;
%template(validateEdges) validate<false, WeightedNodes>;

%template(SNodeArcsWeight) linksWeight<true, Node<SimpleLinks>>;
%template(NodeArcsWeight) linksWeight<true, Node<WeightedLinks>>;
%template(SNodeEdgesWeight) linksWeight<false, Node<SimpleLinks>>;
%template(NodeEdgesWeight) linksWeight<false, Node<WeightedLinks>>;

//template <bool NONSYMMETRIC, typename ItemT>
//AccWeight linksWeight(const ItemT& el);
%template(scarcsWeight) linksWeight<true, Cluster<SimpleLinks>>;
%template(carcsWeight) linksWeight<true, Cluster<WeightedLinks>>;
%template(scedgesWeight) linksWeight<false, Cluster<SimpleLinks>>;
%template(cedgesWeight) linksWeight<false, Cluster<WeightedLinks>>;

//%template(SCluster) Cluster<SimpleLinks>;  //!< Cluster of nodes
//%template(Cluster) Cluster<WeightedLinks>;  //!< Cluster of nodes having weighted links

//%template(SNode) Node<SimpleLinks>;  //!< Node with simple links
//%template(Node) Node<WeightedLinks>;  //!< Node with weighted links
