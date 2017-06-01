/**
 * Copyright 2017 Soramitsu Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <grpc++/grpc++.h>
#include <consensus/block_builder.hpp> // sumeragi::Block
#include <infra/config/iroha_config_with_json.hpp> // createChannel()
#include <membership_service/peer_service.hpp> // ::peer::service::getActivePeerList()
#include <endpoint_generated.h>
#include <endpoint.grpc.fb.h> // protocol::Sumeragi

namespace connection {

enum class ResponseType {
  RESPONSE_OK,
  RESPONSE_INVALID_SIG,  // wrong signature
  RESPONSE_ERRCONN,      // connection error
};

/**
 * helper function to create channel
 */
static std::shared_ptr<grpc::Channel> createChannel(const std::string& serverIp/*, GrpcPortType portType*/) {
  return grpc::CreateChannel(
    serverIp + ":" +
    std::to_string(config::IrohaConfigManager::getInstance()
                     .getGrpcPortNumber(50051)),
    grpc::InsecureChannelCredentials()
  );
}

/**
 * SumeragiServer
 * @brief RPC server for connecting with other sumeragi RPC server.
 */
class SumeragiServer {
  virtual grpc::Status Consensus(
    grpc::ClientContext* context,
    const flatbuffers::BufferRef<protocol::Block>& request,
    flatbuffers::BufferRef<protocol::SumeragiResponse>* response) {

    fbbResponse.Clear();

    // UnPack to sumeragi::Block
    

    // Dispatch to stateless validator

    // Returns validation

    return grpc::Status::OK;
  }

private:
  flatbuffers::FlatBufferBuilder fbbResponse;
};

/**
 * SumeragiClient
 * @brief RPC client for connecting with other sumeragi RPC server.
 */
class SumeragiClient {
public:
  explicit SumeragiClient(const std::string& serverIp)
    : stub_(protocol::Sumeragi::NewStub(createChannel(serverIp))) {}

  /**
   * Consensus
   * @detail sumeragi -> interface -> [RPC Client] -> RPC Server -> other sumeragi
   *
   * @param block_ - block to consensus
   * @param response - to return SumeragiServer response
   *
   * @return grpc::Status
   */
  grpc::Status Consensus(const sumeragi::Block &block,
                         flatbuffers::BufferRef<protocol::SumeragiResponse> *response) const {
    grpc::ClientContext context;
    flatbuffers::FlatBufferBuilder fbb;
    auto block_o = block.packOffset(fbb);
    flatbuffers::BufferRef<protocol::Block> request(
      fbb.GetBufferPointer(), fbb.GetSize()
    );
    return stub_->Consensus(&context, request, response);
  }

private:
  std::unique_ptr<protocol::Sumeragi::Stub> stub_;
};

namespace with_sumeragi {

/**
 * unicasts block to a validating peer.
 * @param block - block to consensus.
 * @param index - validating peer's index.
 */
void unicast(const sumeragi::Block& block, ::peer::Nodes::const_iterator iter) {

  if (::peer::service::isExistIP((*iter)->ip)) {
    SumeragiClient client((*iter)->ip);

    flatbuffers::BufferRef<protocol::SumeragiResponse> response;
    auto stat = client.Consensus(block, &response);

    // current implementation doesn't require response.

    if (!stat.ok()) {
      std::cout << "{error_code: " << stat.error_code() << ", "
                << "error_message: " << stat.error_message() << ", "
                << "error_details: " << stat.error_details() << "}\n";
    }
  } else {
    std::cout << "IP: " << (*iter)->ip << " doesn't exist." << std::endl;
  }
}

/**
 * multicasts block to [beginIndex, endIndex) peers.
 * @param block - block to consensus.
 * @param begin - validating peer's iterator except leader (usually next to begin())
 * @param end - validating peer's tail + 1 iterator
 */
void multicast(const sumeragi::Block& block,
               ::peer::Nodes::const_iterator begin,
               ::peer::Nodes::const_iterator end) {
  for (auto iter = begin; iter != end; iter++) {
    unicast(block, iter);
  }
}

/**
 * commits block to all peers except sender
 */
void commit(const sumeragi::Block& block, ::peer::Nodes::const_iterator sender) {
  auto peers = ::peer::service::getActivePeerList();
  for (auto iter = peers.begin(); iter != peers.end(); iter++) {
    if (sender != iter) {
      unicast(block, iter);
    }
  }
}

} // namespace with_sumeragi

class PeerServiceClient {
public:
  PeerServiceClient(const std::string& serverIp)
    : stub_(protocol::Sumeragi::NewStub(createChannel(serverIp))) {}

  /**
   * Torii
   * @brief Cient for sending tx to sumeragi. This method is for peer service.
   * @details peer service -> interface -> [RPC Cient] -> RPC Server -> other sumeragi
   *
   * @param tx [description]
   * @param responseRef [description]
   *
   * @return grpc::Status
   */
  grpc::Status Torii(const flatbuffers::BufferRef<protocol::Transaction> &request,
                     flatbuffers::BufferRef<protocol::SumeragiResponse> *response) const {
    grpc::ClientContext context;
    return stub_->Torii(&context, request, response);
  }

private:
  std::unique_ptr<protocol::Sumeragi::Stub> stub_;
};


} // namespace connection
