# DAOC - Deterministic and Agglomerative Overlapping Clustering algorithm

`\authors` (c) Artem Lutov <artem@exascale.info>  
`\license` [Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0) with the exception of several files having [MPL 2.0](https://www.mozilla.org/en-US/MPL/2.0/) license specified in their headers, optional commercial support and relicensing is provided by the request  
`\organizations` [eXascale Infolab](http://exascale.info/), [Lumais](http://www.lumais.com/)  
`\keywords` stable clustering, overlapping clustering, com-
munity structure discovery, parameter-free community detection,
cluster analysis

The paper:
```
@inproceedings{Daoc19,
	author={Artem Lutov and Mourad Khayati and Philippe Cudr{\'e}-Mauroux},
	title={DAOC: Stable Clustering of Large Networks},
	year={2019},
	keywords={stable clustering, overlapping clustering, community structure discovery, parameter-free community detection, cluster analysis},
}
```

The source code is being prepared for the publication and cross-platform deployment, and will be fully uploaded soon...  
Meanwhile, please *write me to get the sources*.
The [DAOC binaries](https://github.com/eXascaleInfolab/clubmark/tree/master/algorithms/daoc) built on Linux Ubuntu 16.04+ x64 can be found in the [Clubmark](https://github.com/eXascaleInfolab/clubmark) benchmarking framework.


## Related Projects

- [resmerge](https://github.com/eXascaleInfolab/resmerge)  - Resolution levels clustering merger with filtering. Flattens hierarchy/list of multiple resolutions levels (clusterings) into the single flat clustering with clusters on various resolution levels synchronizing the node base.
- [DAOR](https://github.com/eXascaleInfolab/daor)  - Parameter-free Embedding Framework for Large Graphs (Networks) based on DAOC.
- [Clubmark](https://github.com/eXascaleInfolab/clubmark) - A parallel isolation framework for benchmarking and profiling clustering (community detection) algorithms considering overlaps (covers), includes a dozen of clustering algorithms for large networks.
- [ParallelComMetric](https://github.com/eXascaleInfolab/ParallelComMetric) - A parallel toolkit implemented with Pthreads (or MPI) to calculate various extrinsic and intrinsic quality metrics (with and without ground truth community structure) for non-overlapping (hard, single membership) clusterings.
- [CluSim](https://github.com/Hoosier-Clusters/clusim) - A Python module that evaluates (slowly) various extrinsic quality metrics (accuracy) for non-overlapping (hard, single membership) clusterings.
- [GraphEmbEval](https://github.com/eXascaleInfolab/GraphEmbEval) - Graph (Network) Embeddings Evaluation Framework via classification, which also provides gram martix construction for links prediction.
- [xmeasures](https://github.com/eXascaleInfolab/xmeasures)  - Extrinsic quality (accuracy) measures evaluation for the overlapping clustering on large datasets: family of mean F1-Score (including clusters labeling), Omega Index (fuzzy version of the Adjusted Rand Index) and standard NMI (for non-overlapping clusters).
- [GenConvNMI](https://github.com/eXascaleInfolab/GenConvNMI) - Overlapping NMI evaluation that is compatible with the original NMI and suitable for both overlapping and multi resolution (hierarchical) clustering evaluation.
- [OvpNMI](https://github.com/eXascaleInfolab/OvpNMI)  - NMI evaluation for the overlapping clusters (communities) that is not compatible with the standard NMI value unlike GenConvNMI, but it is much faster than GenConvNMI.
- [ExecTime](https://bitbucket.org/lumais/exectime/)  - A lightweight resource consumption profiler.

See also [eXascale Infolab](https://github.com/eXascaleInfolab) GitHub repository and [our website](http://exascale.info/) where you can find another projects and research papers related to Big Data processing!  

**Note:** Please, [star this project](https://github.com/eXascaleInfolab/daoc) if you use it.
