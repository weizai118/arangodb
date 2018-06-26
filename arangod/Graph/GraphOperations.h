////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz & Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_GRAPHOPERATIONS_H
#define ARANGOD_GRAPH_GRAPHOPERATIONS_H

#include <velocypack/Buffer.h>
#include <chrono>
#include <utility>

#include "Aql/Query.h"
#include "Aql/VariableGenerator.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ResultT.h"
#include "Graph/Graph.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationResult.h"

namespace arangodb {
namespace graph {

class GraphOperations {
 private:
  Graph const& _graph;
  std::shared_ptr<transaction::Context> _ctx;

  Graph const& graph() const { return _graph; };
  std::shared_ptr<transaction::Context>& ctx() { return _ctx; };

  /// @brief edge definitions of this graph
  std::unordered_map<std::string, EdgeDefinition> _edgeDefs;

 public:
  GraphOperations() = delete;
  GraphOperations(Graph const& graph_,
                  std::shared_ptr<transaction::Context> ctx_)
      : _graph(graph_), _ctx(std::move(ctx_)) {}

  // TODO I added the complex result type for the get* methods to exactly
  // reproduce (in the RestGraphHandler) the behaviour of the similar methods
  // in the RestDocumentHandler. A simpler type, e.g. ResultT<OperationResult>,
  // would be preferable.
  /// @brief Graph collection name
  static std::string const _graphs;

  /// @brief Graph collection edge definition attribute name
  static char const* _attrEdgeDefs;

  /// @brief Graph collection orphan list arribute name
  static char const* _attrOrphans;

  /// @brief Get a single vertex document from collection, optionally check rev
  /// The return value is as follows:
  /// If trx.begin fails, the outer ResultT will contain this error Result.
  /// Otherwise, the results of both (trx.document(), trx.finish()) are
  /// returned as a pair.
  /// This is because in case of a precondition error during trx.document(),
  /// the OperationResult may still be needed.
  ResultT<std::pair<OperationResult, Result>> getVertex(
      std::string const& collectionName, std::string const& key,
      boost::optional<TRI_voc_rid_t> rev);

  /// @brief Get a single edge document from definitionName.
  /// Similar to getVertex().
  ResultT<std::pair<OperationResult, Result>> getEdge(
      const std::string& definitionName, const std::string& key,
      boost::optional<TRI_voc_rid_t> rev);

  /// @brief Remove a single edge document from definitionName.
  ResultT<std::pair<OperationResult, Result>> removeEdge(
      const std::string& definitionName, const std::string& key,
      boost::optional<TRI_voc_rid_t> rev, bool waitForSync, bool returnOld);

  /// @brief Remove a vertex and all incident edges in the graph
  ResultT<std::pair<OperationResult, Result>> removeVertex(
      const std::string& collectionName, const std::string& key,
      boost::optional<TRI_voc_rid_t> rev, bool waitForSync, bool returnOld);

  /// @brief Remove a graph and optional all connected collections
  ResultT<std::pair<OperationResult, Result>> removeGraph(bool waitForSync,
                                                          bool dropCollections);

  ResultT<std::pair<OperationResult, Result>> updateEdge(
      const std::string& definitionName, const std::string& key,
      VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
      bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> replaceEdge(
      const std::string& definitionName, const std::string& key,
      VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
      bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> createEdge(
      const std::string& definitionName, VPackSlice document, bool waitForSync,
      bool returnNew);

  ResultT<std::pair<OperationResult, Result>> updateVertex(
      const std::string& collectionName, const std::string& key,
      VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
      bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> replaceVertex(
      const std::string& collectionName, const std::string& key,
      VPackSlice document, boost::optional<TRI_voc_rid_t> rev, bool waitForSync,
      bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> createVertex(
      const std::string& collectionName, VPackSlice document, bool waitForSync,
      bool returnNew);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add an orphan to collection to an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> addOrphanCollection(
      VPackSlice document, bool waitForSync);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an orphan collection from an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> eraseOrphanCollection(
      bool waitForSync, std::string collectionName, bool dropCollection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a new edge definition in an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> addEdgeDefinition(
      VPackSlice edgeDefinition, bool waitForSync);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an edge definition from an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> eraseEdgeDefinition(
      bool waitForSync, std::string edgeDefinitionName, bool dropCollection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create edge definition in an existing graph
  ////////////////////////////////////////////////////////////////////////////////
  ResultT<std::pair<OperationResult, Result>> editEdgeDefinition(
      VPackSlice edgeDefinition, bool waitForSync,
      const std::string& edgeDefinitionName);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief change the edge definition for a specified graph
  ////////////////////////////////////////////////////////////////////////////////
  OperationResult changeEdgeDefinitionsForGraph(
      VPackSlice graph, VPackSlice edgeDefinition,
      std::unordered_set<std::string> possibleOrphans, bool waitForSync,
      transaction::Methods& trx);

  void pushCollectionIfMayBeDropped(const std::string& colName,
                                    const std::string& graphName,
                                    std::vector<std::string>& toBeRemoved);

 private:
  using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

  ResultT<std::pair<OperationResult, Result>> getDocument(
      std::string const& collectionName, const std::string& key,
      boost::optional<TRI_voc_rid_t> rev);

  /// @brief creates a vpack { _key: key } or { _key: key, _rev: rev }
  /// (depending on whether rev is set)
  VPackBufferPtr _getSearchSlice(const std::string& key,
                                 boost::optional<TRI_voc_rid_t>& rev) const;

  ResultT<std::pair<OperationResult, Result>> modifyDocument(
      const std::string& collectionName, const std::string& key,
      VPackSlice document, bool isPatch, boost::optional<TRI_voc_rid_t> rev,
      bool waitForSync, bool returnOld, bool returnNew, bool keepNull);

  ResultT<std::pair<OperationResult, Result>> createDocument(
      transaction::Methods* trx, const std::string& collectionName,
      VPackSlice document, bool waitForSync, bool returnNew);

  void assertEdgeCollectionAvailability(std::string edgeDefinitionName);
  void assertVertexCollectionAvailability(std::string VertexDefinitionName);
};
}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_GRAPHOPERATIONS_H
