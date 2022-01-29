#pragma once

#include "map.hpp"
#include "../math/matrix.hpp"
#include "../spritesheet.hpp"
/*#include <type_traits>
namespace std{
	template< class F, class... ArgTypes>
	using result_of = std::invoke_result<F, ArgTypes...>;

	template< class F, class... ArgTypes>
	using result_of_t = typename std::invoke_result_t<F, ArgTypes...>;
}*/
#pragma warning (disable : 4624 )
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
			TORCH_ARG(int64_t, hidden_layers) = 1;
			TORCH_ARG(bool, bias) = true;
			TORCH_ARG(ActivationFn, activation) = torch::tanh;
			TORCH_ARG(double, total_time) = 0.0;
			TORCH_ARG(bool, residual) = true;
		};

		// Fully connected multi-layer perceptron with residual connections.
		struct MultiLayerPerceptronImpl : public torch::nn::Cloneable<MultiLayerPerceptronImpl>
		{
			using Options = MLPOptions;
			explicit MultiLayerPerceptronImpl(const MLPOptions& _options);

			void reset() override;

			torch::Tensor forward(torch::Tensor _input);
			torch::Tensor inverse(torch::Tensor _output);

			void computeInverse();

			MLPOptions options;
			std::vector<torch::nn::Linear> layers;
			torch::Tensor inverseWeight;
			torch::Tensor bias;
			double timeStep;
		};

		TORCH_MODULE(MultiLayerPerceptron);

		class ColorEmbedding
		{
		public:
			ColorEmbedding(const sf::Image& _src, const ZoneMap::PixelList& _pixels);

			torch::Tensor get(sf::Color _color) const;
			size_t dimension() const { return m_embedding.size(); }
		private:
			std::unordered_map<sf::Uint32, torch::Tensor> m_embedding;
		};

		class InterpolatedImage
		{
		public:
			InterpolatedImage(const sf::Image& _src, 
				const ColorEmbedding& _embedding, 
				const ZoneMap::PixelList& _pixels,
				unsigned _radius = 3);

		//	torch::Tensor getPixel(float x, float y) const;
			torch::Tensor getPixels(const torch::Tensor& _positions) const;
		private:
			math::Matrix<torch::Tensor> m_embeddedColors;
			int64_t m_embeddingDim;
		};
	}

	TransferMap constructMapOptim(const std::vector<sf::Image>& _srcImages,
		const std::vector<sf::Image>& _dstImages,
		unsigned _numThreads = 1,
		unsigned _radius = 3,
		unsigned _numEpochs = 200);
}