#pragma once

#include <queue>
#include <string>

#include "envoy/grpc/async_client.h"
#include "envoy/grpc/async_client_manager.h"
#include "envoy/stats/scope.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/common/request.h"
#include "nighthawk/common/request_stream_grpc_client.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/grpc/typed_async_client.h"
#include "external/envoy/source/common/http/header_map_impl.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif
#include "api/request_source/service.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Nighthawk {

class ProtoRequestHelper {
public:
  static RequestPtr
  messageToRequest(const Envoy::Http::RequestHeaderMap& base_header,
                   const nighthawk::request_source::RequestStreamResponse& message);
};

/**
 * gRPC client for communicating with a remote request-source service.
 */
class RequestStreamGrpcClientImpl
    : public RequestStreamGrpcClient,
      Envoy::Grpc::AsyncStreamCallbacks<nighthawk::request_source::RequestStreamResponse>,
      Envoy::Logger::Loggable<Envoy::Logger::Id::upstream> {
public:
  /**
   * @param async_client Raw async client that we can use.
   * @param dispatcher Dispatcher that will be used.
   * @param base_header Any headers in request specifiers yielded by the remote request
   * source service will override what is specified here.
   * @param header_buffer_length The number of messages to buffer.
   */
  RequestStreamGrpcClientImpl(Envoy::Grpc::RawAsyncClientPtr async_client,
                              Envoy::Event::Dispatcher& dispatcher,
                              const Envoy::Http::RequestHeaderMap& base_header,
                              const uint32_t header_buffer_length);

  // Grpc::AsyncStreamCallbacks
  void onCreateInitialMetadata(Envoy::Http::RequestHeaderMap& metadata) override;
  void onReceiveInitialMetadata(Envoy::Http::ResponseHeaderMapPtr&& metadata) override;
  void onReceiveMessage(
      std::unique_ptr<nighthawk::request_source::RequestStreamResponse>&& message) override;
  void onReceiveTrailingMetadata(Envoy::Http::ResponseTrailerMapPtr&& metadata) override;
  void onRemoteClose(Envoy::Grpc::Status::GrpcStatus status, const std::string& message) override;

  // RequestStreamGrpcClient
  RequestPtr maybeDequeue() override;
  void start() override;
  bool streamStatusKnown() const override {
    return stream_ == nullptr || total_messages_received_ > 0;
  }

private:
  static const std::string METHOD_NAME;
  void trySendRequest();
  Envoy::Grpc::AsyncClient<nighthawk::request_source::RequestStreamRequest,
                           nighthawk::request_source::RequestStreamResponse>
      async_client_;
  Envoy::Grpc::AsyncStream<nighthawk::request_source::RequestStreamRequest> stream_{};
  const Envoy::Protobuf::MethodDescriptor& service_method_;
  std::queue<std::unique_ptr<nighthawk::request_source::RequestStreamResponse>> messages_;
  void emplaceMessage(std::unique_ptr<nighthawk::request_source::RequestStreamResponse>&& message);
  uint32_t in_flight_headers_{0};
  uint32_t total_messages_received_{0};
  const Envoy::Http::RequestHeaderMap& base_header_;
  const uint32_t header_buffer_length_;
};

} // namespace Nighthawk
