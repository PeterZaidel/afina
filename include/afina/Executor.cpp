////
//// Created by peter on 11.04.18.
////
//#include <afina/Executor.h>
//
//#include <iostream>
//#include <functional>
//#include <unordered_set>
//#include <pthread.h>
//
//
//namespace Afina {
//
//    void Executor::Start(size_t low, size_t hight, size_t max, std::chrono::milliseconds idle) {
//        std::cout << "pool: " << __PRETTY_FUNCTION__ << std::endl;
//
//        low_watermark = low;
//        hight_watermark = hight;
//        max_queue_size = max;
//        idle_time = idle;
//
//        std::unique_lock<std::mutex> start_lock(sh_res_mutex);
//
//        // Initiate workers threads
//        for (size_t i = 0; i < low_watermark; ++i)
//        {
//            if (!initiate_thread(this, perform, false)) {
//                start_lock.unlock();
//                this->Stop(true);
//                return;
//            }
//        }
//
//        state.store(Executor::State::kRun, std::memory_order_relaxed);
//    }
//
//    void Executor::Stop(bool await) {
//        std::cout << "pool: " << __PRETTY_FUNCTION__ << std::endl;
//
//        std::unique_lock<std::mutex> lock(sh_res_mutex);
//
//        state.store(Executor::State::kStopping, std::memory_order_release);
//        tasks.clear();
//
//        lock.unlock();
//
//        if (await) {
//            Join();
//        }
//    }
//
//    void Executor::Join() {
//        std::cout << "pool: " << __PRETTY_FUNCTION__ << std::endl;
//
//        std::unique_lock<std::mutex> sh_res_lock(sh_res_mutex);
//
//        while (!threads.empty()) {
//            empty_condition.notify_all();
//            term_condition.wait(sh_res_lock);
//        }
//        state.store(Executor::State::kStopped, std::memory_order_relaxed);
//    }
//}
