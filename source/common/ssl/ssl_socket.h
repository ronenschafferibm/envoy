#pragma once

#include <cstdint>
#include <string>

#include "envoy/network/connection.h"
#include "envoy/network/transport_socket.h"
#include "envoy/stats/scope.h"

#include "common/common/logger.h"
#include "common/ssl/context_impl.h"

#include "openssl/ssl.h"

namespace Envoy {
namespace Ssl {

enum class InitialState { Client, Server };

class SslSocket : public Network::TransportSocket,
                  public Connection,
                  protected Logger::Loggable<Logger::Id::connection> {
public:
  SslSocket(ContextSharedPtr ctx, InitialState state);

  // Ssl::Connection
  bool peerCertificatePresented() const override;
  std::string uriSanLocalCertificate() const override;
  const std::string& sha256PeerCertificateDigest() const override;
  std::string serialNumberPeerCertificate() const override;
  std::string subjectPeerCertificate() const override;
  std::string subjectLocalCertificate() const override;
  std::string uriSanPeerCertificate() const override;
  const std::string& urlEncodedPemEncodedPeerCertificate() const override;
  std::vector<std::string> dnsSansPeerCertificate() const override;
  std::vector<std::string> dnsSansLocalCertificate() const override;

  // Network::TransportSocket
  void setTransportSocketCallbacks(Network::TransportSocketCallbacks& callbacks) override;
  std::string protocol() const override;
  bool canFlushClose() override { return handshake_complete_; }
  void closeSocket(Network::ConnectionEvent close_type) override;
  Network::IoResult doRead(Buffer::Instance& read_buffer) override;
  Network::IoResult doWrite(Buffer::Instance& write_buffer, bool end_stream) override;
  void onConnected() override;
  const Ssl::Connection* ssl() const override { return this; }

  SSL* rawSslForTest() const { return ssl_.get(); }

private:
  Network::PostIoAction doHandshake();
  void drainErrorQueue();
  void shutdownSsl();

  // TODO: Move helper functions to the `Ssl::Utility` namespace.
  std::string getUriSanFromCertificate(X509* cert) const;
  std::string getSubjectFromCertificate(X509* cert) const;
  std::vector<std::string> getDnsSansFromCertificate(X509* cert) const;

  Network::TransportSocketCallbacks* callbacks_{};
  ContextImplSharedPtr ctx_;
  bssl::UniquePtr<SSL> ssl_;
  bool handshake_complete_{};
  bool shutdown_sent_{};
  uint64_t bytes_to_retry_{};
  mutable std::string cached_sha_256_peer_certificate_digest_;
  mutable std::string cached_url_encoded_pem_encoded_peer_certificate_;
};

class ClientSslSocketFactory : public Network::TransportSocketFactory {
public:
  ClientSslSocketFactory(ClientContextConfigPtr config, Ssl::ContextManager& manager,
                         Stats::Scope& stats_scope);

  Network::TransportSocketPtr createTransportSocket() const override;
  bool implementsSecureTransport() const override;

private:
  Ssl::ContextManager& manager_;
  Stats::Scope& stats_scope_;
  ClientContextConfigPtr config_;
  ClientContextSharedPtr ssl_ctx_;
};

class ServerSslSocketFactory : public Network::TransportSocketFactory {
public:
  ServerSslSocketFactory(ServerContextConfigPtr config, Ssl::ContextManager& manager,
                         Stats::Scope& stats_scope, const std::vector<std::string>& server_names);

  Network::TransportSocketPtr createTransportSocket() const override;
  bool implementsSecureTransport() const override;

private:
  Ssl::ContextManager& manager_;
  Stats::Scope& stats_scope_;
  ServerContextConfigPtr config_;
  const std::vector<std::string> server_names_;
  ServerContextSharedPtr ssl_ctx_;
};

} // namespace Ssl
} // namespace Envoy
