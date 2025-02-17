#pragma once

#include <userver/utest/using_namespace_userver.hpp>

#include <userver/ugrpc/server/middlewares/base.hpp>

namespace sample::grpc::auth::server {

/// [gRPC middleware sample - Middleware component declaration]
class Component final : public ugrpc::server::MiddlewareFactoryComponentBase {
public:
    static constexpr std::string_view kName = "grpc-auth-server";

    Component(const components::ComponentConfig&, const components::ComponentContext&);

    std::shared_ptr<ugrpc::server::MiddlewareBase> CreateMiddleware(
        const ugrpc::server::ServiceInfo&,
        const yaml_config::YamlConfig& middleware_config
    ) const override;

private:
    std::shared_ptr<ugrpc::server::MiddlewareBase> middleware_;
};
/// [gRPC middleware sample - Middleware component declaration]

}  // namespace sample::grpc::auth::server
