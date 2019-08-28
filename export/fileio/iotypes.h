//! \brief Common IO types.
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

#ifndef IOTYPES_H
#define IOTYPES_H

#include <memory>  // unique_ptr, shared_ptr
#include <utility>  // move, forward
#include <string>  // to_string
#include <vector>
#include <fstream>

#include "macrodef.h"  // TRACE, VALIDATE, etc.


namespace daoc {

using std::string;
using std::vector;
//using std::unique_ptr;
//using std::make_unique;
using std::shared_ptr;
using std::make_shared;

//using pos_type = std::istream::pos_type;  // ifstream

//! Global trace log
//! \note Mainly stdout or stderr are assumed to used
extern FILE* ftrace;

// File Format related definitions ---------------------------------------------
//! Base type for FileFormat
using FileFormatBase = uint8_t;

enum class FileFormat: FileFormatBase {
	UNKNOWN = 0,  //!< Such file extension have not been registered
	// Input formats
	RCG,  //!< Readable Compact Graph format (former Hirecs Input Graph)
	//NSL,  //!< Network Specified by Links (Nodes Specifying Links)
	NSE,  //!< Network Specified by Edges (Nodes Specifying Edges)
	NSA,  //!< Network Specified by Arcs (Nodes Specifying Arcs)
	// Output Formats
	CNL,  //!< Cluster (community) Nodes List
	RHB,  //!< Readable Hierarchy from Bottom format, .rcg-like

	// Defaults
	DEFAULT_INPUT = RCG
};

//! Supported file extensions to be processed as the correspondent FileFormat
namespace FileExts {
	// Input formats
	constexpr char RCG[] = "rcg hig";
	constexpr char NSE[] = "nse nsl ncol ll";
	constexpr char NSA[] = "nsa";
	// Output formats
	constexpr char CNL[] = "cnl";
	constexpr char RHB[] = "rhb";
};

//! \brief Infer file format by the extension
//!
//! \param filename const char* - file name
//! \return FileFormat
FileFormat inpFileFmt(const char* filename);

#if defined(SWIG) && !defined(SWIG_OVERLOADS)
%rename(strFileFormat) to_string(FileFormat flag, bool bitstr=false);
#endif // SWIG

//! \brief Convert FileFormat to string
//! \relates FileFormat
//!
//! \param flag FileFormat - the flag to be converted
//! \param bitstr=false bool  - convert to bits string or to FileFormat captions
//! \return string  - resulting flag as a string
string to_string(FileFormat flag, bool bitstr=false);

//! Input Network (Graph) Options
struct InpOptions {
	FileFormat  format;  //! Input graph (network) format: RCG, NSL (nse, nsa)
	string  filename;  //! Evaluating input graph (network)
	bool  sumdups;  //! Accumulate weights of the duplicated links or skip them (applicable only for the weighted graph)
	bool  shuffle;  //! Shuffle (rand reorder) nodes and links

	// Note: FileFormat::UNKNOWN is used initially to try fetch the format from the file extension
	InpOptions(): format(FileFormat::UNKNOWN), filename(), sumdups(false), shuffle(false)
	{}
};

// Accessory File Processing types ---------------------------------------------
//! \brief Create specified directory if required
//!
//! \param dir const string&  - directory path to be created if required
//! \return void
void ensureDir(const string& dir);

//! \brief Wrapper around the FILE* to prevent hanging file descriptors
class FileWrapper {
    FILE*  dsc;
    bool  tidy;
public:
    //! \brief Constructor
    //! \param fd FILE*  - the file descriptor to be held
    //! \param cleanup=true bool  - close the file descriptor on destruction
    //! 	(typically false if stdin/out is supplied)
    FileWrapper(FILE* fd=nullptr, bool cleanup=true) noexcept
    : dsc(fd), tidy(cleanup)  {}

    //! \brief Copy constructor
    //! \note Any file descriptor should have a single owner
    FileWrapper(const FileWrapper&)=delete;

	//! \brief Move constructor
	// ATTENTION: fw.dsc is not set to nullptr by the default move operation
	// ATTENTION:  std::vector will move their elements if the elements' move constructor is noexcept, and copy otherwise (unless the copy constructor is not accessible)
    FileWrapper(FileWrapper&& fw) noexcept
    : FileWrapper(fw.dsc, fw.tidy)
    {
    	fw.dsc = nullptr;
    }

    //! \brief Copy assignment
    //! \note Any file descriptor should have the single owner
    FileWrapper& operator= (const FileWrapper&)=delete;

	//! \brief Move assignment
	// ATTENTION: fw.dsc is not set to nullptr by the default move operation
    FileWrapper& operator= (FileWrapper&& fw) noexcept
    {
    	reset(fw.dsc, fw.tidy);
    	fw.dsc = nullptr;
    	return *this;
    }

    //! \brief Destructor
    ~FileWrapper()  // noexcept by default
    {
        if(dsc && tidy) {
            fclose(dsc);
            dsc = nullptr;
        }
    }

#ifdef SWIG
	%rename(fdescr) operator FILE*() const noexcept;
#endif // SWIG
    //! \brief Implicit conversion to the file descriptor
    //!
    //! \return FILE*  - self as a file descriptor
    operator FILE*() const noexcept  { return dsc; }

    //! \brief Reset the wrapper
    //!
    //! \param fd FILE*  - the file descriptor to be held
    //! \param cleanup=true bool  - close the file descriptor on destruction
    //! 	(typically false if stdin/out is supplied)
    //! \return void
	void reset(FILE* fd=nullptr, bool cleanup=true) noexcept
	{
        if(dsc && tidy)
            fclose(dsc);
    	dsc = fd;
    	tidy = cleanup;
	}

    //! \brief Release ownership of the holding file
    //!
    //! \return FILE*  - file descriptor
    FILE* release() noexcept
    {
    	auto fd = dsc;
    	dsc = nullptr;
		return fd;
    }
};

using FileWrappers = vector<FileWrapper>;

}  // daoc

#endif // IOTYPES_H
