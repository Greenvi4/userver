/* THIS FILE IS AUTOGENERATED, DON'T EDIT! */
#pragma once

#include <userver/components/component_base.hpp>

#include <client/test/client_impl.hpp>

namespace clients::test {

class Component final : public USERVER_NAMESPACE::components::ComponentBase {
public:
    static constexpr std::string_view kName = "test-client";

    Component(
        const USERVER_NAMESPACE::components::ComponentConfig& config,
        const USERVER_NAMESPACE::components::ComponentContext& context
    );

    Client& GetClient();

private:
    ClientImpl client_;
};

}  // namespace clients::test

template <>
inline constexpr auto USERVER_NAMESPACE::components::kConfigFileMode<::clients::test::Component> =
    USERVER_NAMESPACE::components::ConfigFileMode::kNotRequired;
