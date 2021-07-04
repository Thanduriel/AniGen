#pragma once

#include <thread>

namespace utils {
	// @param Fn function that applies a function to the range (It,It)
	// It must be random access for now
	template<typename It, typename Fn>
	void runMultiThreaded(It _begin, It _end, Fn _fn, size_t _numThreads = 1)
	{
		if (_numThreads == 1)
			_fn(_begin, _end);
		else
		{
			std::vector<std::thread> threads;
			threads.reserve(_numThreads - 1);

			using DistanceType = decltype(_end - _begin);
			const DistanceType n = static_cast<DistanceType>(_numThreads);
			const DistanceType rows = (_end - _begin) / n;
			for (DistanceType i = 0; i < n - 1; ++i)
				threads.emplace_back(_fn, i * rows, (i + 1) * rows);
			_fn((n - 1) * rows, _end);

			for (auto& thread : threads)
				thread.join();
		}
	}
}