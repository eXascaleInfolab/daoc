//! \brief The Dao of Clustering library - Robust & Fine-grained Deterministic Clustering for Large Networks
//! 	Command-line Interface implemented as C++ client app of the library.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! >	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2014-09-04

#include <stdexcept>
#include <cstring>  // strlen
#include "client.h"

using std::logic_error;


// Hardcoded usecases ---------------------------------------------------------
//! \brief Execute hardcoded ecample
//!
//! \param example uint8_t  - example to be executed
//! \return void
void testcase(uint8_t example=0)
{
	fprintf(ftrace, "-Hardcoded example\n");
	// Simple Usecases ----------------------------------------------------
	StructNodeErrors  nderrs("WARNING build(), the duplicated nodes are skipped: ");
	StructLinkErrors  lnerrs("WARNING build(), the duplicated links are skipped: ");
//	using GraphT = Graph<false>;  // Unweighted
	using GraphT = Graph<true>;  // Weighted
	GraphT  graph;
	constexpr bool  DIRECTED = false;
	if(example >= 3) {
		// Simple triangle
		if(example == 3 || example / 10 == 3) {
			graph.addNodes({0, 1, 2}, &nderrs);
			graph.addNodeLinks<DIRECTED>(0, {1, 2}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(1, {2}, &lnerrs);
			// With separate node
			if(example >= 30)
				graph.addNodes({3}, &nderrs);

			// Extension to simple square
			if(example == 31)
				graph.addNodeLinks<DIRECTED>(3, {1, 2}, &lnerrs);
		}

		// Simple square
		else if(example == 4) {
			graph.addNodes(3, 0, &nderrs);  // 3 Nodes starting from id = 0
			graph.addNodeLinks<DIRECTED>(0, {1, 2});
			graph.addNodeLinks<DIRECTED>(3, {1, 2});
		}

		// Simple pentagon (5)
		else if(example == 5) {
			graph.addNodes(5);
			graph.addNodeLinks<DIRECTED>(0, {1, 2}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(3, {1, 4}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(2, {4}, &lnerrs);
		}

		// Simple hexagon (6)  (8 ~ 6: orig Q0 > 0, dQ0 > 0, Q1 < 0)
		else if(example == 6) {
			graph.addNodes({0, 1, 2, 3, 4, 5}, &nderrs);
			graph.addNodeLinks<DIRECTED>(0, {1, 2}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(3, {1, 5}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(4, {2, 5}, &lnerrs);
		}

		// Simple decagon (10) (orig Q0, dQ0, Q1 > 0)
		else if(example == 10) {
			GraphT::Ids  ids;
			ids.reserve(10);
			for(Id i = 0; i < 10; ++i)
				ids.push_back(i);
			graph.addNodes(ids, &nderrs);
			graph.addNodeLinks<DIRECTED>(0, {1, 2}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(3, {1, 5}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(4, {2, 6}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(7, {5, 9}, &lnerrs);
			graph.addNodeLinks<DIRECTED>(8, {6, 9}, &lnerrs);
		}
	}

	// Basic 3x overlapping
	if(graph.nodes().empty()) {
		graph.addNodes({0, 1, 2, 3}, &nderrs);
		using InpLinkT = GraphT::InpLinkT;
		graph.addNodeLinks<true>(0, {InpLinkT(0, 6)}, &lnerrs);
		graph.addNodeLinks<true>(1, {InpLinkT(1, 6)}, &lnerrs);
		graph.addNodeLinks<true>(3, {InpLinkT(3, 6)}, &lnerrs);
		graph.addNodeLinks<false>(2, {0, 1, 3}, &lnerrs);
	}
#if TRACE >= 1
	nderrs.show();
	lnerrs.show();
#endif // TRACE

	const bool edges = !graph.directed();
	Client::processNodes(*graph.release(), edges);
}


//! A client of the clustering lib
int main(int argc, char* argv[])
{
	// Check matching of the precompiled clustering strategies of the client and library
	if(!clientBuild().compatibleWith(libBuild())) {
		fprintf(ftrace, "main(), client macro definitions (%s) are not compatible with the library (%s)."
			"\n= Library Build =\n%s\n= Client Build =\n%s"
			, to_string(clientBuild().features).append("; ").append(to_string(clientBuild().clustering)).c_str()
			, to_string(libBuild().features).append("; ").append(to_string(libBuild().clustering)).c_str()
			, libBuild().summary().c_str(), clientBuild().summary().c_str());
		throw logic_error("Precompiled clustering strategy of the client does not match the library\n");
	}

	// Check for the hardcoded example call:
	// <app> #<example_number>
	if(argc == 2 && strlen(argv[1]) >= 2  && argv[1][0] == '#') {
		auto val = atoi(&argv[1][1]);
		uint8_t  exampleMax = -1;
		if(val >= 0 && val < exampleMax) {
			testcase(val);
			return 0;
		}
	}

	// Standard execution
	Client  client;
	if(client.parseArgs(argc, argv))
		client.execute();
	else client.info(argv[0]);
	return 0;
}
