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
				unsigned _radius = 2);

		//	torch::Tensor getPixel(float x, float y) const;
			torch::Tensor getPixels(const torch::Tensor& _positions) const;
		private:
			math::Matrix<torch::Tensor> m_embeddedColors;
			int64_t m_embeddingDim;
		};
	}

	auto constructMapOptim(const std::vector<sf::Image>& _srcImages,
		const std::vector<sf::Image>& _dstImages,
		unsigned _numThreads = 1)
	{
		assert(_srcImages.size() == _dstImages.size());

		ZoneMap srcZoneMap(_srcImages[0], _dstImages[0]);
		ZoneMap dstZoneMap(_dstImages[0], _srcImages[0]);

		using namespace details;

		for (const auto& [col, pixelsSrc] : srcZoneMap)
		{
			const auto& pixelsDst = dstZoneMap(col);

			// data setup
			const size_t numImgs = _srcImages.size() - 1;
			std::vector<ColorEmbedding> embeddings;
			embeddings.reserve(numImgs);
			std::vector<InterpolatedImage> dstInterpolated;
			dstInterpolated.reserve(numImgs);

			std::vector<torch::Tensor> srcColors;
			srcColors.reserve(numImgs);
			std::vector<torch::Tensor> srcPositions;
			srcPositions.reserve(numImgs);

			for (size_t i = 1; i < _srcImages.size(); ++i)
			{
				torch::NoGradGuard noGrad;
				embeddings.emplace_back(_srcImages[i], pixelsSrc);
				dstInterpolated.emplace_back(_dstImages[i], embeddings.back());

				const int64_t numPixels = static_cast<int64_t>(pixelsSrc.size());
				const int64_t dims = static_cast<int64_t>(embeddings.back().dimension());
				srcColors.push_back(torch::zeros({ numPixels, dims }));
				srcPositions.push_back(torch::zeros({ numPixels, 2 }, requires_grad(true)));

				for (size_t j = 0; j < pixelsSrc.size(); ++j)
				{
					using namespace torch::indexing;
					const int64_t col = static_cast<int64_t>(j);
					const sf::Vector2u pos = getIndex(_srcImages[i], pixelsSrc[j]);
					srcColors.back().index_put_({ col, Slice() }, embeddings.back().get(_srcImages[i].getPixel(pos.x,pos.y)));
					srcPositions.back().index_put_({ col, Slice() }, torch::tensor({ float(pos.x), float(pos.y) }));
				}
			}

			// network setup
			MultiLayerPerceptron net(MLPOptions(2));

			// training
			torch::optim::Adam optimizer(net->parameters(),
				torch::optim::AdamOptions(1e-2));

			for (size_t epoch = 0; epoch < 10000; ++epoch)
			{
				torch::Tensor loss = torch::zeros({ 1 });
				for (size_t j = 0; j < numImgs; ++j)
				{
					torch::Tensor dstPositions = net->forward(srcPositions[j]);
					loss += torch::mse_loss(dstInterpolated[j].getPixels(dstPositions), srcColors[j]);
				}
				loss.backward();
				optimizer.step();

				std::cout << "Epoch: " << epoch << " loss: " << loss.item<float>() << "\n";
			}
		}
	}
}