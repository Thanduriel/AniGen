#pragma once

#include <thread>
#include <string_view>

namespace utils {
	// @param Fn function to be parallelized, should take a range (It,It)
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

	struct SplitNameResult
	{
		std::string_view name;
		std::string_view animation;
	};
	inline SplitNameResult splitName(std::string_view str)
	{
		auto begin = str.find_first_of("_");
		auto end = str.find_last_of(".");
		return { str.substr(0, begin), str.substr(begin + 1, end - begin) };
	}

	// type trait that checks for the operator<<(S&, T)
	template <typename S, typename T, typename = void>
	struct is_stream_writable : std::false_type {};
	template <typename S, typename T>
	struct is_stream_writable<S,
		T,
		std::void_t<decltype(std::declval<S&>() << std::declval<T>())>>
		: std::true_type {};

	// type trait that checks for the operator>>(S&, T&)
	template <typename S, typename T, typename = void>
	struct is_stream_readable : std::false_type {};
	template <typename S, typename T>
	struct is_stream_readable<S,
		T,
		std::void_t<decltype(std::declval<S&>() >> std::declval<T&>())>>
		: std::true_type {};
}