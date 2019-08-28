//! \brief Exporting interface of the whole library.
//! The Dao (Deterministic Agglomerative Overlapping) of Clustering library:
//! Robust & Fine-grained Deterministic Clustering for Large Networks.
//!
//! \license Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0.html
//! >	Simple explanation: https://tldrlegal.com/license/apache-license-2.0-(apache-2.0)
//!
//! Copyright (c)
//! \authr Artem Lutov
//! \email luart@ya.ru
//! \date 2014-09-08

#ifndef ALL_HPP
#define ALL_HPP

#include "types.h"  // Defines all main types. Includes flags.h
#include "functionality.h"  // Defines functional interface of the library
#include "processing.h"  // Defines accessory functions of the library
#include "graph.hpp"  // Note: Includes operations.hpp
// Note: types.hpp includes processing.hpp that includes functionality.h

#endif // ALL_HPP
