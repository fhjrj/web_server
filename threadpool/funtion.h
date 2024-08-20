#ifndef FUN_H
#define FUN_H

#include <memory>
#include <thread>

class function_war {
	struct impl_base {
		virtual void call() = 0;
		virtual ~impl_base() {};
	};
	std::unique_ptr<impl_base> impl;
	template<typename F>
	struct impl_type :impl_base {
		F f;
		impl_type(F&& f_) :f(std::move(f_)) {}//函数转移到此种去进行调用
		void call() {
			f();
		}
	};
public:
	template<typename F>
	function_war(F&& f) :impl(new impl_type<F>(std::move(f))) {}
	void operator()() {
		impl->call();//
	}

	function_war() = default;
	function_war(function_war&& other) : impl(std::move(other.impl)) {}
	function_war& operator=(function_war&& other) {
		impl = std::move(other.impl);
		return *this;
	}

	function_war(function_war const&) = delete;
	function_war& operator=(function_war const&) = delete;

};


#endif