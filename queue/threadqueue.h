#ifndef THREAD_QUEUE
#define THREAD_QUEUE

#include <condition_variable>
#include <mutex>
#include <memory>
#include <atomic>
template<typename T>
class threadsafe_queue1
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        node* prev;
    };

    std::mutex head_mutex;
    std::unique_ptr<node> head;
    std::mutex tail_mutex;
    node* tail;
    std::condition_variable data_cond;
    std::atomic_bool  bstop;//用于终止

    node* get_tail()
    {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    std::unique_ptr<node> pop_head()   
    {
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }

    std::unique_lock<std::mutex> wait_for_data()   
    {
        std::unique_lock<std::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock,[&] {return head.get() != get_tail() || bstop.load() == true; });//与原来非改装的安全队列相比，队列一个bstop==true(),其作用是在手动停止线程池时，进行安全退出
        return std::move(head_lock);   
    }

        std::unique_ptr<node> wait_pop_head()
        {
            std::unique_lock<std::mutex> head_lock(wait_for_data());  
            if (bstop.load()) {
                return nullptr;/*因为想要停止，所以这里返回空*/
            }

                return pop_head();
        }
        std::unique_ptr<node> wait_pop_head(T& value)
        {
            std::unique_lock<std::mutex> head_lock(wait_for_data());  
            if (bstop.load()) {
                return nullptr;/*同理，在手动停止下，唤醒所以挂起的线程，线程都获得nullptr*/
            }
            value = std::move(*head->data);
            return pop_head();
        }

        std::unique_ptr<node> try_pop_head()
        {
            std::lock_guard<std::mutex> head_lock(head_mutex);//有锁 不用转移
            if (head.get() == get_tail())
            {
                return std::unique_ptr<node>();
            }
            return pop_head();
        }
        std::unique_ptr<node> try_pop_head(T& value)
        {
            std::lock_guard<std::mutex> head_lock(head_mutex);
            if (head.get() == get_tail())
            {
                return std::unique_ptr<node>();
            }
            value = std::move(*head->data);
            return pop_head();
        }
public:

    threadsafe_queue1() :  
        head(new node), tail(head.get())
    {}

    ~threadsafe_queue1() {
        bstop.store(true);
        data_cond.notify_all();
    }

    threadsafe_queue1(const threadsafe_queue1& other) = delete;
    threadsafe_queue1& operator=(const threadsafe_queue1& other) = delete;

    void Exit() {
        bstop.store(true);
        data_cond.notify_all();
    }//同析构函数

    bool wait_and_pop_timeout(T& value) {
        std::unique_lock<std::mutex> head_lock(head_mutex);
        auto res = data_cond.wait_for(head_lock, std::chrono::milliseconds(100),
                [&] {return head.get() != get_tail() || bstop.load() == true; });
        if (res == false) {
            return false;
        }

        if (bstop.load()) {
            return false;
        }
   /*这里要进行判断两次*/
        value = std::move(*head->data);    
        head = std::move(head->next);
        return true;
    }

    std::shared_ptr<T> wait_and_pop()  
    {
        std::unique_ptr<node> const old_head = wait_pop_head();
        if (old_head == nullptr) {
            return nullptr;
        }
        return old_head->data;
    }

    bool  wait_and_pop(T& value)  
{
        std::unique_ptr<node> const old_head = wait_pop_head(value);
        if (old_head == nullptr) {
            return false;
        }
        return true;
    }


    std::shared_ptr<T> try_pop()
    {
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }

    bool try_pop(T& value)
    {
        std::unique_ptr<node> const old_head = try_pop_head(value);
        if (old_head) {
            return true;
        }
        return false;
    }

    bool empty()
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return (head.get() == get_tail());
    }

    void push(T new_value)
    {
      std::shared_ptr<T>(new T(std::move(new_value)));
        std::unique_ptr<node> p(new node);
        {
            std::lock_guard<std::mutex> tail_lock(tail_mutex);
            tail->data = new_value;
            node* const new_tail = p.get();
            new_tail->prev = tail;
            tail->next = std::move(p);
            tail = new_tail;//双链表更新
        }

        data_cond.notify_one();
    }


     bool try_steal(T& value) {
        std::unique_lock<std::mutex> tail_lock(tail_mutex,std::defer_lock);
        std::unique_lock<std::mutex>  head_lock(head_mutex, std::defer_lock);/*都加上锁，确保没有其他线程进行头尾结点的改动*/
        std::lock(tail_lock, head_lock);
        if (head.get() == tail)
        {
            return false;
        }

        node* prev_node = tail->prev;
        value = std::move(*(prev_node->data));
        tail = prev_node;
        tail->next = nullptr;
        return true;
    }

};


#endif
