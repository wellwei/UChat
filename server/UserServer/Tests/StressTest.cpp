#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <getopt.h>
#include "GrpcClient.h"

// 全局统计
std::atomic<int64_t> g_success{0};
std::atomic<int64_t> g_error{0};
std::vector<int64_t> g_latencys;
std::mutex g_latency_mutex;

void PrintUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  -c <num>   Number of concurrent connections (default: 10)\n"
              << "  -n <num>   Total number of requests (default: 1000)\n"
              << "  -h <host>  Server host:port (default: localhost:50051)\n"
              << "  -t <ms>    Request timeout in ms (default: 3000)\n"
              << "  -r <rpc>   RPC to test: getprofile|search (default: getprofile)\n"
              << "  -v         Verbose output\n"
              << "  --help     Show this help message\n";
}

struct Options {
    int concurrency = 10;
    int total_requests = 1000;
    std::string host = "localhost:50053";
    int timeout_ms = 3000;
    std::string rpc = "search";
    bool verbose = false;
};

Options ParseArgs(int argc, char* argv[]) {
    Options opts;
    static struct option long_options[] = {
        {"help", no_argument, 0, 'H'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:n:h:t:r:v", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'c': opts.concurrency = std::stoi(optarg); break;
            case 'n': opts.total_requests = std::stoi(optarg); break;
            case 'h': opts.host = optarg; break;
            case 't': opts.timeout_ms = std::stoi(optarg); break;
            case 'r': opts.rpc = optarg; break;
            case 'v': opts.verbose = true; break;
            case 'H': PrintUsage(argv[0]); exit(0);
            default: PrintUsage(argv[0]); exit(1);
        }
    }
    return opts;
}

void RunWorker(const Options& opts, int worker_id) {
    // 每个 worker 创建自己的客户端连接
    GrpcClient client(opts.host);

    // 每个 worker 应该发送的请求数
    int requests_per_worker = opts.total_requests / opts.concurrency;
    // 加上余数给前面的 worker
    if (worker_id < opts.total_requests % opts.concurrency) {
        requests_per_worker++;
    }

    for (int i = 0; i < requests_per_worker; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        bool ok = false;
        if (opts.rpc == "getprofile") {
            // 随机 uid 1-100
            uint64_t uid = (worker_id * 1000 + i) % 100 + 1;
            ok = client.GetUserProfile(uid);
        } else if (opts.rpc == "search") {
            // 搜索关键词
            ok = client.SearchUser("test");
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        int64_t latency_ms = latency_us / 1000;

        if (ok) {
            g_success++;
        } else {
            g_error++;
        }

        // 记录延迟
        {
            std::lock_guard<std::mutex> lock(g_latency_mutex);
            g_latencys.push_back(latency_ms);
        }
    }
}

double CalculatePercentile(std::vector<int64_t>& sorted, double p) {
    if (sorted.empty()) return 0;
    double idx = p / 100.0 * (sorted.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(idx));
    size_t upper = static_cast<size_t>(std::ceil(idx));
    if (lower == upper) return sorted[lower];
    return sorted[lower] + (sorted[upper] - sorted[lower]) * (idx - lower);
}

int main(int argc, char* argv[]) {
    auto opts = ParseArgs(argc, argv);

    std::cout << "=== UserServer Stress Test ===\n"
              << "Server: " << opts.host << "\n"
              << "Concurrency: " << opts.concurrency << "\n"
              << "Total Requests: " << opts.total_requests << "\n"
              << "RPC: " << opts.rpc << "\n"
              << "================================\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    // 启动 workers
    std::vector<std::thread> workers;
    workers.reserve(opts.concurrency);
    for (int i = 0; i < opts.concurrency; ++i) {
        workers.emplace_back(RunWorker, std::ref(opts), i);
    }

    // 等待所有 workers 完成
    for (auto& w : workers) {
        w.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_sec = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() / 1000.0;

    // 计算延迟统计
    std::sort(g_latencys.begin(), g_latencys.end());

    int64_t total = g_success.load() + g_error.load();
    double qps = total / duration_sec;

    std::cout << "\n=== Results ===\n"
              << "Total: " << total << "\n"
              << "Success: " << g_success.load() << "\n"
              << "Error: " << g_error.load() << "\n"
              << "QPS: " << qps << "\n";

    if (!g_latencys.empty()) {
        std::cout << "Latency (ms): min=" << g_latencys.front()
                  << ", p50=" << CalculatePercentile(g_latencys, 50)
                  << ", p95=" << CalculatePercentile(g_latencys, 95)
                  << ", p99=" << CalculatePercentile(g_latencys, 99)
                  << ", max=" << g_latencys.back() << "\n";
    }

    return 0;
}
