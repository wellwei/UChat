//
// Created by goodnight on 26-3-17.
//

#ifndef TIMERWHEEL_H
#define TIMERWHEEL_H
#include <functional>
#include <list>
#include <mutex>
#include <thread>

#include "Singleton.h"


class TimerWheel : public Singleton<TimerWheel> {
public:
    using Task = std::function<void()>;
    explicit TimerWheel(const int solt_nums = 600, const int tick_ms = 100)
        : solt_nums_(solt_nums), tick_ms_(tick_ms), current_slot_(0), running_(true) {
        wheel_.resize(solt_nums);
        worker_thread_ = std::thread(&TimerWheel::Run, this);
    }

    ~TimerWheel() {
        Stop();
    }

    // slots_num * tick_ms > 最大超时时间
    void AddTask(int delay_ms, Task task) {
        int ticks = delay_ms / tick_ms_;

        std::lock_guard<std::mutex> lock(mutex_);
        int target_slot = (current_slot_ + ticks) % solt_nums_;
        wheel_[target_slot].push_back(std::move(task));
    }

    void Stop() {
        running_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

private:

    void Run() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(tick_ms_));

            std::list<Task> current_tasks;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                current_tasks = std::move(wheel_[current_slot_]);
                current_slot_ = (current_slot_ + 1) % solt_nums_;
            }

            for (auto& task : current_tasks) {
                try {
                    task();
                } catch (...) {
                    // LOG
                }
            }
        }
    }

    int solt_nums_;
    int tick_ms_;
    int current_slot_;
    std::atomic_bool running_;
    std::vector<std::list<Task>> wheel_;
    std::mutex mutex_;
    std::thread worker_thread_;
};



#endif //TIMERWHEEL_H
