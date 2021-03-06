#include "ts/concurrent_queue.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>
#include "CppUnitTest.h"

using ts::concurrent_queue;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace unittest {	

TEST_CLASS(concurrent_queue_concurrent_queue) {
public:

	TEST_METHOD(ctors)
	{
		concurrent_queue<int> queue(4);
		Assert::IsTrue(queue.empty());
		Assert::AreEqual<size_t>(0, queue.size());
		Assert::IsTrue(queue.wait_allowed());
	}

	TEST_METHOD(push_emplace_pop_one_thread)
	{
		std::unique_ptr<int> p0 = std::make_unique<int>(24);
		std::unique_ptr<int> p1 = std::make_unique<int>(100);
		int* p2 = new int(1024);

		// push
		concurrent_queue<std::unique_ptr<int>> queue(6);
		queue.push(std::move(p0));
		queue.push(std::move(p1));
		queue.emplace(p2);

		Assert::IsFalse(bool(p0));
		Assert::IsFalse(bool(p1));
		Assert::AreEqual<size_t>(3, queue.size());

		// push(iterator, iterator)
		std::vector<std::unique_ptr<int>> pointers;
		pointers.emplace_back(new int(1));
		pointers.emplace_back(new int(2));
		pointers.emplace_back(new int(3));
		queue.push(std::make_move_iterator(pointers.begin()), std::make_move_iterator(pointers.end()));
		
		Assert::IsTrue(std::all_of(pointers.cbegin(), pointers.cend(), [](const std::unique_ptr<int>& p) { return !p; }));
		Assert::AreEqual<size_t>(6, queue.size());

		// pop
		std::unique_ptr<int> out;

		// pop 24 back
		Assert::IsTrue(queue.try_pop(out));
		Assert::AreEqual(24, *out);
		Assert::AreEqual<size_t>(5, queue.size());

		// pop 100 back
		Assert::IsTrue(queue.try_pop(out));
		Assert::AreEqual(100, *out);
		Assert::AreEqual<size_t>(4, queue.size());

		// pop 1024 back
		Assert::IsTrue(queue.wait_pop(out));
		Assert::AreEqual(1024, *out);
		Assert::AreEqual<size_t>(3, queue.size());

		// pop 1 back
		Assert::IsTrue(queue.try_pop(out));
		Assert::AreEqual(1, *out);
		Assert::AreEqual<size_t>(2, queue.size());

		// pop 2 back
		Assert::IsTrue(queue.try_pop(out));
		Assert::AreEqual(2, *out);
		Assert::AreEqual<size_t>(1, queue.size());

		// pop 3 back
		Assert::IsTrue(queue.try_pop(out));
		Assert::AreEqual(3, *out);
		Assert::AreEqual<size_t>(0, queue.size());

		Assert::IsTrue(queue.empty());
	}

	TEST_METHOD(push_pop_several_threads)
	{
		const size_t thread_count = 29; // this_thread is not taken into account here
		const size_t elements_per_thread = 3000;
		using it_t = std::vector<int>::iterator;

		std::vector<int> origin_vector((thread_count + 1) * elements_per_thread); // thread_count + 1 - 1 means this_thread
		std::iota(origin_vector.begin(), origin_vector.end(), 0);
	

		concurrent_queue<int> queue(origin_vector.size());
		std::vector<int> actual_vector = origin_vector;

		// worker_func acts as a producer and a consumer interchangeable.
		// puts all the values from [begin, end) to the queue_.
		auto worker_func = [&queue, &actual_vector](it_t begin, it_t end) 
		{
			// put values into the queue_
			for (auto i = begin; i != end; ++i)
				queue.push(*i);

			// try_pop
			// get values from the queue_ and put them back into [begin, end)
			for (auto i = begin; i != end; ++i) {
				int v;
				while (!queue.try_pop(v)) { ; }
				
				*i = v;
			}

			// wait_pop
			// put values into the queue_ again
			for (auto i = begin; i != end; ++i)
				queue.push(*i);

			// get values from the queue_ and put them back into [begin, end)
			for (auto i = begin; i != end; ++i) {
				int v;
				queue.wait_pop(v);

				*i = v;
			}
		};

		
		// spawn worker threads
		std::vector<std::thread> threads;
		threads.reserve(thread_count);

		for (size_t i = 0; i < thread_count; ++i) {
			it_t begin = actual_vector.begin() + i * elements_per_thread;
			it_t end = begin + elements_per_thread;
			threads.emplace_back(worker_func, begin, end);
		}

		it_t begin = actual_vector.end() - elements_per_thread;
		it_t end = actual_vector.end();
		worker_func(begin, end);

		for (auto& t : threads)
			t.join();

		std::sort(actual_vector.begin(), actual_vector.end());
		Assert::IsTrue(std::equal(origin_vector.cbegin(), origin_vector.cend(), actual_vector.cbegin()));
	}

	TEST_METHOD(push_wait_allowed)
	{
		concurrent_queue<int> queue(1);

		std::thread waiter([&queue] {
			int v;
			queue.wait_pop(v);
		});

		queue.set_wait_allowed(false);
		waiter.join(); // if wait_flag does not work, we are going to wait forever }:]
	}
};

} // namespace
