#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>

#include "ChatApiService.h"
#include "DeliverApiClient.h"
#include "Logger.h"
#include "RedisMgr.h"
#include "im.grpc.pb.h"

namespace {

class ScopedChatApiServer {
public:
    ScopedChatApiServer() {
        auto deliver_client = std::make_shared<DeliverApiClient>();
        service_ = std::make_unique<ChatApiService>(deliver_client);

        grpc::ServerBuilder builder;
        builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port_);
        builder.RegisterService(service_.get());
        server_ = builder.BuildAndStart();
        if (!server_ || port_ <= 0) {
            throw std::runtime_error("failed to start ChatApi benchmark server");
        }
    }

    ~ScopedChatApiServer() {
        if (server_) {
            server_->Shutdown();
        }
    }

    std::unique_ptr<im::ChatApi::Stub> NewStub() const {
        auto channel = grpc::CreateChannel(
            "127.0.0.1:" + std::to_string(port_),
            grpc::InsecureChannelCredentials());
        return im::ChatApi::NewStub(channel);
    }

private:
    int port_ = 0;
    std::unique_ptr<ChatApiService> service_;
    std::unique_ptr<grpc::Server> server_;
};

struct BenchOptions {
    int send_requests = 2000;
    int sync_requests = 0;
    int concurrency = 16;
    int receiver_count = 128;
    int sync_limit = 100;
    int uid_offset = 0;
    bool ack_after_sync = true;
};

struct OpStats {
    std::atomic<int> attempts{0};
    std::atomic<int> success{0};
    std::atomic<int64_t> total_latency_us{0};
    std::atomic<int64_t> max_latency_us{0};
    mutable std::mutex latencies_mu;
    std::vector<int64_t> latencies_us;

    explicit OpStats(std::size_t reserve_count = 0) {
        latencies_us.reserve(reserve_count);
    }

    void Record(bool ok, int64_t latency_us) {
        attempts.fetch_add(1, std::memory_order_relaxed);
        total_latency_us.fetch_add(latency_us, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(latencies_mu);
            latencies_us.push_back(latency_us);
        }

        int64_t current_max = max_latency_us.load(std::memory_order_relaxed);
        while (latency_us > current_max &&
               !max_latency_us.compare_exchange_weak(current_max, latency_us,
                                                     std::memory_order_relaxed)) {
        }

        if (ok) {
            success.fetch_add(1, std::memory_order_relaxed);
        }
    }
};

constexpr int kMaxWorkerThreads = 64;

BenchOptions ParseArgs(int argc, char** argv) {
    BenchOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--requests" && i + 1 < argc) {
            opts.send_requests = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--send-requests" && i + 1 < argc) {
            opts.send_requests = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--sync-requests" && i + 1 < argc) {
            opts.sync_requests = std::max(0, std::atoi(argv[++i]));
        } else if (arg == "--concurrency" && i + 1 < argc) {
            opts.concurrency = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--receivers" && i + 1 < argc) {
            opts.receiver_count = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--sync-limit" && i + 1 < argc) {
            opts.sync_limit = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--uid-offset" && i + 1 < argc) {
            opts.uid_offset = std::max(0, std::atoi(argv[++i]));
        } else if (arg == "--no-ack-after-sync") {
            opts.ack_after_sync = false;
        }
    }

    opts.send_requests = std::max(1, opts.send_requests);
    opts.sync_requests = std::max(0, opts.sync_requests);
    opts.receiver_count = std::max(1, opts.receiver_count);
    opts.concurrency = std::max(1, opts.concurrency);
    opts.sync_limit = std::max(1, opts.sync_limit);
    return opts;
}

void CleanupReceiver(int64_t uid, uint64_t max_seq_hint = 4096) {
    auto& redis = RedisMgr::getInstance();
    redis.del("user_inbox_seq:" + std::to_string(uid));
    redis.del("offline_index:" + std::to_string(uid));
    redis.del("ack_seq:" + std::to_string(uid));
    for (uint64_t seq = 1; seq <= max_seq_hint; ++seq) {
        redis.del("offline_msg:" + std::to_string(uid) + ":" + std::to_string(seq));
    }
}

double PercentileMs(const std::vector<int64_t>& sorted_latencies_us, double percentile) {
    if (sorted_latencies_us.empty()) {
        return 0.0;
    }

    const double rank = percentile * static_cast<double>(sorted_latencies_us.size() - 1);
    const auto lower = static_cast<std::size_t>(rank);
    const auto upper = std::min(lower + 1, sorted_latencies_us.size() - 1);
    const double fraction = rank - static_cast<double>(lower);
    const double value_us = static_cast<double>(sorted_latencies_us[lower]) * (1.0 - fraction) +
                            static_cast<double>(sorted_latencies_us[upper]) * fraction;
    return value_us / 1000.0;
}

void PrintStats(const char* label, const OpStats& stats) {
    const auto attempts = stats.attempts.load(std::memory_order_relaxed);
    const auto success = stats.success.load(std::memory_order_relaxed);
    const auto total_latency_us = stats.total_latency_us.load(std::memory_order_relaxed);
    const auto max_latency_us = stats.max_latency_us.load(std::memory_order_relaxed);
    const double avg_latency_ms = success > 0
        ? static_cast<double>(total_latency_us) / success / 1000.0
        : 0.0;
    std::vector<int64_t> sorted_latencies_us;
    {
        std::lock_guard<std::mutex> lock(stats.latencies_mu);
        sorted_latencies_us = stats.latencies_us;
    }
    std::sort(sorted_latencies_us.begin(), sorted_latencies_us.end());

    std::cout << label
              << " attempts=" << attempts
              << " success=" << success
              << " avg_latency_ms=" << avg_latency_ms
              << " p50_ms=" << PercentileMs(sorted_latencies_us, 0.50)
              << " p95_ms=" << PercentileMs(sorted_latencies_us, 0.95)
              << " p99_ms=" << PercentileMs(sorted_latencies_us, 0.99)
              << " max_latency_ms=" << (max_latency_us / 1000.0)
              << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    Logger::init();
    spdlog::set_level(spdlog::level::err);

    try {
        const auto opts = ParseArgs(argc, argv);
        const int worker_threads = std::min(opts.concurrency, kMaxWorkerThreads);
        if (worker_threads < opts.concurrency) {
            std::cerr << "Requested concurrency " << opts.concurrency
                      << " exceeds safe thread cap " << kMaxWorkerThreads
                      << "; using " << worker_threads
                      << " worker threads to avoid exhausting file descriptors."
                      << std::endl;
        }

        ScopedChatApiServer server;

        const int64_t from_uid = 3000001 + static_cast<int64_t>(opts.uid_offset) * 100000;
        const int64_t receiver_base = 3001000 + static_cast<int64_t>(opts.uid_offset) * 100000;
        for (int i = 0; i < opts.receiver_count; ++i) {
            CleanupReceiver(receiver_base + i);
        }

        std::vector<std::atomic<uint64_t>> sync_cursor(opts.receiver_count);
        for (auto& cursor : sync_cursor) {
            cursor.store(0, std::memory_order_relaxed);
        }

        std::atomic<int> next_send_index{0};
        std::atomic<int> next_sync_index{0};
        std::atomic<int64_t> sync_messages{0};
        std::atomic<int> ack_attempts{0};
        std::atomic<int> ack_success{0};
        OpStats send_stats(static_cast<std::size_t>(opts.send_requests));
        OpStats sync_stats(static_cast<std::size_t>(opts.sync_requests));

        const auto benchmark_start = std::chrono::steady_clock::now();

        std::vector<std::thread> workers;
        workers.reserve(worker_threads);
        for (int t = 0; t < worker_threads; ++t) {
            workers.emplace_back([&, t]() {
                auto stub = server.NewStub();

                while (true) {
                    const int current_send = next_send_index.load(std::memory_order_relaxed);
                    const int current_sync = next_sync_index.load(std::memory_order_relaxed);
                    if (current_send >= opts.send_requests && current_sync >= opts.sync_requests) {
                        break;
                    }

                    bool do_send = false;
                    if (current_send < opts.send_requests) {
                        if (opts.sync_requests == 0) {
                            do_send = true;
                        } else if (current_sync >= opts.sync_requests) {
                            do_send = true;
                        } else {
                            do_send = (static_cast<int64_t>(current_send) * opts.sync_requests) <=
                                      (static_cast<int64_t>(current_sync) * opts.send_requests);
                        }
                    }

                    if (do_send) {
                        const int index = next_send_index.fetch_add(1, std::memory_order_relaxed);
                        if (index >= opts.send_requests) {
                            continue;
                        }

                        const auto to_uid = receiver_base + (index % opts.receiver_count);
                        im::SendMessageReq req;
                        req.set_from_uid(from_uid);
                        req.set_to_uid(to_uid);
                        req.set_client_msg_id("bench-" + std::to_string(opts.uid_offset) + "-" +
                                              std::to_string(t) + "-" + std::to_string(index));
                        req.set_payload("payload-" + std::to_string(index));
                        req.set_trace_id("bench-trace");

                        im::SendMessageResp resp;
                        grpc::ClientContext rpc_ctx;
                        const auto rpc_start = std::chrono::steady_clock::now();
                        const auto status = stub->SendMessage(&rpc_ctx, req, &resp);
                        const auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - rpc_start).count();
                        send_stats.Record(status.ok() && resp.ok(), latency_us);
                    } else {
                        const int index = next_sync_index.fetch_add(1, std::memory_order_relaxed);
                        if (index >= opts.sync_requests) {
                            continue;
                        }

                        const int receiver_idx = index % opts.receiver_count;
                        const auto uid = receiver_base + receiver_idx;
                        const auto last_seq = sync_cursor[receiver_idx].load(std::memory_order_relaxed);

                        im::SyncUpReq sync_req;
                        sync_req.set_uid(uid);
                        sync_req.set_last_inbox_seq(last_seq);
                        sync_req.set_limit(opts.sync_limit);

                        im::SyncUpResp sync_resp;
                        grpc::ClientContext sync_ctx;
                        const auto rpc_start = std::chrono::steady_clock::now();
                        const auto status = stub->Sync(&sync_ctx, sync_req, &sync_resp);
                        const auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - rpc_start).count();
                        const bool ok = status.ok() && sync_resp.ok();
                        sync_stats.Record(ok, latency_us);

                        if (!ok) {
                            continue;
                        }

                        sync_messages.fetch_add(sync_resp.msgs_size(), std::memory_order_relaxed);

                        if (sync_resp.max_inbox_seq() > last_seq) {
                            sync_cursor[receiver_idx].store(sync_resp.max_inbox_seq(), std::memory_order_relaxed);

                            if (opts.ack_after_sync) {
                                ack_attempts.fetch_add(1, std::memory_order_relaxed);

                                im::AckUpReq ack_req;
                                ack_req.set_uid(uid);
                                ack_req.set_type(im::ACK_RECEIVED);
                                ack_req.set_max_inbox_seq(sync_resp.max_inbox_seq());
                                if (sync_resp.msgs_size() > 0) {
                                    ack_req.set_msg_id(sync_resp.msgs(sync_resp.msgs_size() - 1).msg_id());
                                    ack_req.set_ts_ms(sync_resp.msgs(sync_resp.msgs_size() - 1).ts_ms());
                                }

                                im::AckUpResp ack_resp;
                                grpc::ClientContext ack_ctx;
                                const auto ack_status = stub->Ack(&ack_ctx, ack_req, &ack_resp);
                                if (ack_status.ok() && ack_resp.ok()) {
                                    ack_success.fetch_add(1, std::memory_order_relaxed);
                                }
                            }
                        }
                    }
                }
            });
        }

        for (auto& worker : workers) {
            worker.join();
        }

        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - benchmark_start).count();
        const auto total_success = send_stats.success.load(std::memory_order_relaxed) +
                                   sync_stats.success.load(std::memory_order_relaxed);
        const double overall_qps = elapsed > 0 ? static_cast<double>(total_success) / elapsed : 0.0;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "ChatApi mixed benchmark" << std::endl;
        std::cout << "send_requests=" << opts.send_requests
                  << " sync_requests=" << opts.sync_requests
                  << " requested_concurrency=" << opts.concurrency
                  << " worker_threads=" << worker_threads
                  << " receivers=" << opts.receiver_count
                  << " sync_limit=" << opts.sync_limit
                  << " ack_after_sync=" << (opts.ack_after_sync ? "true" : "false")
                  << " uid_offset=" << opts.uid_offset
                  << std::endl;
        std::cout << "elapsed_sec=" << elapsed
                  << " overall_success_qps=" << overall_qps
                  << " sync_messages=" << sync_messages.load(std::memory_order_relaxed)
                  << " ack_attempts=" << ack_attempts.load(std::memory_order_relaxed)
                  << " ack_success=" << ack_success.load(std::memory_order_relaxed)
                  << std::endl;
        PrintStats("send", send_stats);
        PrintStats("sync", sync_stats);

        for (int i = 0; i < opts.receiver_count; ++i) {
            CleanupReceiver(receiver_base + i, 4096);
        }

        return (send_stats.success.load(std::memory_order_relaxed) == opts.send_requests &&
                sync_stats.success.load(std::memory_order_relaxed) == opts.sync_requests)
            ? 0
            : 1;
    } catch (const std::exception& e) {
        std::cerr << "ChatApiQpsBenchmark failed: " << e.what() << std::endl;
        return 1;
    }
}
