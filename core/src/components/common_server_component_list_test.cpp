#include <userver/components/common_server_component_list.hpp>

#include <userver/components/common_component_list.hpp>
#include <userver/components/run.hpp>
#include <userver/fs/blocking/temp_directory.hpp>  // for fs::blocking::TempDirectory
#include <userver/fs/blocking/write.hpp>  // for fs::blocking::RewriteFileContents
#include <userver/server/handlers/ping.hpp>

#include <components/component_list_test.hpp>
#include <utest/utest.hpp>

namespace {

const auto kTmpDir = fs::blocking::TempDirectory::Create();
const std::string kRuntimeConfingPath =
    kTmpDir.GetPath() + "/runtime_config.json";
const std::string kConfigVariablesPath =
    kTmpDir.GetPath() + "/config_vars.json";

const std::string kConfigVariables = fmt::format(
    R"(
  userver-dumps-root: {0}
  runtime_config_path: {1})",
    kTmpDir.GetPath(), kRuntimeConfingPath);

// BEWARE! No separate fs-task-processor. Testing almost single thread mode
const std::string kStaticConfig = R"(
components_manager:
  coro_pool:
    initial_size: 50
    max_size: 500
  default_task_processor: main-task-processor
  event_thread_pool:
    threads: 4
  task_processors:
    main-task-processor:
      thread_name: main-worker
      worker_threads: 4
    monitor-task-processor:
      thread_name: mon-worker
      worker_threads: 1
  components:
    manager-controller:  # Nothing
    logging:
      fs-task-processor: main-task-processor
      loggers:
        default:
          file_path: '@stderr'
    tracer:
        service-name: config-service
    statistics-storage:
      # Nothing
    taxi-config:
      fs-cache-path: $runtime_config_path
      fs-task-processor: main-task-processor
    server:
      listener:
          port: 8087
          task_processor: main-task-processor
      listener-monitor:
          connection:
              in_buffer_size: 32768
              requests_queue_size_threshold: 100
          port: 8088
          port#fallback: 1188
          task_processor: monitor-task-processor
    auth-checker-settings:
      # Nothing
    logging-configurator:
      limited-logging-enable: true
      limited-logging-interval: 1s
    dump-configurator:
      dump-root: $userver-dumps-root
    testsuite-support:
    http-client:
      pool-statistics-disable: true
      thread-name-prefix: http-client
      threads: 2
      fs-task-processor: main-task-processor
      destination-metrics-auto-max-size: 100
      user-agent: common_component_list sample
      testsuite-enabled: true
      testsuite-timeout: 5s
      testsuite-allowed-url-prefixes: ['http://localhost:8083/', 'http://localhost:8084/']
    taxi-configs-client:
      get-configs-overrides-for-service: true
      service-name: common_component_list-service
      http-timeout: 20s
      http-retries: 5
      config-url: http://localhost:8083/
      use-uconfigs: $use_uconfigs
      use-uconfigs#fallback: false
    taxi-config-client-updater:
      store-enabled: true
      load-only-my-values: true
      fallback-path: $runtime_config_path
      fallback-path#fallback: /some/path/to/runtime_config.json

      # options from components::CachingComponentBase
      update-types: full-and-incremental
      update-interval: 5s
      update-jitter: 2s
      full-update-interval: 5m
      first-update-fail-ok: true
      config-settings: true
      additional-cleanup-interval: 5m
      testsuite-force-periodic-update: true
    tracer:
        service-name: config-service
    statistics-storage:
      # Nothing
    http-client-statistics:
      fs-task-processor: main-task-processor
    system-statistics-collector:
      fs-task-processor: main-task-processor
      update-interval: 1m
      with-nginx: false
# /// [Sample tests control component config]
# yaml
    tests-control:
        skip-unregistered-testpoints: true
        testpoint-timeout: 10s
        testpoint-url: https://localhost:7891/testpoint

        # Some options from server::handlers::HttpHandlerBase
        path: /tests/control
        task_processor: main-task-processor
# /// [Sample tests control component config]
# /// [Sample congestion control component config]
# yaml
    congestion-control:
        fake-mode: true
        min-cpu: 2
        only-rtc: false

        # Common component options
        load-enabled: true
# /// [Sample congestion control component config]
# /// [Sample handler ping component config]
# yaml
    handler-ping:
        # Options from server::handlers::HandlerBase
        path: /ping
        task_processor: main-task-processor
        method: GET
        max_url_size: 128
        max_request_size: 256
        max_headers_size: 256
        parse_args_from_body: false
        # auth: TODO:
        url_trailing_slash: strict-match
        max_requests_in_flight: 100
        request_body_size_log_limit: 128
        response_data_size_log_limit: 128
        max_requests_per_second: 100
        decompress_request: false
        throttling_enabled: false
        set-response-server-hostname: false

        # Options from server::handlers::HttpHandlerBase
        log-level: WARNING

        # Common component options
        load-enabled: true
# /// [Sample handler ping component config]
# /// [Sample handler log level component config]
# yaml
    handler-log-level:
        method: GET,PUT
        path: /service/log-level/{level}
        task_processor: monitor-task-processor
# /// [Sample handler log level component config]
# /// [Sample handler inspect requests component config]
# yaml
    handler-inspect-requests:
        path: /service/inspect-requests
        task_processor: monitor-task-processor
# /// [Sample handler inspect requests component config]
# /// [Sample handler implicit http options component config]
# yaml
    handler-implicit-http-options:
        as_fallback: implicit-http-options
        task_processor: main-task-processor
# /// [Sample handler implicit http options component config]
# /// [Sample handler jemalloc component config]
# yaml
    handler-jemalloc:
        method: GET
        path: /service/jemalloc/prof/{command}
        task_processor: monitor-task-processor
# /// [Sample handler jemalloc component config]
# /// [Sample handler server monitor component config]
# yaml
    handler-server-monitor:
        path: /*
        task_processor: monitor-task-processor
        method: GET
# /// [Sample handler server monitor component config]
config_vars: )" + kConfigVariablesPath +
                                  R"(
)";

}  // namespace

TEST(ServerCommonComponentList, Base) {
  tests::LogLevelGuard logger_guard{};
  fs::blocking::RewriteFileContents(kRuntimeConfingPath, tests::kRuntimeConfig);
  fs::blocking::RewriteFileContents(kConfigVariablesPath, kConfigVariables);

  components::RunOnce(
      components::InMemoryConfig{kStaticConfig},
      components::CommonComponentList()
          .AppendComponentList(components::CommonServerComponentList())
          .Append<server::handlers::Ping>());
}
