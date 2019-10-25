//! \brief The Dao of Clustering library - Robust & Fine-grained Deterministic Clustering for Large Networks
//! 	C++ client app of the library.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! >	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2014-11-02
#ifndef CLIENT_H
#define CLIENT_H

#include <chrono>  // For the execution timing

#include "all.hpp"

using std::unique_ptr;
using namespace std::chrono;
using namespace daoc;


using Timestamp = time_point<steady_clock>;  //!< Time stamp
//using Timing = time_point<steady_clock>;  //!< Execution timing

//! \brief Execution times tracer
class Timing {
private:
	Timestamp  m_mark;  // Latest time stamp (construction or update time)
public:
	uint64_t  loadnet;  //!< Input network loading time
	uint64_t  loadcls;  //!< Evaluating clusters loading time
	uint64_t  cluster;  //!< Clustering time (duration)
	uint64_t  evaluate;  //!< Evaluation time (duration)
	uint64_t  outpfile;  //!< Results serialization time
	uint64_t  outpterm;  //!< Results output time to the terminal

	//! \brief Trace timing to the specified file
	//!
	//! \param mcsec uint64_t  - time duration to be traced
	//! \param prefix="" const char*  - output prefix
	//! \param fout=stdout FILE*  - output file
	//! \return void
	static void print(uint64_t mcsec, const char* prefix="", FILE* fout=stdout);

	Timing() noexcept: m_mark(steady_clock::now()), loadnet(), loadcls(), cluster()
		, evaluate(), outpfile(), outpterm()  {}

	Timing(Timing&&)=default;
	Timing(const Timing&)=delete;

	Timing& operator =(Timing&&)=default;
	Timing& operator =(const Timing&)=delete;

    //! \brief Update timestamp returning the duration in mcs
    //!
    //! \return Timestamp  - duration (mcs) since the last update
	uint64_t update();
};

//!< Processing and Output Options
struct Options {
	//! Hierarchy output format to the terminal:
	//! 'n' - none (omit the output), 't' - text, 'c' - csv, 'j' - json
	char  toutfmt;
	//! Extended hierarchy output format to the terminal:
	//! 0  - none, 1  - show inter-cluster links, 2  - unwrap root clusters to nodes
	uint8_t  extoutp;
	ClusterOptions  clustering;  //!< Clustering options
#if FEATURE_EMBEDDINGS >= 1
	unique_ptr<NodeVecOptions>  nodevec;  //!< Node vectorization options
#endif // FEATURE_EMBEDDINGS
	vector<OutputOptions>  outputs;  //! Series of clustering (hierarchy) output options
    unique_ptr<Timing>  timing;  //! Execution timing

	Options() noexcept: toutfmt('n'), extoutp(false), clustering()
#if FEATURE_EMBEDDINGS >= 1
		, nodevec()
#endif // FEATURE_EMBEDDINGS
		, outputs(), timing()  {}
};

//! \brief Client of the clustering library.
//! Prepares input data for the clustering based on console input
//!
//! Typical usage:
//! \code{.cpp}
//! Client  client;
//!	if(client.parseArgs(argc, argv))
//!		client.process();
//!	else client.info(argv[0]);
//!	return 0;
//! \endcode
class Client {
public:
	Client() noexcept;  // Note: explicit constructor to always initialize POD types
	Client(const Client& c)=delete;
	Client& operator=(const Client& c)=delete;

    //! \brief Internal revision of the client
    //! \note Use rev(fullrev) to fetch the user-friendly representation
#ifndef REVISION
#error REVISION is not defined
#endif // REV
    constexpr static char const*  fullrev = MACROVALUE_STR(REVISION);

    constexpr static Clustering   clustering =
#ifdef MEMBERSHARE_BYCANDS
		Clustering::FUZZY_OVP
#else // MEMBERSHARE_BYCANDS
		Clustering::CRISP_OVP
#endif // MEMBERSHARE_BYCANDS
#ifndef GESTCHAINS_MCANDS
		| Clustering::CHAINS_EXTRA
#endif // !GESTCHAINS_MCANDS
#ifdef PREFILTER_OFF
		| Clustering::MCANDS_NOFLT
#endif // PREFILTER_OFF
	;

    //! \brief Parse arguments from console and convert to internal representation
    //!
    //! \param argc int  - number of parameters
    //! \param argv[] char*  - array of string parameters
    //! \return bool  - whether parameters are successfully processed
	bool parseArgs(int argc, char *argv[]);

    //! \brief Output usage and / or version information to stdout
    //!
    //! \param filename[] const char  - executable filename
    //! \return void
	void info(const char filename[]) const;

    //! \brief Build and process the graph using parsed arguments
    //!
    //! \return void
	void execute();

    //! \brief Build and process the graph using parsed arguments
	//! 	and the specified parser of the input network (graph)
    //! \return void
	template <typename ParserT>
	void execute();

    //! \brief Perform the graph (input network) clustering using input parameters
    //!
    //! \param graph Graph<WEIGHTED>&  - input graph to be processed
    //! \return void
	template <bool WEIGHTED=true>
	void process(Graph<WEIGHTED>& graph);

    //! \brief Build hierarchy from the nodes and output the results
    //! 	Outputs results to stdout, stderr and corresponding files (clsfile)
	//! \pre Node links are ordered by bsDest() and UNIQUEOptions
	//! \post Nodes are moved to the hierarchy, i.e. the external argument nodes are reseted
    //!
    //! \param[in] nodes Nodes<LinksT>&  - nodes with links to be processed
    //! \param edges bool  - whether links are edges (undirected, have symmetric weights)
    //! \param opts const Options&  - processing and output options
    //! \param showver=false bool  - show executable version to the terminal in the end of the execution
    //! \return void
    // TODO: separate hierarchy output and move it to the library as default output
	template <typename LinksT>
	static void processNodes(Nodes<LinksT>& nodes, bool edges
		, const Options& opts=Options(), bool showver=false);
private:
	InpOptions  m_inpopts;  // Input options
	Intrinsics  m_evals;  // Whether to evaluate specified cluster file or form it
	Options  m_opts;  // Processing and output options
	uint8_t m_showver;  // Show version of the executable: 1 - brief, 3 - full (actual only for the usage info)
};

//! \brief Client build info
const BuildInfo& clientBuild();

#endif // CLIENT_H
