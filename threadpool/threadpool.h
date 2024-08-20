#ifndef POOL_ANTHER
#define  POOL_ANTHER

#include "joiner.h"
#include "funtion.h"
//#include "blockqueue.h"
#include <atomic>
#include <thread>
#include <vector>
#include <future>
#include "../queue/threadqueue.h"

/*
class Threadpool {
private:
	Threadpool(const Threadpool&) = delete;
	Threadpool& operator=(const Threadpool&) = delete;
    std::queue<std::function<void()>> tasks;
	std::atomic<int> thread_num_;
	std::vector<std::thread>  pool_;
	std::atomic<bool> stop_;
	std::mutex cv_mt_;
	std::condition_variable cv_lock_;

	void start() {
		for (int i = 0; i < thread_num_; i++) {
			pool_.emplace_back([this]() {
				while (!this->stop_.load()) {
						std::unique_lock<std::mutex> cv_mt(cv_mt_);
						this->cv_lock_.wait(cv_mt, [this] {
							return this->stop_.load() || !this->tasks.empty();
							});//无任务线程挂起

						if (this->tasks.empty())
							return;
						auto task = std::move(this->tasks.front());
						this->tasks.pop();
					 cv_mt.unlock();
					this->thread_num_--;
					task();
					this->thread_num_++;
				}
				});//λ表达式
		}
	}

	void stop() {
		stop_.store(true);
		cv_lock_.notify_all();//全部唤醒，最后执行，没任务直接return,有任务执行完再说
		for (auto& td : pool_) {
			if (td.joinable()) {
				std::cout << "join thread " << td.get_id() << std::endl;
				td.join();
			}
		}
	}


public:
	static Threadpool& instance() {
		static Threadpool ins;
		return ins;
	}

	Threadpool(unsigned int num = std::thread::hardware_concurrency()) :stop_(false) {
		if (num <= 1) {
			thread_num_ = 2;
		}
		else {
			thread_num_ = num;
		}
		start();
	}

	~Threadpool() {
		stop();
	}
	
  template<class F>
    void AddTask(F&& task) {
       std::lock_guard<std::mutex> locker(cv_mt_);
            tasks.emplace(std::forward<F>(task));
        }
           cv_lock_.notify_one();
    }

};
*/


class Threadpool
{
private:

    void worker_thread(int index)
    {
        while (!done)
        {
            function_war wrapper;
            bool pop_res = thread_work_ques[index].try_pop(wrapper);
            if (pop_res) {
                wrapper();
                continue;
            }
          //发现任务队列为空 则依次循环进行偷任务，偷了一个就进行停止
            bool steal_res = false;
            for (int i = 0; i < thread_work_ques.size(); i++) {
                if (i == index) {
                    continue;
                }

                steal_res  = thread_work_ques[i].try_pop(wrapper);
                if (steal_res) {
                    wrapper();
                    break;
                }

            }

            if (steal_res) {
                continue;
            }

            std::this_thread::yield();
        }
    }
public:

    static Threadpool& instance() {
        static  Threadpool pool;
        return pool;
    }
    ~Threadpool()
    {
        
        done = true;
        for (unsigned i = 0; i < thread_work_ques.size(); i++) {
            thread_work_ques[i].Exit();
        }

        for (unsigned i = 0; i < threads.size(); ++i)
        {
            
            threads[i].join();
        }
    }

    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type>
        submit(FunctionType f)
    {
        int index = (atm_index.load() + 1) % thread_work_ques.size();
        atm_index.store(index);
        typedef typename std::result_of<FunctionType()>::type result_type;
        std::packaged_task<result_type()> task(std::move(std::forward<FunctionType>(f)));
        std::future<result_type> res(task.get_future());
        thread_work_ques[index].push(std::move(task));
        return res;
    }

private:
    Threadpool() :
        done(false), joiner(threads), atm_index(0)
    {
        unsigned const thread_count = std::thread::hardware_concurrency();
        try
        {
            thread_work_ques = std::vector < threadsafe_queue1<function_war>>(thread_count);

            for (unsigned i = 0; i < thread_count; ++i)
            {
                threads.push_back(std::thread(&Threadpool::worker_thread, this, i));
            }
        }
        catch (...)
        {
            done = true;
            for (int i = 0; i < thread_work_ques.size(); i++) {
                thread_work_ques[i].Exit();
            }
            throw;
        }
    }

    std::atomic_bool done;
    std::vector<threadsafe_queue1<function_war>> thread_work_ques;
    std::vector<std::thread> threads;
    join_threads joiner;
    std::atomic<int>  atm_index;
};

#endif