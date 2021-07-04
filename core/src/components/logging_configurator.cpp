#include <tracing/no_log_spans.hpp>
#include <userver/components/logging_configurator.hpp>
#include <userver/taxi_config/storage/component.hpp>
#include <userver/tracing/tracer.hpp>

#include <logging/rate_limit.hpp>

namespace components {

namespace {

/// [LoggingConfigurator config key]
tracing::NoLogSpans ParseNoLogSpans(const taxi_config::DocsMap& docs_map) {
  return docs_map.Get("USERVER_NO_LOG_SPANS").As<tracing::NoLogSpans>();
}

constexpr taxi_config::Key<ParseNoLogSpans> kNoLogSpans{};
/// [LoggingConfigurator config key]

}  // namespace

LoggingConfigurator::LoggingConfigurator(const ComponentConfig& config,
                                         const ComponentContext& context) {
  logging::impl::SetLogLimitedEnable(
      config["limited-logging-enable"].As<bool>());
  logging::impl::SetLogLimitedInterval(
      config["limited-logging-interval"].As<std::chrono::milliseconds>());

  config_subscription_ =
      context.FindComponent<components::TaxiConfig>()
          .GetSource()
          .UpdateAndListen(this, kName, &LoggingConfigurator::OnConfigUpdate);
}

void LoggingConfigurator::OnConfigUpdate(const taxi_config::Snapshot& config) {
  (void)this;  // silence clang-tidy
  tracing::Tracer::SetNoLogSpans(tracing::NoLogSpans{config[kNoLogSpans]});
}

}  // namespace components
