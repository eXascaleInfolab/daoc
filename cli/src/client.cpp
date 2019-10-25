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

#include <cstdio>
#include <iostream>  // Input file processing
#include <utility>  // make_pair, forward
#include <limits>  //  numeric_limits
#include <stdexcept>  // Exception (for Arguments processing)
#include <cstdlib>  // strtof
#include <cstring>  // strchr, strerror
#include <algorithm>  // sort(), swap(), max(), move[container items]()
#include <cassert>  // assert

#include "fileio.hpp"
#include "client.h"

// NOTE: Most of the client output is prepended with '-' prefix to distinguish it
// from the library output to distinguish the actual results from the tracing.
//
// MACROSES:
// See global macroses in macrodef.h.

// Conditional inclusion of the CLI arguments (options)
#define OPT_R_  (FEATURE_CLUSTERING >= 2)  // Reduce items by non significant links

#define  OPT_E_  (FEATURE_CLUSTERING >= 5)  // Evaluate intrinsic measure(s)
#define  OPT_CX_  (FEATURE_CLUSTERING >= 5)  // Per-level clusters, all unique clusters and the whole hierarchy output


using std::vector;
using std::move;
using std::domain_error;
using std::out_of_range;
using std::invalid_argument;
using std::endl;
using std::to_string;
using namespace daoc;


// Formatting helpers ---------------------------------------------------------
//! \brief Output items as a string
//! \tparam ItemsT  - container type
//! \tparam PTR=true  - items are pointers
//!
//! \param els const ItemsT&  - items to be outputted
//! \param ' char delim='  - items delimiter
//! \param strict=false bool  - whether to output a special mark for the empty container
//! \param prefix="" const string&  - prefix for the outputting item
//! \param suffix="" const string&  - suffix for the outputting item
//! \return string  - items in the string form
template <typename ItemsT, bool PTR=true>
string itemsToStr(const ItemsT& els, char delim=' ', bool strict=false
	, const string& prefix="", const string& suffix="")
{
	string str;

	if(!els.empty()) {
		str = prefix;
		for(auto& c: els)
			str += to_string(c->id) += delim;
		str.pop_back();
		str += suffix;
	} else if(!strict)
		str = "-";

	return str;
}

//! \brief Output items .dest as a string
template <typename ItemsT>
string itemsDestToStr(const ItemsT& els, char delim=' ', bool strict=false
	, const string& prefix="", const string& suffix="")
{
	string str;

	if(!els.empty()) {
		str = prefix;
		for(auto& c: els)
			str += to_string(c.dest->id) += delim;
		str.pop_back();
		str += suffix;
	} else if(!strict)
		str = "-";

	return str;
}

template <typename LinksT>
string linksToStr(const LinksT& ls)
{
	string str;

	if(!ls.empty()) {
		for(const auto& ln: ls)
			str += to_string(ln.dest->id).append(" ");
	} else str = "-";

	return str;
}

// Clusters output -------------------------------------------------------------
//! \brief Output nodes-cluster membership into the file
//!
//! \param cid Id  - id of the cluster to be outputted
//! \param cnodes ClusterNodes<LinksT>&  - member nodes with shares
//! \param fout FileWrapper&  - output file
//! \param clsfmt ClsOutFmt  - output format
//! \return void
template <typename LinksT>
void outpMembership(Id cid, ClusterNodes<LinksT>& cnodes, FileWrapper& fout, ClsOutFmtBase clsfmt)
{
	assert(!cnodes.empty() && "rawOutpCluster(), clusters must be unwrapable");
	// Output cluster id if required
	const bool  numbered = isset(clsfmt, ClsOutFmt::EXTENDED);
	const bool  shares = numbered || isset(clsfmt, ClsOutFmt::SHARED);
	if(numbered)
		// TODO: consider possibility of dropping clusters with the share <= 0.5 for !EXTENDED
		fputs((to_string(cid) += "> ").c_str(), fout);
	// Output cluster nodes with shares if required
	for(auto& cnd: cnodes)
		if(shares //&& less(cnd.share, Share(1)
		&& !equal(cnd.share, Share(1) / cnd.dest->owners.size(), cnd.dest->owners.size()))
			fprintf(fout, "%u:%G ", cnd.dest->id, cnd.share);
		else fprintf(fout, "%u ", cnd.dest->id);
	fputc('\n', fout);
}

// File Name forming helpers --------------------------------------------------
//! \brief Replace file extension
//!
//! \param fullname string  - full file name to update the extension
//! \param newext const string&  - new file extension to be set including the starting '.'
//! \return string  - resulting updated file name
string replaceExt(string fullname, const string& newext)
{
	// Ensure to modify only the file name, not the parent directory
	auto posb = fullname.rfind('/');  // Begin of the file name
	if(posb == string::npos)
		posb = 0;
	// Find index of the extension (or part of the parent dir name
	auto pos = fullname.rfind('.');
	if(pos != string::npos && pos > posb)
		fullname.resize(pos);  // Trunc also the ending '.'
	return fullname += newext;
}

//! \brief Form output file name from the input file name
//!
//! \param outopt const OutputOptions&  - output options to be encoded in the output name
//! \param inpfname const string&  - input file name
//! \return string  - resulting output file name
string outpFileName(const OutputOptions& outopt, const string& inpfname) {
	string  suf("_");  // Output file name suffix with the file extension
	const auto  outfmt = toClsOutFmt(outopt.clsfmt & ClsOutFmt::MASK_OUTSTRUCT);
	switch(outfmt) {
	case ClsOutFmt::ROOT:
		suf.append("r.") += FileExts::CNL;
		break;
	case ClsOutFmt::PERLEVEL:
		suf.append("la.") += FileExts::CNL;
		break;
	case ClsOutFmt::CUSTLEVS:
	case ClsOutFmt::CUSTLEVS_APPROXNUM:
		suf += outfmt == ClsOutFmt::CUSTLEVS ? "lc" : "lp";
		// Consider CustlevsOptions
		switch(outopt.custlevs.levmarg) {
		case LevMargKind::CLSNUM:
			suf += "-n";
			break;
		case LevMargKind::LEVID:
			suf += "-i";
			break;
		case LevMargKind::LEVSTEPNUM:
			suf += "-s";
			break;
		case LevMargKind::NONE:
		default:
			;
		}
		if(outopt.custlevs.levmarg != LevMargKind::NONE) {
			if(outopt.custlevs.margmin)
				suf += std::to_string(outopt.custlevs.margmin);
			suf += '-';
			if(outopt.custlevs.margmax != ID_NONE) {
				suf += std::to_string(outopt.custlevs.margmax);
			}
			if(outopt.custlevs.clsrstep) {
				suf += '_';
				suf += std::to_string(outopt.custlevs.clsrstep);
			}
		}
		suf.append(".") += FileExts::CNL;
		break;
	case ClsOutFmt::ALLCLS:
		suf.append("ca.") += FileExts::CNL;
		break;
	case ClsOutFmt::HIER:
		suf.append(".") += FileExts::RHB;
		break;
	case ClsOutFmt::SIGNIF_OWNSDIR:
	case ClsOutFmt::SIGNIF_OWNADIR:
	case ClsOutFmt::SIGNIF_OWNSHIER:
	case ClsOutFmt::SIGNIF_OWNAHIER:
	case ClsOutFmt::SIGNIF_DEFAULT:
		switch(outfmt) {
			case ClsOutFmt::SIGNIF_OWNSDIR:
				suf += "sd";
				break;
			case ClsOutFmt::SIGNIF_OWNADIR:
				suf += "ad";
				break;
			case ClsOutFmt::SIGNIF_OWNSHIER:
				suf += "sh";
				break;
			case ClsOutFmt::SIGNIF_OWNAHIER:
				suf += "ah";
				break;
			case ClsOutFmt::SIGNIF_DEFAULT:
			default:
				suf += "d";
				break;
		}
		// Consider SignifclsOptions
		if(!equal(outopt.signifcls.densdrop, 1.f))  // Note: densdrop can be >> 1
			suf += std::to_string(outopt.signifcls.densdrop);
		if(less(outopt.signifcls.wrstep, 1.f)) {
			suf += '-';
			if(outopt.signifcls.wrange)
				suf += 'r';
			suf += std::to_string(outopt.signifcls.wrstep);
		}
		if(outopt.signifcls.szmin) {
			suf += '_';
			suf += std::to_string(outopt.signifcls.szmin);
		}
		suf.append(".") += FileExts::CNL;
		break;
	default:
		throw invalid_argument(string("Unexpected type of the clusters output"
			" format for the output file name construction: ").append(
			to_string(toClsOutFmt(outopt.clsfmt))) += '\n');
	}
	return replaceExt(inpfname, suf);
}

// Input arguments processing -------------------------------------------------
void classifyArgs(int argc, char* argv[], vector<string>& opts, vector<string>& files)
{
	opts.clear();
	files.clear();
	if(argc < 2)
		return;

	for(auto i = 1; i < argc; ++i) {
		if(argv[i][0] == '-')
			opts.push_back(argv[i] + 1);  // Skip '-'
		else files.push_back(argv[i]);
	}

#if VALIDATE >= 2
	fprintf(ftrace, "-Arguments are classified:\n-  Options:");
	// Output options arguments
	if(!opts.empty())
		for(const auto& arg: opts)
			fputs((" " + arg).c_str(), ftrace);
	else fprintf(ftrace, " -");
	fprintf(ftrace, "\n-  Files:");
	// Output files arguments
	if(!files.empty())
		for(const auto& arg: files)
			fputs((" " + arg).c_str(), ftrace);
	else fprintf(ftrace, " -");
	fprintf(ftrace, "\n");
#endif // VALIDATE
}

// Timing implementation -----------------------------------------------------
void Timing::print(uint64_t mcsec, const char* prefix, FILE* fout)
{
	assert(fout && "printTiming(), output file should be specified");
	if(fout)
		fprintf(fout, "%s%lu.%06lu sec (%lu h %lu min %lu sec %06lu mcs)\n"
			, prefix, mcsec / 1'000'000, mcsec % 1'000'000, mcsec / 3'600'000'000
			, mcsec / 60'000'000 % 60, mcsec / 1'000'000 % 60, mcsec % 1'000'000);
}

uint64_t Timing::update()
{
	const auto  t = m_mark;
	m_mark = steady_clock::now();
	return duration_cast<microseconds>(m_mark - t).count();
}

// Client implementation ------------------------------------------------------
Client::Client() noexcept: m_inpopts(), m_evals(), m_opts(), m_showver(0)
{
#if TRACE >= 2
	fprintf(ftrace, "Revision: %s.%s\n= Lib Version =\n%s", libBuild().rev().c_str()
		, clientBuild().rev().c_str(), libBuild().summary().c_str());
#endif // TRACE
}

template <typename LinksT>
void Client::processNodes(Nodes<LinksT>& nodes, bool edges, const Options& opts, bool showver)
{
	// Trace input data
#if TRACE >= 2
	fprintf(ftrace, "-Nodes:\n");
	for(auto& nd: nodes) {
#if VALIDATE >= 3  // Note: anyway this validation is performed on hierarchy building
		else assert(sorted(nd.links.begin(), nd.links.end(), bsDest<typename LinksT::value_type>));
#endif // VALIDATE
#if TRACE >= 3
		fprintf(ftrace, "-Node #%2u(%p): ", nd.id, &nd);
		for(const auto& ln: nd.links)
			fprintf(ftrace, " %u(%p):%g", ln.dest->id, ln.dest, ln.weight);
		fputc('\n', ftrace);
#else  // !TRACE_EXTRA
		fprintf(ftrace, "-Node #%2u: %s\n", nd.id, linksToStr(nd.links).c_str());
#endif // TRACE
	}
	fprintf(ftrace, "\n");
#endif // TRACE
	auto hier = cluster(nodes, edges, opts.clustering);
	// Measure the clustering time
	if(opts.timing)
		opts.timing->cluster = opts.timing->update();

	// Output the hierarchy
	if(hier->levels().empty()) {
#if TRACE >= 1
		fprintf(ftrace, "-WARNING processNodes(), the number of hierarchy levels is ZERO.\n"
			"# Q: %G, roots: %lu, levels: %lu, clusters: %lu, nodes: %lu"
			", node links (directed): %lu\n"
			, hier->score().modularity, hier->root().size(), hier->levels().size()
			, hier->score().clusters, hier->nodes().size(), hier->score().nodesLinks);
#else
		puts("-WARNING processNodes(), number of the hierarchy levels is ZERO.\n", ftrace);
#endif // TRACE
		return;
	}

	hier->output(opts.outputs);
	// Measure the file output time
	if(opts.timing)
		opts.timing->outpfile = opts.timing->update();

	if(showver)
		printf("-Rev: %s.%s (%s clustering strategy), filterMarg: %G, edges (symmetric link weights): %d\n"
			, libBuild().rev().c_str(), clientBuild().rev().c_str()
			, to_string(libBuild().clustering).c_str(), opts.clustering.filterMarg, hier->edges());

	// Here Clusters destructors output will be under the strong TRACE macros
	puts("");  // puts() adds newline to the output

	// Measure the terminal output time
	if(opts.timing)
		opts.timing->outpterm = opts.timing->update();
}

bool Client::parseArgs(int argc, char *argv[])
{
	if(argc < 2 || (argc == 2 && !strcmp(argv[argc-1], "-h")))
		return false;
	// Parse input data and perform clustering
	vector<string>  opts;
	vector<string>  files;
	classifyArgs(argc, argv, opts, files);
	bool clsoutp = false;  // Whether clusters output options are specified (possibly, without the output file)

	// Check and apply options
	for(auto& opt: opts) {
		errno = 0;  // Reset the system error
		string errmsg;  // Details about the error
		switch(opt[0]) {
		case 'V':
			if(opt.length() > 2 || (opt.length() == 2 && opt[1] != 'x'))
				throw invalid_argument("Unexpected option.V is provided: -" + opt + "\n");
			if(opt.length() >= 2)
				m_showver = 3;
			else m_showver = 1;
			break;
		case 'c': {
			// Check that it is not used with the clusters evaluation
			if(m_evals)
				throw invalid_argument("Output clusters option (-c) is not compatible"
					" with the clusters evaluation option (-e)\n");

			OutputOptions  outopt;
			uint16_t  iop = 1;  // Index of end of the previous param / begin of the current param
			if(opt.length() > iop && opt[iop] && opt[iop] == 'f') {
				outopt.fltMembers = true;
				++iop;
			}
			if(iop < opt.length()) {
				bool parsed = false;
				switch(opt[iop]) {
				case 'x':
					outopt.clsfmt |= ClsOutFmt::MAXSHARE;  // Note: it is actual only for MEMBERSHARE_BYCANDS
					parsed = true;
					break;
				default:
					//throw out_of_range((string("Provided value of '-c' is out of the expected range: ")
					//	+= opt[iop]).append("\n"));
					break;
				}
				if(parsed)
					++iop;
			}
			if(opt.length() <= iop)
				throw invalid_argument("Unexpected mandatory option is provided: -" + opt + "\n");
			// Output kind of clusters
			switch(opt[iop++]) {
			case 'r':
				outopt.clsfmt |= ClsOutFmt::ROOT;
				break;
			// {S,s}[{s,a}{d,h}[%[b]{<dens_drop>,e,g}][/{<weight_rstep>,e,g}[~]]][_{<cl_szmin>,l2,le,pg,r<base>}]X
			case 's':
			case 'S':
			{
				const bool sowner = opt[iop - 1] == 'S';
				if(opt.length() < iop + 2u || opt[iop] == '_' || opt[iop] == '=' || opt[iop+1] == '=') {
					outopt.clsfmt |= ClsOutFmt::SIGNIF_DEFAULT;
					outopt.signifcls.reset(true, sowner);
					if(opt[iop] != '_')
						break;
					//throw invalid_argument("Invalid format 1 of the option, the option is too short: -"
					//+ opt + "\n");
				}
				// Initialize the suboptions with the default values
				if(opt[iop] != '_') {
					outopt.signifcls.reset(false, sowner);

					bool single = false;
					switch(opt[iop]) {
					case 's':
						single = true;
						break;
					case 'a':
						break;
					default:
						throw invalid_argument("Invalid format for 's/a' of the option: -" + opt + "\n");
					}

					bool direct = false;
					switch(opt[++iop]) {
					case 'd':
						direct = true;
						break;
					case 'h':
						break;
					default:
						throw invalid_argument("Invalid format for 'd/h' of the option: -" + opt + "\n");
					}

					if(direct)
						outopt.clsfmt |= single
							? ClsOutFmt::SIGNIF_OWNSDIR : ClsOutFmt::SIGNIF_OWNADIR;
					else outopt.clsfmt |= single
						? ClsOutFmt::SIGNIF_OWNSHIER : ClsOutFmt::SIGNIF_OWNAHIER;
					++iop;
				}
				// Parsing of the optional sub parameters
				// [%[b]{<dens_drop>,e,g}][/{<weight_rstep>,e,g}[~]]][_{<cl_szmin>,l2,le,pg,r<base>}]
				const char*  optstr = opt.c_str();
				bool subopt = true;  // Suboption parsing
				while(subopt && iop < opt.length() && opt[iop] != '=') {
					switch(opt[iop++]) {
					case '%': {  // [%[b]{<dens_drop>,e,g}]
						if(iop >= opt.length())
							throw invalid_argument("Invalid format for '%' of the option: -" + opt + "\n");
						if(opt[iop] == 'b') {
							outopt.signifcls.densbound = true;
							if(++iop >= opt.length())
								throw invalid_argument("Invalid format for '%b' of the option: -" + opt + "\n");
						}
						// Density drop ratio
						switch(opt[iop]) {
						case 'e':
							outopt.signifcls.densdrop = CEXPM2;  // ~0.865
							++iop;
							break;
						case 'g':
							outopt.signifcls.densdrop = RGOLDINV;  // ~0.618
							++iop;
							break;
						default: {
							char *optvale = nullptr;  // End of the option value
							outopt.signifcls.densdrop = strtof(optstr + iop, &optvale);
							if(!errno && optvale)
								iop = optvale - optstr;
							else errmsg = string("Invalid '%' value in the option -").append(opt);
						}
						}
					} break;
					case '/': {  // /{<weight_rstep>,e,g}[~]
						// Output weight step ratio with the optional "!" suffix
						switch(opt[iop]) {
						case 'e':
							outopt.signifcls.wrstep = CEXPM2;  // ~0.865
							++iop;
							break;
						case 'g':
							outopt.signifcls.wrstep = RGOLDINV;  // ~0.618
							++iop;
							break;
						default: {
							char *optvale = nullptr;  // End of the option value
							outopt.signifcls.wrstep = strtof(optstr + iop, &optvale);
							if(!errno && optvale)
								iop = optvale - optstr;
							else errmsg = string("Invalid '/' value in the option -").append(opt);
						}
						}
						// Apply wrange if "!" is specified
						if(iop < opt.length() && opt[iop] == '~') {
							outopt.signifcls.wrange = true;
							++iop;
						} else outopt.signifcls.wrange = false;
					} break;
					case '_': {  // [_{<cl_szmin>,l2,le,pg,r<base>}]
						// Min size of the output clusters
						if(iop >= opt.length())
							throw invalid_argument("Invalid format for '_' of the option: -" + opt + "\n");
						switch(opt[iop++]) {
						case 'l':
							if(iop >= opt.length() || (opt[iop] != '2' && opt[iop] != 'e'))
								throw invalid_argument("Invalid format of the option: -" + opt + "\n");
							outopt.signifcls.setClszminf(opt[iop++] == '2'
								? SzFnName::CLSSZ_LOG2 : SzFnName::CLSSZ_LOGE);
							break;
						case 'p':
							if(iop >= opt.length() || opt[iop] != 'g')
								throw invalid_argument("Invalid format of the option: -" + opt + "\n");
							outopt.signifcls.setClszminf(SzFnName::CLSSZ_PRGOLDINV);
							++iop;
							break;
						case 'r': {
							// Root base
							char *optvale = nullptr;  // End of the option value
							const auto  rtbase = strtoul(optstr + iop, &optvale, 10);
							if(!errno && optvale) {
								iop = optvale - optstr;
								if(rtbase < RBMIN || rtbase > RBMAX)
									throw invalid_argument("Invalid root base in the option: -" + opt + "\n");
								outopt.signifcls.clszminf = rootOf<RBMIN, RBMAX+1>(rtbase);
								outopt.signifcls.szmin = 0;
							} else errmsg = string("Invalid 'r' value in the option -").append(opt);
						} break;
						default: {
							// Direct cl_szmin value
							char *optvale = nullptr;  // End of the option value
							// ATTENTION: --iop to retain the switch value
							outopt.signifcls.szmin = strtoul(optstr + --iop, &optvale, 10);
#if TRACE >= 3
							fprintf(ftrace, "parseArgs(), size opt: %c, val: %u\n"
								, iop < opt.length() ? opt[iop] : 0, outopt.signifcls.szmin);
#endif // TRACE
							if(!errno && optvale && (outopt.signifcls.szmin || opt[iop] == '0'))
								iop = optvale - optstr;
							else errmsg = string("Invalid 'r' value in the option -").append(opt);
						} break;
						}
					} break;
					default:
						--iop; // Retain the last value
#if TRACE >= 3
						fprintf(ftrace, "parseArgs(), rem opt: %c\n", iop <= opt.length() ? opt[iop] : 0);
#endif // TRACE
						// Note: common optional suffix can be present
						//throw invalid_argument("Invalid format 5 of the option: -" + opt + "\n");
						subopt = false;
						break;
					}
					if(errmsg.size())
						iop = opt.length();  // The remained sub-options are invalid
				}
#if VALIDATE >= 1
				outopt.signifcls.validate();
#endif // VALIDATE
			} break;
#if OPT_CX_
			case 'l': {
				// Initialize the suboptions with the default values
				outopt.custlevs.reset();

				const char ctlsmb = opt[iop++];  // Note: increment iop second type after the ctlsmb assigning
				char *optvale = nullptr;  // End of the option value
				const char*  optstr = opt.c_str();
				errno = 0;
				outopt.custlevs.levmarg = LevMargKind::CLSNUM;
				switch(ctlsmb) {
				case '[':
					if(opt.length() < iop + 2u)  // 1 smb for clsnum + ']'
						throw invalid_argument("Invalid format 1 of the option, the option is too short: -"
						+ opt + "\n");
					outopt.clsfmt |= ClsOutFmt::CUSTLEVS;
					if(opt[iop] != ':') {
						if(opt[iop] == '%') {
							outopt.custlevs.levmarg = LevMargKind::LEVSTEPNUM;
							if(opt[(++iop)++] != '#' || opt.length() < iop + 2u)  // 1 smb for clsnum + ']'
								throw invalid_argument("Invalid format 2 of the option, the option is too short: -"
								+ opt + "\n");
						} else if(opt[iop] == '#') {
							outopt.custlevs.levmarg = LevMargKind::LEVID;
							if(opt.length() < ++iop + 2u)  // 1 smb for clsnum + ']'
								throw invalid_argument("Invalid format 3 of the option, the option is too short: -"
								+ opt + "\n");
						}
						if(opt[iop] != ':') {
							outopt.custlevs.margmin = strtoul(optstr + iop, &optvale, 10);  // Note: inability to convert results in 0
							if(!errno && optvale)
								iop = optvale - optstr;
							else errmsg = string("Invalid margmin value in the option -").append(opt);
						}
					}
					// Read max number of clusters if required
					if(opt.length() > iop && opt[iop] == ':') {
						if(opt[++iop] != '/') {
							outopt.custlevs.margmax = strtoul(optstr + iop, &optvale, 10);
							if(!errno && optvale)
								iop = optvale - optstr;
							else errmsg = string("Invalid margmax value in the option -").append(opt);
						}
						// Read the step ratio if required
						if(opt[iop] == '/') {
							outopt.custlevs.clsrstep = strtof(optstr + ++iop, &optvale);
							if(!errno && optvale)
								iop = optvale - optstr;
							else errmsg = string("Invalid '/' value in the option -").append(opt);
						}
					}
					// Check for the closing ']'
					if(opt.length() <= iop || opt[iop] != ']')
						throw invalid_argument("Invalid format 4 of the option, or the closing ']' is missed: -"
						+ opt + ". Expected types: [uint:uint/float]\n");
					++iop;  // Skip ']'
					break;
				case '~':
					outopt.clsfmt |= ClsOutFmt::CUSTLEVS_APPROXNUM;
					outopt.custlevs.margmin = strtoul(optstr + iop, &optvale, 10);  // Note: unability to convert results in 0
					if(!errno && optvale)
						iop = optvale - optstr;
					else errmsg = string("Invalid '~' value in the option -").append(opt);
					break;
				default:
					--iop; // Retain the last value
					// "-cl[X]=" format case. Output file format is processed in the following section
					outopt.clsfmt |= ClsOutFmt::PERLEVEL;
					break;
					//throw invalid_argument("Invalid format of the option, either"
					//	"an approximate number of clusters or a range is expected: -"
					//	+ opt + "\n");
				}
			} break;
			case 'a':
				outopt.clsfmt |= ClsOutFmt::ALLCLS;
				break;
			case 'h':
				outopt.clsfmt |= ClsOutFmt::HIER;
				break;
#endif // OPT_CX_
			default:
				throw out_of_range("Unexpected option[2].1 is provided (" + string(1, opt[iop]) + "): -" + opt + "\n");
			}
			// Output file format
#if TRACE >= 3
			fprintf(ftrace, "parseArgs(), filefmt opt: %c\n", iop < opt.length() ? opt[iop] : 0);
			//exit(0);
#endif // TRACE
			if(opt.length() > iop && opt[iop] != '=') {
				if(isset(outopt.clsfmt, ClsOutFmt::HIER))
					throw invalid_argument("Invalid format of the option (hierarchy output"
						" does not have extended specification): -" + opt + "\n");
				switch(opt[iop]) {
				case 'p':
					outopt.clsfmt |= ClsOutFmt::PURE;
					break;
				case 's':
					outopt.clsfmt |= ClsOutFmt::SIMPLE;
					break;
				case 'h':
					outopt.clsfmt |= ClsOutFmt::SHARED;
					break;
				case 'e':
					outopt.clsfmt |= ClsOutFmt::EXTENDED;
					break;
				default:
					throw out_of_range("Unexpected option[" + std::to_string(iop)
					+ "].2 is provided (" + opt[iop] + "): -" + opt + "\n");
				}
				++iop;
			} else outopt.clsfmt |= ClsOutFmt::DEFAULT & ClsOutFmt::MASK_FILEFMT;
			// Note: output file name can be omitted to use a default one
			if(opt.length() > iop && (opt[iop] != '=' || opt.length() == iop+1u
			|| (opt.length() == iop+2u && opt[iop+1] == '.')
			|| (opt.length() == iop+3u && opt[iop+1] == '.' && opt[iop+2] == '.')))
				throw invalid_argument("The filename is not specified in: -" + opt + "\n");
			// Consider quotes and skip them, skip '='
			if(iop < opt.length()) {
				if(opt[iop++] != '=' || opt.length() <= iop)
					throw invalid_argument("Unexpected option.c is provided: -" + opt + "\n");
				const bool  quoted = (opt[iop] == '"' && opt.back() == '"')
					|| (opt[iop] == '\'' && opt.back() == '\'');
				if(iop >= opt.length() + 1 + quoted * 2)
					throw invalid_argument("Unexpected option.c is provided: -" + opt + "\n");
				if(quoted) {
					++iop;
					opt.pop_back();
				}
				outopt.clsfile = opt.erase(0, iop);
			}
			else assert(!outopt.clsfile.empty() && "parseArgs(), default clsfile is expected");
			clsoutp = true;
			//fprintf(ftrace, "fmt: %#x, file: %s\n", outopt.clsfmt, outopt.clsfile.c_str());
			m_opts.outputs.push_back(move(outopt));
		} break;
#if OPT_E_
		case 'e': {
			// Check that it is not used with the clusters output
			if(clsoutp || !m_opts.outputs.empty())
				throw invalid_argument("Clusters evaluation option (-e) is not compatible with"
					" the clusters output option (-c) and only one evaluation option is expected\n");

			// The filename is mandatory
			OutputOptions  outopt;
			outopt.clsfile.clear();
			size_t  iop = opt.find('=', 1);  // Index of the filename with '=' prefix
			if(iop == string::npos
			|| (opt.length() == iop+2 && opt[iop+1] == '.')
			|| (opt.length() == iop+3 && opt[iop+1] == '.' && opt[iop+2] == '.'))
				throw invalid_argument("The filename is not specified in: -" + opt + "\n");

			// Check what evaluations should be performed
			if(iop >= 2) {
#if VALIDATE >= 2
				assert(iop - 1 <= 3
					&& "parseArgs(), evaluation option IntrinsicsFlags validation failed");
#endif // VALIDATE
				for(size_t i = 1; i < iop; ++i)
					switch(opt[i]) {
					case 'c':
						m_evals.flags |= IntrinsicsFlags::CONDUCTANCE;
						break;
					case 'm':
						m_evals.flags |= IntrinsicsFlags::MODULARITY;
						break;
					case 'g':
						m_evals.flags |= IntrinsicsFlags::GAMMA;
						break;
					default:
						throw invalid_argument("Invalid IntrinsicsFlags in: -" + opt + "\n");
					}
			} else m_evals.flags = IntrinsicsFlags::ALL;

			// Process the filename: consider quotes and skip them, skip '='
			if(opt[iop++] != '=' || opt.length() <= iop)
				throw invalid_argument("Unexpected option.e is provided: -" + opt + "\n");
			const bool  quoted = (opt[iop] == '"' && opt.back() == '"')
				|| (opt[iop] == '\'' && opt.back() == '\'');
			if(iop >= opt.length() + 1 + quoted * 2)
				throw invalid_argument("Unexpected option.e is provided: -" + opt + "\n");
			if(quoted) {
				++iop;
				opt.pop_back();
			}
			outopt.clsfile = opt.erase(0, iop);
			m_opts.outputs.push_back(move(outopt));
#if TRACE >= 3
			fprintf(ftrace, "parseArgs(), evals flags: %s, outputs (%lu): %s, opt: %s\n", to_string(m_evals.flags).c_str()
				, m_opts.outputs.size(), m_opts.outputs.front().clsfile.c_str(), opt.c_str());
#endif // TRACE
		} break;
#endif  // OPT_E_
		case 'a':
			if(opt.length() >= 2)
				throw invalid_argument("Unexpected option.a is provided: -" + opt + "\n");
			m_inpopts.sumdups = true;
			break;
		// -g=<resolution> | -gr[<step_ratio>][:[<step_ratio_max>]][=[<gamma_min>][:gamma_max]]
		case 'g': {
			uint8_t  iop = 1;
			if(opt.length() <= iop)
				throw invalid_argument("Unexpected option.g is provided: -" + opt + "\n");
			switch(opt[iop]) {
			case '=': {
				if(opt.length() <= ++iop)
					throw invalid_argument("Unexpected option.g is provided: -" + opt + "\n");
				// Disable the ratio
				m_opts.clustering.gammaRatio = 0;
				// Parse gamma value
				const auto valstr = &opt.c_str()[iop];
				const auto val = strtof(valstr, nullptr);
#ifndef DYNAMIC_GAMMA
				if(val < 0)
					throw out_of_range("Provided value of '-g' is out of the expected range: "
							+ string(valstr) + "\n");
#endif // DYNAMIC_GAMMA
				m_opts.clustering.gamma = val;
			} break;
			case 'r': {  // r[<step_ratio>][:[<step_ratio_max>]]
				if(opt.length() <= ++iop)
					break;
				// Parse the ratio value
				const char*  bval;
				char*  eval;
				if(opt[iop] != ':') {
					bval = opt.c_str() + iop;
					auto val = strtof(bval, &eval);
					if(eval != bval) {
						if(val <= 0 || val >= 1)
							throw invalid_argument("Out of the range (0, 1) option.gr gammaRatio is provided: -" + opt + "\n");
						m_opts.clustering.gammaRatio = val;
						iop += eval - bval;
					}
				}
				if(iop < opt.length() && opt[iop] == ':') {
					if(++iop < opt.length() && opt[iop] != '=') {
						// Parse stepRatioMax E [stepRatio, 1)
						bval = opt.c_str() + ++iop;
						auto val = strtof(bval, &eval);
						if(val < m_opts.clustering.gammaRatio || val >= 1)
							throw invalid_argument("Out of the range (0, 1) option.gr gammaRatioMax is provided: -" + opt + "\n");
						m_opts.clustering.gammaRatioMax = val;
						iop += eval - bval;
					} else m_opts.clustering.gammaRatioMax = ClusterOptions::GAMMARATIOMAXDFL;
				}
				if(opt.length() <= iop)
					break;
				// =[<gamma_min>][:gamma_max]
				m_opts.clustering.gammaMin = m_opts.clustering.gamma = -1;  // Set automatic identification of the gamma range
				if(opt[iop] != '=' || opt.length() <= ++iop)
					throw invalid_argument("Unexpected option.g is provided: -" + opt + "\n");
				while(opt.length() > iop) {
					const bool  tail = opt[iop] == ':';
					if(tail && ++iop >= opt.length())
						break;
					bval = opt.c_str() + iop;
					auto val = strtof(bval, &eval);
					if(eval != bval) {
						if(val < 0)
							throw invalid_argument("Out of the range >= 0 option.gr value is provided: -" + opt + "\n");
						if(tail) {
							if(m_opts.clustering.gammaMin >= 0 && val >= 0 && val < m_opts.clustering.gammaMin)
								throw invalid_argument("Invalid range (gammaMin <= gamma), option.gr: -" + opt + "\n");
							m_opts.clustering.gamma = val;
						} else m_opts.clustering.gammaMin = val;
						iop += eval - bval;
					} else throw invalid_argument("Unexpected option.g is provided: -" + opt + "\n");
				}
			} break;
			default:
				throw invalid_argument("Unexpected option.g is provided: -" + opt + "\n");
			}
		} break;
		case 'b': {
			// -b[s][p]{u,d}][=<root_szmax>]
			if(opt.length() <= 1)
				throw invalid_argument("Invalid option.b is provided (without any parameters): -" + opt + "\n");
			uint8_t  iop = 0;
			while(++iop < opt.length()) {
				// Parse =VALUE
				if(opt[iop] == '=') {
					if(opt.length() <= 1u + iop)  // Note: 1 to consider '=': = + <N>
						throw invalid_argument("Invalid option.b is provided (without the value): -" + opt + "\n");
					auto val = strtoul(&opt.c_str()[++iop], nullptr, 10);
					// Note: >= because COUPLING_MAX is also reserved
					constexpr auto  vmax = numeric_limits<decltype(m_opts.clustering.rootMax)>::max();
					if(val >= vmax)
						throw out_of_range("The value is out of range: -" + opt
							+ " (does not belong to (0, " + std::to_string(vmax) + ")\n");
					// Note: vmax = 0 in case of invalid input, which deactivates the rootMax
					m_opts.clustering.rootMax = val;
					// Apply the default bounding policy (bound all) if not specified explicitly
					if((m_opts.clustering.rootBound & *RootBound::MASK_UPDOWN) == *RootBound::NONE)
						m_opts.clustering.rootBound |= RootBound::MASK_UPDOWN;
					break;
				}
				switch(opt[iop]) {
				case 's':
					// Parse BOUNDSTANDALONE (disconnected clusters)
					if(m_opts.clustering.rootBound & *RootBound::BOUNDSTANDALONE)
						throw invalid_argument("Invalid option.b is provided with the duplicated flag: -" + opt + "\n");
					m_opts.clustering.rootBound |= RootBound::BOUNDSTANDALONE;
					break;
				case  'p':
					// Parse NONEGATIVE (positive)
					if(m_opts.clustering.rootBound & *RootBound::NONEGATIVE)
						throw invalid_argument("Invalid option.b is provided with the duplicated flag: -" + opt + "\n");
					m_opts.clustering.rootBound |= RootBound::NONEGATIVE;
					break;
				case 'u':
				case 'd':
					// Parse UP/DOWN
					if(m_opts.clustering.rootBound & *RootBound::MASK_UPDOWN)
						throw invalid_argument("Invalid option.b is provided with"
							" either duplicated flag or exclusive flags: -" + opt + "\n");
					m_opts.clustering.rootBound |= opt[iop] == 'u' ? RootBound::UP : RootBound::DOWN;
					break;
				default:
					throw invalid_argument("Unexpected option.b is provided: -" + opt + "\n");
				}
			}
			if(m_opts.clustering.rootBound & *RootBound::NONEGATIVE
			&& (m_opts.clustering.rootBound & *RootBound::UP) == *RootBound::NONE)
				throw invalid_argument("Inconsistent options of the flag, "
					"'p' requires (possibly implicit) 'u' bound: -" + opt + "\n");
#if VALIDATE >= 2
			assert((!m_opts.clustering.rootMax || *m_opts.clustering.rootBound)
				&& "parseArgs(), rootMax specification requires rootBound initialization\n");
#endif // VALIDATE
		} break;
#if OPT_R_
		case 'r': {
			// -r[w][{a,m,s}]
			if(opt.length() <= 1) {
				m_opts.clustering.reduction = Reduction::MEAN;
				break;
			}
			uint8_t  iop = 1;
			if(opt[iop] == 'w') {
				m_opts.clustering.reduction |= Reduction::CRITERIA_WEIGHT;
				++iop;
				if(opt.length() <= iop)
					break;
			}
			switch(opt[iop]) {
			case 'a':
				m_opts.clustering.reduction = Reduction::ACCURATE;
				break;
			case 'm':
				m_opts.clustering.reduction = Reduction::MEAN;
				break;
			case 's':
				m_opts.clustering.reduction = Reduction::SEVERE;
				break;
			default:
				throw invalid_argument("Unexpected option.r is provided: -" + opt + "\n");
			}
		} break;
#endif // OPT_R_
		case 'l': {
			if(opt.length() <= 2 || opt[1] != '=')
				throw invalid_argument("Unexpected option.l is provided: -" + opt + "\n");
			const auto  valstr = &opt.c_str()[2];
			const auto  severity = atoi(valstr);
			switch(severity) {
				case 0:
					m_opts.clustering.validation = Validation::NONE;
					break;
				case 1:
					m_opts.clustering.validation = Validation::STANDARD;
					break;
				case 2:
					m_opts.clustering.validation = Validation::SEVERE;
					break;
				default:
					throw out_of_range("Provided node links severity is out of the expected range: "
						+ string(valstr) + "\n");
			}
		} break;
		case 'f':
			if(opt.length() <= 2 || opt[1] != '=')
				throw invalid_argument("Unexpected option.f is provided: -" + opt + "\n");
			m_opts.clustering.filterMarg = strtof(&opt.c_str()[2], nullptr);
			if(m_opts.clustering.filterMarg < 0 || m_opts.clustering.filterMarg > 1)
				throw out_of_range("The value is out of range: -" + opt + "\n");
			break;
		case 't':
			if(opt.length() > 1)
				throw invalid_argument("Unexpected option.t is provided: -" + opt + "\n");
			m_opts.timing.reset(new Timing());  // Start time measurement
			break;
		case 's':
			if(opt.length() > 1)
				throw invalid_argument("Unexpected option.s is provided: -" + opt + "\n");
			m_inpopts.shuffle = true;
			break;
		case 'x':
			if(opt.length() <= 1)
				throw invalid_argument("Unexpected option.x is provided: -" + opt + "\n");
			switch(opt[1]) {
			case 'a':
				m_opts.clustering.useAHash = false;
				break;
			default:
				throw invalid_argument("Unexpected option.x suboption is provided: -" + opt + "\n");
			}
			break;
		case 'm':
			// -m[s]=<gain_margmin>
			if(opt.length() <= 2 || (opt[1] != '='
			&& (opt.length() <= 3 || opt[1] != 's' || opt[2] != '=')))
				throw invalid_argument("Unexpected option.m is provided: -" + opt + "\n");
			m_opts.clustering.gainMargDiv = opt[1] == 's';
			m_opts.clustering.gainMarg = strtof(&opt.c_str()[m_opts.clustering.gainMargDiv ? 3 : 2], nullptr);
			if(m_opts.clustering.gainMarg < -0.5f || m_opts.clustering.gainMarg > 1)
				throw out_of_range("The value is out of range: -" + opt + "\n");
			break;
		case 'i':
			if(opt.length() > 1)
				throw invalid_argument("Unexpected option.i is provided: -" + opt + "\n");
			m_opts.clustering.modtrace = true;
			break;
		case 'n':
			if(opt.length() < 2 || opt.length() >= 3)
				throw invalid_argument("Unexpected option.n is provided: -" + opt + "\n");
			switch(opt[1]) {
			case 'r':
				m_inpopts.format = FileFormat::RCG;
				break;
			case 'e':
				m_inpopts.format = FileFormat::NSE;
				break;
			case 'a':
				m_inpopts.format = FileFormat::NSA;
			break;
			default:
				throw invalid_argument("Unexpected option.d1 is provided: -" + opt + "\n");
			}
			break;
		case 'h':
			if(opt.length() > 1)
				throw invalid_argument("Unexpected option.h is provided: -" + opt + "\n");
			// Just list the API
			break;
		default:
			throw invalid_argument("Unexpected option.d is provided: -" + opt + "\n");
		}
		if(errno) {
			perror(errmsg.insert(0, "Error on the arguments parsing. ").c_str());
			throw invalid_argument(string(strerror(errno)) += '\n');
		}
	}

	// Check files
	// Note: currently only the first file is accepted and processed
	if(m_evals && m_opts.outputs.front().clsfile.empty())
		throw invalid_argument("Evaluation file name is expected to be provided\n");
	// Note: only one input network at a time is supported currently
	if(files.size() == 1) {  // !files.empty()
		m_inpopts.filename = files.front();
		// Use the dynamic default output file for the clustering instead of the static one
		for(auto& outopt: m_opts.outputs) {
			if(outopt.clsfile == OutputOptions::CLSFILEDFLT)
				outopt.clsfile = outpFileName(outopt, m_inpopts.filename);
		}
	} else {
		if(files.size() >= 1) {
			fputs("-ERROR parseArgs(), only one input network at a time is supported currently: ", ftrace);
			for(auto& fname: files)
				fprintf(ftrace, " %s", fname.c_str());
			fputc('\n', ftrace);
		}
		return false;  // Show the version and exit
	}

	return true;
}

void Client::info(const char filename[]) const
{
	// Note: macros-dependent parameters are just hided if not supported, but can be supplied
	// not affecting anything to have unified API for the lib and simplified benchmarking
	if(!m_showver) {
		OutputOptions  outopts;  // Required to provide default values
		// Note: "=" is used as prefix in  parameter value definition to resolve ambiguity between
		// the default parameter value and non-keyword parameter (<input_network.rcg>).
		// Note: '!'<num> can't be used in bash without escaping, also as |, >, <.
		std::cout << "Usage:  " << filename <<
			"[-c[f][{o,x}]{r"
			",{s,S}[{s,a}{d,h}[%[b]{<dens_drop>,e,g}][/{<weight_rstep>,e,g}[~]]][_{<cl_szmin>,l2,le,pg,r<base>}]"
#if OPT_CX_
			",l[~<clsnum> | \\[[[%]#][<margmin>][:<margmax>][/<rstep>]\\]],a"
#endif // OPT_CX_
			"}[{p,s,h,e}]"
#if OPT_CX_
			"|h"
#endif // OPT_CX_
			"[=<filename>]"
#if OPT_E_
			" | -e{c,m,g}*=<filename>]"
#endif  // OPT_E_
			" [-a] [-g=<resolution> | -gr[<step_ratio>][:[<step_ratio_max>]][=[<gamma_min>][:gamma_max]]]"
			" [-b[s][p][{u,d}][=<root_szmax>]]"
#if OPT_R_
			" [-r[w][{a,m,s}]]"
#endif // OPT_R_
			" [-l=0..2]"
#ifndef NOPREFILTER
			" [-f=<filter_margin>]"
#endif // NOPREFILTER
			" [-t] [-s] [-x{a}] [-m[s]=<gain_margmin>]"
			" [-i] [-n{r,e,a}] <input_network> | -V[x] | [-h]\n"
			"\n"
			"Examples:\n  "
#if OPT_CX_
				<< filename << " -t -g=1 -ne -cxl[:/0.8]s=results/com-amazon.ungraph.cnl"
			" ../realnets/com-amazon.ungraph.txt\n  "
#endif // OPT_CX_
				<< filename << " -t"
#if OPT_R_
					" -r"
#endif // OPT_R_
					" -cxss tests/5K5.nse\n  "
				<< filename << " -t -rwm -bpeu=0 -cxsad%1.01/0.85_3s=tests/5K5_rw_bpeu0_sad.cnl networks/5K5.nse\n  "
				<< filename <<
#if OPT_R_
					" -r"
#endif // OPT_R_
					" -cSsd%b.5/0.618034s=tests/5K25_r_s.cnl networks/5K25.nse 2> tests/5K25_r_Sb.log\n  "
				<< filename << " -t -b=10 -cxssds=tests/1K5m/1K5_xssd.cnl -cxsad/0.1~_3s=tests/1K5m/1K5_xsad8-1-3.cnl"
#if OPT_CX_
				" -cxl[:/0.8]s=tests/1K5m/1K5_xl--8.cnl"
#endif // OPT_CX_
				" networks/1K5.nse 2> tests/1K5m/1K5_xsl.log\n  "
			"\n"
			"Limitations:\n"
			"  - weights accuracy: " << precision_limit<LinkWeight>()
				<< " (" << numeric_limits<LinkWeight>::digits10 << " decimal digits)\n"
			"  - max size of the input network: 2^32 (4 B) nodes\n"
			"\n"
			"Options:\n"
			"NOTE: sequence of the suboptions of each parameter is important.\n"
			"  -h  - help, show this API usage\n"
			"  -V[x]  - show version: <library>.<client>\n"
			"    x  - extended version including the clustering strategies:\n"
			"Library: <clustering_strategy>\nClient: <clustering_strategy>\n"
			"  -c[f][{o,x}]{r,{s,S}[{s,a}{d,h}[%[b]{<dens_drop>,e,g}][/{<weight_rstep>,e,g}[~]]][_{<cl_szmin>,l2,le,pg,r<base>}]"
#if OPT_CX_
			",l[~<clsnum> | \\[[[%]#][<margmin>][:<margmax>][/<rstep>]\\]],a"
#endif  // OPT_CX_
			"}[{p,s,h,e}]"
#if OPT_CX_
			"|h"
#endif  // OPT_CX_
			"[=<filename>]"
			"  - output the clustering (nodes membership) to the file"
			", the option can be specified multiple times to produce several outputs."
//			" For the {r,l,a} options"
//			" 'h' is implicitly included to additionally output the hierarchy into the specified"
//			" filename with the substituted '.rhb' extension."
			" The output levels are indexed from the bottom (the most fine-grained level) having index 0"
			" to the top (root, the most coarse-grained level) having the maximal index."
			" Default: omitted, format: e, filename (outputted to the input directory): "
				<< OutputOptions::CLSFILEDFLT << endl <<
			"    f  - filter out cluster members (nodes) having set the highest bit in the id from the"
			" resulting clusters. This feature is useful when the clustering should be performed for all"
			" input items (nodes or clusters) and then some of the items should be discarded from the formed clusters."
			" NOTE: affects only the cluster levels output (nodes membership), not the whole hierarchy output.\n"
			"    x  - output only max shares for the fuzzy-overlapping node\n"
			"    rX  - output only the root level clusters (similar to l[:<root_clsnum>]) to the <filename>"
			" creating the non-existing dirs\n"
			"    s[{s,a}{d,h}[%[b]{<dens_drop>,e,g}][/{<weight_rstep>,e,g}[~]]][_{<cl_szmin>,l2,le,pg,r<base>}]X  - output only significant"
			" (representative) clusters starting from the root and including all descendants that have"
			" higher density of the cluster structure than:\n"
			"      s  - single owner cluster (any one)\n"
			"      a  - all of owner clusters\n"  // ad means the least number of the purest clusters
			"      d  - direct owners only\n"
			"      h  - hierarchy of the representative owners\n"  // Note: sh is senseless as too weak
			"      Recommended: sd (good recall, fastest, default), ad (used in StaTIX, strictest)"
			", ah (good precision, recommended for the nodes vectorization)\n"
			"      %[b]{<dens_drop>,e,g}  - allowed density drop for a (possibly indirect) descendant of the outputting"
			" cluster, multiplier >= 0."
			"        b  - bottom bounding of the linear density drop relative to the top level"
			" from 1 on the top to <dens_drop> on the bottom level.\n"
			"        <dens_drop>  - a floating point value: [0, 1) means output subclusters of the lower density"
			" (cluster weight relative to the number of member nodes), (1, +inf) means"
			" output only more dense subclusters (normal), 1 - do not drop the density, 0 - output clusters having any density."
			" Recommended range: [0.95, 1.25] or b[0.5, 0.8] (typically used as 'sd%b0.5/0.6'"
			" or more strict 'sd%b.8/.5' for large networks with many small clusters).\n"
			"        e  - complement of the squared inverse exponent, typically used as 'sd%be/g'.\n"
			"        g  - inverse golden ratio.\n"
			"      /{<weight_rstep>,e,g}[~]  - weight step ratio to avoid output of the large clusters that"
			" differ only a bit in weight, multiplier, (0, 1]. 1 - output [descendant] clusters of any weight."
			" e - complement of the squared inverse exponent. g - inverse golden ratio."
			" '~' suffix means threat weight_rstep as range E [1-weight_rstep, weight_rstep], weight_rstep E (0.5, 1).\n"
			"      _{<cl_szmin>,l2,le,pg,r<base>}  - minimal number of nodes in the non-root outputting clusters (recommended: 3):"
			" <cl_szmin> - absolute number, l{2,e} - log with base '2' or 'e' of the number of nodes"
			" in the input network, pg - power of the inverse gold ratio, r<base> - root of the integer base (2 .. 8)"
			" of the number of nodes. Default: " << outopts.signifcls.szmin << endl <<
			"Recommended subflags are 'sa/0.9_3'\n"
			"    S...X  - output only significant (representative) clusters having a single owner cluster at most,"
			" otherwise the same as s...X option\n"
#if OPT_CX_
			"    l[~clsnum | \\[[[%]#][<margmin>][:<margmax>][/<rstep>]\\]]X"
			"  - output clusters from the hierarchy levels that satisfy the specified condition to the"
			" <filename_name>/<filename_name>_LevNum[.filename_ext] creating the non-existing dirs\n"
			"      ~<clsnum>  - have the number of clusters from the closest lower up to the"
			" closest higher than <clsnum> to the <filename_name>/<filename_name>_LevNum[.filename_ext]"
			" creating the non-existing dirs\n"
			"      \\[[[%]#][<margmin>][:<margmax>][/<rstep>]\\]"
			"  - have the number of clusters from <margmin> to <margmax> using <rstep>"
			" multiplication of the <clsnum> for each subsequent level, or the hierarchy level from"
			" #<margmin> to #<margmax> (starting from the bottom, node owners) and using <rstep>"
			" multiplication of the number of clusters on the bottom level to obtain minimal requirement for"
			" the following non-filtered out level. '%' means counting levels relative to the <rstep>."
			" Recommended values: 0.368f (e^-1), 0.618f (golden ratio), 0.85f (Pareto principle, 0.8-0.95f)."
			" For example:\n[:25/0.825]  - output cluster levels having <= 25 clusters on the level up"
			" to the root level, skipping the levels having more than <clsnum_prev> * 0.825 clusters.\n"
			"[%#3:/0.368]  - output cluster levels staring from one having <= 0.368^3 clusters of the number"
			" of clusters on the bottom level and up to the root level, skipping the levels having more than"
			" <clsnum_prev> * 0.368 clusters.\n"
			"      - output clusters from each hierarchy level when the condition is not specified\n"
			"    aX  - output all distinct clusters (once for all levels even if"
			" the cluster is propagated, flatter the hierarchy) to the <filename>,"
			" creating the non-existing parent dirs\n"
#endif // OPT_CX_
			"    Clusters output format:\n"
			"    Xp  - pure space separated (simple and without the header):  <node1> <node2> ...\n"
			"    Xs  - simple space separated: <node1> <node2> ...\n"
			"    Xh  - share listed (for unequal shares only):  <node1>[:<share1>] <node2>[:<share2>] ...\n"
			"    Xe  - extended (numbered by the cluster and listing the node share E(0, 1]):"
			"  <cluster1>> <node1>[:<share1>] <node2>[:<share2>] ...\n"
#if OPT_CX_
			"    h  - output all the hierarchy to the <filename_name> in the rhb (rcg-like) format"
			" starting from the nodes and bottom levels and listing the shares\n"
#endif // OPT_CX_
#if OPT_E_
			"  -e{c,m,g}*=<filename>  - evaluate intrinsic measure(s) for the specified nodes-clusters"
			" membership file without the clustering.\n"
			"Multiple suboptions can be specified: -emc."
			" Default: perform all evaluations\n"
			"    c  - conductance\n"
			"    m  - modularity (for the specified resolution parameter gamma)\n"
			"    g  - expected static resolution parameter gamma and additional modularity"  // Newman's gamma
			" on this resolution (besides the requested by the 'm' option)\n"
			"File format:\n"
			"[<cluster_id>>] <node1_id>[:<node1_share>] <node2_id>[:<node2_share> ...]\n"
			"NOTE:\n"
			"- line comments are allowed with '#': # This is a comment\n"
			"- cluster_id should be either specified and unique for each line"
			", or omitted in all lines\n"
			"- only the flat clusters are represented in this file, which usually"
			" corresponds to the root level of the hierarchy, overlaps are allowed\n"
			"- node shares are optional for the crisp overlaps (if the node is equally"
			" shared between all owner clusters)\n"
			"  -a  - accumulate weights of the duplicated links on graph construction"
			" (applicable only for the weighted graphs/networks), otherwise skip the duplicates\n"
			"  -g=<resolution> | -gr[<step_ratio>][:[<step_ratio_max>]][=[<gamma_min>][:gamma_max]]  - resolution parameter gamma\n"
			"    r[<step_ratio>][:[<step_ratio_max>]]  - gamma step ratio to construct the hierarchy using variable gamma, multiplier, (0, 1)."
			" If explicitly specified by ':', the step ratio is non-linearly increases to the <step_ratio_max>"
			" (corresponds to gamma precision, default: " << ClusterOptions::GAMMARATIOMAXDFL << ")"
			" near gamma = 1 and to decreases in other directions. Default: " << m_opts.clustering.gammaRatio << endl <<
			"    gamma | gamma_min, gamma_max  - minimal < maximal values of the resolution parameter gamma if the range is enabled,"
			" , float >= 0. The value omission triggers it's automatic identification considering the resolution limit"
			" of the input network.\n"
			" Otherwise, a fixed value of the resolution parameter gamma"
#ifdef DYNAMIC_GAMMA
			", a real number. Negative value means automatic evaluation of the dynamic gamma (1.5x slower but more accurate clustering)."
#else
			", float >= 0."
#endif // DYNAMIC_GAMMA
			" (0, 1) - micro clustering (a larger number of smaller clusters on the root level), > 1 - macro clustering."
			" Recommended: 0.1 .. 5. Default: " << m_opts.clustering.gamma << endl <<
#endif  // OPT_E_
			"  -b[s][p][{u,d}][=<root_szmax>]  - max size in clusters (bound) of the root level, 0 - unlimited (typically used as -bpu=0)"
			", recommended for the visualization: 5..10. Default: enforced root level shrinking in both up and down directions, root_szmax = 0."
			" Note: disables <gain_margmin> if required.\n"
			"    s  - shrink also stand-alone (disconnected) clusters in the root to a single or several heavy clusters.\n"
			"    p  - consider only positive (including +0) or also negative modularity gain for the root level reduction (shrinking up)."
			" Requires 'u' or <root_szmax> specification without 'b'.\n"
			"    Xu  - bound up enforcing the clustering till the root is shrinked up the specified bound.\n"
			"    Xd  - bound down terminating the clustering early if the root level reaches the specified bound.\n"
#if OPT_R_
			"  -r[w][{a,m,s}]  - reduce items by non significant links, speeding up the clustering almost not affecting the accuracy, default: m."
			" The clustering remains deterministic, and the weight of items is retained.\n"
			"    w  - perform reduction using direct weights instead of the optimization function.\n"
			"    Xa  - accurate reduction minimizing affect to the clustering accuracy."
			" Applicable to the heavy tailed weights distribution in large networks.\n"
			"    Xm  - mean reduction, a compromise between the links reduction severity and clustering accuracy.\n"
			"    Xs  - severe reduction to have maximal speedup and lowest memory consumption with minor drop"
			" of the clustering accuracy. Should not be used for the fine-grained clustering of the non large networks.\n"
#endif // OPT_R_
			"  -l=0..2  - node links validation severity, performed before the clustering"
			" (errors are shown and fixed, default: 1):\n"
			"    0  - skip links consistency validation, should be used only for the verified input data\n"
			"    1  - standard validation, in case of the object were constructed using APi\n"
			"    2  - severe validation, in case node links were manually edited (extended, reordered, etc.)\n"
#ifndef NOPREFILTER
			"  -f=<filter_margin>  - filtering margin of the clusterable items to skip the filtering"
			" if the number of clusterable items is small;"
			" float E [0, 1] (0 - always filter, 1 - discard the filtering). Default: "
				<< m_opts.clustering.filterMarg << endl <<
			"\nNote: Discarding the filtering slow downs the convergence time\n"
#endif // NOPREFILTER
			"  -t  - trace execution timings\n"
			"  -s  - shuffle (randomly reorder) nodes (hence, also links) on graph construction\n"
			"  -x{a}  - features to be disabled (excluded):\n"
			"    a  - AgordiHash application for the fast identification of the fully mutual mcands."
			" AgordiHash application is extremely useful for the semantic and other networks with lots of the"
			" fully mutual mcands, which often happens on conversion attributed networks to the pairwise relations\n"
			"  -m[s]=<gain_margmin>  - [modularity] gain min margin for the early exit in case the gain increases on"
			" less than this value, float E [-1/2, 1]. Applied only for the STATIC gamma and only if the maximal size"
			" of the root level is not enforced otherwise the gain margin automatically disabled if required."
			" Default: " << m_opts.clustering.gainMarg << ", but in practice ~>= 0.\n"
			" Negative value functionally the same as 0. Recommended: <= 1E-5 or 0\n"
			"    s  - divide the value by sqrt(numlinks), recommended: 0.01\n"
			// Note: -i is actual only for the RELEASE build, the tracing is always performed for the debug
			"  -i  - informative tracing, output optimization function (modularity) for each clustering iteration\n"
			"  -n{r,e,a}  - format of the input network (graph). Default: " << to_string(m_inpopts.format) << endl <<
			"    r  - readable compact graph (RCG), former hig\n"
			"    e  - network specified by edges (NSE), compatible with: ncol, Link List, [Weighted] Edge Graph and SNAP network formats\n"
			"    a  - network specified by arcs (NSA)\n"
			"  <input_network>  -  input network / graph (similarity / adjacency matrix) to be processed,"
			" specified in the .rcg (former .hig) or nsl format\n"
			"\n"
			"Rev: " << libBuild().rev() << "." << clientBuild().rev() <<
			" (" << to_string(libBuild().clustering) << ")\n";
	} else {
		if(m_showver == 1)
			printf("r-%s.%s (%s)\n", libBuild().rev().c_str(), clientBuild().rev().c_str()
				, to_string(libBuild().clustering).c_str());
		else printf("= Library Build =\n%s\n= Client Build =\n%s", libBuild().summary().c_str()
				, clientBuild().summary().c_str());
	}
}

void Client::execute()
{
	// Try to identify input format by the extension
	if(m_inpopts.format == FileFormat::UNKNOWN)
		m_inpopts.format = inpFileFmt(m_inpopts.filename.c_str());
	if(m_inpopts.format == FileFormat::UNKNOWN) {
		m_inpopts.format = FileFormat::DEFAULT_INPUT;
		fprintf(ftrace, "-WARNING execute(), input file format is not specified and"
			" can't be identified by the file extension, the default is used: %s\n"
			, to_string(m_inpopts.format).c_str());
	}

	switch(m_inpopts.format) {
	case FileFormat::RCG:
        execute<RcgParser>();
        break;
	case FileFormat::NSE:
	case FileFormat::NSA:
        execute<NslParser>();
        break;
	default:
		throw domain_error("Required parser have not been implemented yet, use .rcg format\n");
	}
}

template<typename ParserT>
void Client::execute()
{
	ParserT  parser(m_inpopts);

	// Note: nodes are reduced on clustering if required, not on the graph construction
	if(parser.weighted())
		process(*parser.template build<Graph<true>>());
	else process(*parser.template build<Graph<false>>());

	// Output execution timings
	if(m_opts.timing) {
		puts("-execute(), timings:");
		const auto& t = *m_opts.timing;
		if(t.loadnet)
			Timing::print(t.loadnet, "-  input network loading: ");
		if(t.loadcls)
			Timing::print(t.loadcls, "-  clusters loading: ");
		if(t.cluster)
			Timing::print(t.cluster, "-  clustering: ");
		if(t.evaluate)
			Timing::print(t.evaluate, "-  evaluation: ");
		if(t.outpfile)
			Timing::print(t.outpfile, "-  results serialization: ");
		if(t.outpterm)
			Timing::print(t.outpterm, "-  results output (terminal): ");
	}
}

template <bool WEIGHTED>
void Client::process(Graph<WEIGHTED>& graph)
{
	// Measure the network parsing time
	if(m_opts.timing)
		m_opts.timing->loadnet = m_opts.timing->update();
	// Perform clustering
	const bool  directed = graph.directed();
	// Update reduction clustering option if required
	if(graph.reduced())
		m_opts.clustering.reduction |= Reduction::SKIP_NODES;
#if TRACE >= 2
	printf("-process(), the input network is directed: %s, reduced: %s, shuffled: %s\n"
	, toYesNo(directed), toYesNo(graph.reduced()), toYesNo(m_inpopts.shuffle));
#endif // TRACE

	if(m_evals) {
		using LinksT = typename Graph<WEIGHTED>::LinksT;
		Clusters<LinksT>  cls;  // Loaded clusters
		// Load evaluating clusters
		const auto& outopt = m_opts.outputs.front();
		AccWeight  weight = loadClusters<CnlParser>(cls, graph
			, outopt.clsfile, m_opts.clustering.validation);
		// Measure the [ground-truth] clusters loading time
		if(m_opts.timing)
			m_opts.timing->loadcls = m_opts.timing->update();

		if(directed)
			intrinsicMeasures<true>(m_evals, cls, weight, m_opts.clustering.gamma);
		else intrinsicMeasures<false>(m_evals, cls, weight, m_opts.clustering.gamma);
		// Measure the evaluation time
		if(m_opts.timing)
			m_opts.timing->evaluate = m_opts.timing->update();

#if VALIDATE >= 2
		// Note: it's fine that for arbitrary cluster modularity can be negative, but it is always >= -1
#if TRACE >= 1
		// ATTENTION: reasonable precision is at most precision of the underlying items
		using  WeightT = typename LinksT::value_type::Weight;
		if(less<WeightT>(m_evals.mod))
			fprintf(ftrace, "WARNING process(), modularity is negative: %G\n", m_evals.mod);
#endif // TRACE
		assert(!less<WeightT>(m_evals.mod, -0.5) && !less<LinkWeight>(1, m_evals.mod)
			&& string("process(), modularity E [-0.5, 1]: mod = ").append(to_string(m_evals.mod)).c_str());
#endif // VALIDATE
		//fputs("-", stdout);
		bool initialized = false;
		if(m_evals.flags & IntrinsicsFlags::CONDUCTANCE) {
			printf("Conductance f: %G", m_evals.cdn);
			initialized = true;
		}
		if(m_evals.flags & IntrinsicsFlags::MODULARITY) {
			if(initialized)
				fputs(", ", stdout);
			printf("Q: %G on gamma=%G", m_evals.mod, m_opts.clustering.gamma);
			initialized = true;
		}
		if(m_evals.flags & IntrinsicsFlags::GAMMA) {
			if(initialized)
				fputs(", ", stdout);
			printf("Q*: %G on the expected static (Newman's) gamma=%G"
				, m_evals.sgmod, m_evals.gamma);  // Expected static Newman's gamma
		}
		printf(", clusters: %lu\n", cls.size());
	} else processNodes(*graph.release(), !directed, m_opts, m_showver);
	// ATTENTION: graph should NOT be finalized here to be able to get a node by id
}

// Build Information Definition -----------------------------------------------
static BuildInfo  buildInfo(buildinfo::revision, buildinfo::features, buildinfo::clustering, buildinfo::compiler
	, buildinfo::langenv, buildinfo::time);

const BuildInfo& clientBuild() { return buildInfo; }

#undef OPT_R_

#undef OPT_E_
#undef OPT_CX_
