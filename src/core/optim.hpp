#pragma once

#include "map.hpp"
/*#include <type_traits>
namespace std{
	template< class F, class... ArgTypes>
	using result_of = std::invoke_result<F, ArgTypes...>;

	template< class F, class... ArgTypes>
	using result_of_t = typename std::invoke_result_t<F, ArgTypes...>;
}*/
#include <torch/torch.h>

#include <functional>

namespace nn {

	namespace details {

		using ActivationFn = std::function<torch::Tensor(const torch::Tensor&)>;

		struct MLPOptions
		{
			MLPOptions(int64_t _inputSize) : input_size_(_inputSize), output_size_(_inputSize), hidden_size_(_inputSize) {}

			TORCH_ARG(int64_t, input_size);
			TORCH_ARG(int64_t, output_size);
			TORCH_ARG(int64_t, hidden_size);
			TORCH_ARG(int64_t, hidden_layers) = 0;
			TORCH_ARG(bool, bias) = true;
			TORCH_ARG(ActivationFn, activation) = torch::tanh;
			TORCH_ARG(double, total_time) = 0.0;
		};

		// Fully connected multi-layer perceptron with residual connections.
		struct MultiLayerPerceptronImpl : public torch::nn::Cloneable<MultiLayerPerceptronImpl>
		{
			using Options = MLPOptions;
			explicit MultiLayerPerceptronImpl(const MLPOptions& _options);

			void reset() override;

			torch::Tensor forward(torch::Tensor _input);

			MLPOptions options;
			std::vector<torch::nn::Linear> layers;
			double timeStep;
		};

		TORCH_MODULE(MultiLayerPerceptron);

		class ColorEmbedding
		{
		public:
			ColorEmbedding(const sf::Image& _src, const ZoneMap::PixelList& _pixels);

			torch::Tensor get(sf::Color _color) const;

		private:
			std::unordered_map<sf::Uint32, torch::Tensor> m_embedding;
		};

		class InterpolatedImage
		{
			InterpolatedImage(const sf::Image& _src, const ColorEmbedding& _embedding);

			torch::Tensor getPixel(float x, float y) const;
		private:
			const sf::Image& m_src;
			const ColorEmbedding& m_embedding;
		};
	}

	template<typename It>
	auto constructMapOptim(It _srcBegin, It _srcEnd,
		It _dstBegin, It _dstEnd,
		const ZoneMap* _zoneMap = nullptr,
		unsigned _numThreads = 1)
	{
		using namespace details;
		// for each image (or image zone) create a color embedding



	}
}