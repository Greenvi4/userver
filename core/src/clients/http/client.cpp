#include <userver/clients/http/client.hpp>

#include <chrono>
#include <cstdlib>
#include <limits>

#include <moodycamel/concurrentqueue.h>

#include <userver/clients/http/destination_statistics.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/async.hpp>

#include <clients/http/easy_wrapper.hpp>
#include <clients/http/testsuite.hpp>
#include <crypto/openssl.hpp>
#include <curl-ev/multi.hpp>
#include <curl-ev/ratelimit.hpp>
#include <engine/ev/thread_pool.hpp>
#include <userver/clients/http/config.hpp>
#include <userver/utils/userver_info.hpp>

namespace clients::http {
namespace {

const std::string kIoThreadName = "curl";
const auto kEasyReinitPeriod = std::chrono::minutes{1};

// cURL accepts options as long, but we use size_t to avoid writing checks.
// Clamp too high values to LONG_MAX, it shouldn't matter for these magnitudes.
long ClampToLong(size_t value) {
  return std::min<size_t>(value, std::numeric_limits<long>::max());
}

}  // namespace

Client::Client(const std::string& thread_name_prefix, size_t io_threads,
               engine::TaskProcessor& fs_task_processor)
    : destination_statistics_(std::make_shared<DestinationStatistics>()),
      statistics_(io_threads),
      idle_queue_(),
      fs_task_processor_(fs_task_processor),
      user_agent_(utils::GetUserverIdentifier()),
      proxy_(),
      connect_rate_limiter_(std::make_shared<curl::ConnectRateLimiter>()) {
  engine::ev::ThreadPoolConfig ev_config;
  ev_config.threads = io_threads;
  ev_config.thread_name =
      kIoThreadName +
      (thread_name_prefix.empty() ? "" : ("-" + thread_name_prefix));
  thread_pool_ = std::make_unique<engine::ev::ThreadPool>(std::move(ev_config));

  ReinitEasy();

  multis_.reserve(io_threads);
  for (auto thread_control_ptr : thread_pool_->NextThreads(io_threads)) {
    multis_.push_back(std::make_unique<curl::multi>(*thread_control_ptr,
                                                    connect_rate_limiter_));
  }

  easy_reinit_task_.Start(
      "http_easy_reinit",
      utils::PeriodicTask::Settings(kEasyReinitPeriod,
                                    {utils::PeriodicTask::Flags::kCritical}),
      [this] { ReinitEasy(); });

  SetConfig({});
}

Client::~Client() {
  easy_reinit_task_.Stop();

  // We have to destroy *this only when all the requests are finished, because
  // otherwise `multis_` and `thread_pool_` are destroyed and pending requests
  // cause UB (e.g. segfault).
  //
  // We can not refcount *this in `EasyWrapper` because that leads to
  // destruction of `thread_pool_` in the thread of the thread pool (that's an
  // UB).
  //
  // Best solution so far: track the pending tasks and wait for them to drop to
  // zero.
  while (pending_tasks_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  while (TryDequeueIdle())
    ;

  multis_.clear();
  thread_pool_.reset();
}

std::shared_ptr<Request> Client::CreateRequest() {
  std::shared_ptr<Request> request;

  auto easy = TryDequeueIdle();
  if (easy) {
    auto idx = FindMultiIndex(easy->GetMulti());
    auto wrapper = std::make_shared<impl::EasyWrapper>(std::move(easy), *this);
    request = std::make_shared<Request>(std::move(wrapper),
                                        statistics_[idx].CreateRequestStats(),
                                        destination_statistics_);
  } else {
    thread_local unsigned int rand_state = 0;
    auto i = rand_r(&rand_state) % multis_.size();
    auto& multi = multis_[i];

    auto wrapper = std::make_shared<impl::EasyWrapper>(
        easy_.Get()->GetBound(*multi), *this);
    request = std::make_shared<Request>(std::move(wrapper),
                                        statistics_[i].CreateRequestStats(),
                                        destination_statistics_);
  }

  if (testsuite_config_) {
    request->SetTestsuiteConfig(testsuite_config_);
  }

  if (user_agent_) {
    request->user_agent(*user_agent_);
  }

  {
    // Even if proxy is an empty string we should set it, because empty proxy
    // for CURL disables the use of *_proxy env variables.
    auto proxy_value = proxy_.Read();
    request->proxy(*proxy_value);
  }
  request->SetEnforceTaskDeadline(enforce_task_deadline_.ReadCopy());

  return request;
}

void Client::SetMultiplexingEnabled(bool enabled) {
  for (auto& multi : multis_) {
    multi->SetMultiplexingEnabled(enabled);
  }
}

void Client::SetMaxHostConnections(size_t max_host_connections) {
  for (auto& multi : multis_) {
    multi->SetMaxHostConnections(ClampToLong(max_host_connections));
  }
}

std::string Client::GetProxy() const { return proxy_.ReadCopy(); }

void Client::ReinitEasy() {
  easy_.Set(utils::CriticalAsync(fs_task_processor_, "http_easy_reinit",
                                 &curl::easy::Create)
                .Get());
}

InstanceStatistics Client::GetMultiStatistics(size_t n) const {
  UASSERT(n < statistics_.size());
  InstanceStatistics s(statistics_[n]);

  /* Update statistics from multi in place */
  const auto& multi_stats = multis_[n]->Statistics();

  /* There is a race between close/open updates, so at least make open>=close
   * to observe non-negative current socket count. */
  s.multi.socket_close = multi_stats.close_socket_total();
  s.multi.socket_open = multi_stats.open_socket_total();
  s.multi.current_load = multi_stats.get_busy_storage().GetCurrentLoad();
  s.multi.socket_ratelimit = multi_stats.socket_ratelimited_total();
  return s;
}

size_t Client::FindMultiIndex(const curl::multi* multi) const {
  for (size_t i = 0; i < multis_.size(); i++) {
    if (multis_[i].get() == multi) return i;
  }
  UASSERT_MSG(false, "Unknown multi");
  throw std::logic_error("Unknown multi");
}

PoolStatistics Client::GetPoolStatistics() const {
  PoolStatistics stats;
  stats.multi.reserve(multis_.size());
  for (size_t i = 0; i < multis_.size(); i++) {
    stats.multi.push_back(GetMultiStatistics(i));
  };
  return stats;
}

void Client::SetDestinationMetricsAutoMaxSize(size_t max_size) {
  destination_statistics_->SetAutoMaxSize(max_size);
}

const DestinationStatistics& Client::GetDestinationStatistics() const {
  return *destination_statistics_;
}

void Client::PushIdleEasy(std::shared_ptr<curl::easy>&& easy) noexcept {
  try {
    easy->reset();
    idle_queue_->enqueue(std::move(easy));
  } catch (const std::exception& e) {
    LOG_ERROR() << e;
  }

  DecPending();
}

std::shared_ptr<curl::easy> Client::TryDequeueIdle() noexcept {
  std::shared_ptr<curl::easy> result;
  if (!idle_queue_->try_dequeue(result)) {
    return {};
  }
  return result;
}

void Client::SetTestsuiteConfig(const TestsuiteConfig& config) {
  LOG_INFO() << "http client: configured for testsuite";
  testsuite_config_ = std::make_shared<const TestsuiteConfig>(config);
}

void Client::SetConfig(const Config& config) {
  const auto pool_size =
      ClampToLong(config.connection_pool_size / multis_.size());
  if (pool_size * multis_.size() != config.connection_pool_size) {
    LOG_DEBUG() << "SetConnectionPoolSize() rounded pool size for each multi ("
                << config.connection_pool_size << "/" << multis_.size()
                << " rounded to " << pool_size << ")";
  }
  enforce_task_deadline_.Assign(config.enforce_task_deadline);
  for (auto& multi : multis_) {
    multi->SetConnectionCacheSize(pool_size);
  }

  connect_rate_limiter_->SetGlobalHttpLimits(config.http_connect_throttle_limit,
                                             config.http_connect_throttle_rate);
  connect_rate_limiter_->SetGlobalHttpsLimits(
      config.https_connect_throttle_limit, config.https_connect_throttle_rate);
  connect_rate_limiter_->SetPerHostLimits(
      config.per_host_connect_throttle_limit,
      config.per_host_connect_throttle_rate);

  proxy_.Assign(config.proxy);
}

void Client::ResetUserAgent(std::optional<std::string> user_agent) {
  user_agent_ = std::move(user_agent);
}

}  // namespace clients::http
