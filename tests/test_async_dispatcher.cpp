#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <logspine/logspine.hpp>

namespace {

class counting_sink final : public logspine::sink {
 public:
  void write(const logspine::log_event&) override { ++writes; }
  void flush() override { ++flushes; }

  std::atomic<int> writes{0};
  std::atomic<int> flushes{0};
};

class throwing_sink final : public logspine::sink {
 public:
  void write(const logspine::log_event&) override { throw std::runtime_error("sink failure"); }
  void flush() override {}
};

class slow_sink final : public logspine::sink {
 public:
  void write(const logspine::log_event&) override {
    {
      std::scoped_lock lock(mutex);
      write_started = true;
    }
    started.notify_all();

    std::unique_lock lock(mutex);
    release.wait(lock, [this] { return can_continue; });
  }

  void flush() override {}

  std::mutex mutex;
  std::condition_variable started;
  std::condition_variable release;
  bool write_started = false;
  bool can_continue = false;
};

}  // namespace

TEST_CASE("async dispatcher flushes accepted events", "[async][flush]") {
  auto sink = std::make_shared<counting_sink>();
  logspine::async_dispatcher dispatcher(std::vector<std::shared_ptr<logspine::sink>>{sink},
                                        logspine::async_options{.queue_capacity = 8, .overflow = logspine::overflow_policy::drop_newest, .batch_size = 4});

  for (int index = 0; index < 4; ++index) {
    logspine::log_event event;
    event.logger_name = "async";
    event.message = "message";
    dispatcher.dispatch(std::move(event));
  }
  dispatcher.flush();
  REQUIRE(sink->writes.load() == 4);
  REQUIRE(sink->flushes.load() >= 1);
}

TEST_CASE("async dispatcher counts sink failures", "[async][errors]") {
  auto sink = std::make_shared<throwing_sink>();
  logspine::async_dispatcher dispatcher(std::vector<std::shared_ptr<logspine::sink>>{sink},
                                        logspine::async_options{.queue_capacity = 2, .overflow = logspine::overflow_policy::drop_newest, .batch_size = 1});
  logspine::log_event event;
  event.logger_name = "async";
  event.message = "throws";
  dispatcher.dispatch(std::move(event));
  dispatcher.flush();
  REQUIRE(dispatcher.sink_failures() >= 1U);
}

TEST_CASE("block overflow policy stalls producers until capacity returns", "[async][overflow]") {
  auto sink = std::make_shared<slow_sink>();
  logspine::async_dispatcher dispatcher(std::vector<std::shared_ptr<logspine::sink>>{sink},
                                        logspine::async_options{.queue_capacity = 1, .overflow = logspine::overflow_policy::block, .batch_size = 1});

  logspine::log_event first;
  first.logger_name = "async";
  first.message = "first";
  dispatcher.dispatch(std::move(first));

  {
    std::unique_lock lock(sink->mutex);
    sink->started.wait(lock, [&sink] { return sink->write_started; });
  }

  logspine::log_event second;
  second.logger_name = "async";
  second.message = "second";
  dispatcher.dispatch(std::move(second));

  logspine::log_event third;
  third.logger_name = "async";
  third.message = "third";
  dispatcher.dispatch(std::move(third));

  std::atomic<bool> fourth_finished = false;
  std::thread producer([&dispatcher, &fourth_finished] {
    logspine::log_event fourth;
    fourth.logger_name = "async";
    fourth.message = "fourth";
    dispatcher.dispatch(std::move(fourth));
    fourth_finished.store(true, std::memory_order_relaxed);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  REQUIRE_FALSE(fourth_finished.load(std::memory_order_relaxed));

  {
    std::scoped_lock lock(sink->mutex);
    sink->can_continue = true;
  }
  sink->release.notify_all();

  producer.join();
  dispatcher.flush();
  REQUIRE(fourth_finished.load(std::memory_order_relaxed));
}

TEST_CASE("drop_oldest overflow policy records dropped events", "[async][overflow]") {
  auto sink = std::make_shared<counting_sink>();
  logspine::async_dispatcher dispatcher(std::vector<std::shared_ptr<logspine::sink>>{sink},
                                        logspine::async_options{.queue_capacity = 1, .overflow = logspine::overflow_policy::drop_oldest, .batch_size = 1});
  for (int index = 0; index < 16; ++index) {
    logspine::log_event event;
    event.logger_name = "async";
    event.message = "drop-oldest";
    dispatcher.dispatch(std::move(event));
  }
  dispatcher.flush();
  REQUIRE(dispatcher.dropped_events() > 0U);
}
