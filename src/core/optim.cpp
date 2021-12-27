#include "optim.hpp"

namespace nn {

	namespace details {
	
		MultiLayerPerceptronImpl::MultiLayerPerceptronImpl(const MLPOptions& _options)
			: options(_options)
		{
			if (options.total_time() == 0.0)
				options.total_time(options.hidden_layers());
			reset();
		}

		void MultiLayerPerceptronImpl::reset()
		{
			layers.clear();
			layers.reserve(options.hidden_layers());
			timeStep = options.total_time() / options.hidden_layers();

			torch::nn::LinearOptions hiddenOptions(options.hidden_size(), options.hidden_size());
			hiddenOptions.bias(options.bias());
			for (int64_t i = 0; i < options.hidden_layers(); ++i)
			{
				layers.emplace_back(torch::nn::Linear(hiddenOptions));
				register_module("hidden" + std::to_string(i), layers.back());
			}
		}

		torch::Tensor MultiLayerPerceptronImpl::forward(torch::Tensor x)
		{
			auto& activation = options.activation();
			for (size_t i = 0; i+1 < layers.size(); ++i)
				x = x + timeStep * activation(layers[i](x));

			return x + layers.back()(x);
		}

		// *************************************************************** //
		ColorEmbedding::ColorEmbedding(const sf::Image& _src, const ZoneMap::PixelList& _pixels)
		{
			auto read = [&](size_t ind) 
			{
				const sf::Uint32* ptr = reinterpret_cast<const sf::Uint32*>(_src.getPixelsPtr());
				return ptr[ind];
			};

			torch::Tensor tensor;
			for (size_t ind : _pixels)
			{
				m_embedding.insert(std::pair<size_t, torch::Tensor>(read(ind), tensor));
			}

			const int64_t s = static_cast<int64_t>(m_embedding.size());
			int64_t cur = 0;
			for (auto& [key, value] : m_embedding)
			{
				value = torch::zeros({ s });
				value.index_put_({ cur }, 1.f);
			}
		}

		torch::Tensor ColorEmbedding::get(sf::Color _color) const
		{
			auto it = m_embedding.find(_color.toInteger());
			return it != m_embedding.end() ? it->second : torch::Tensor{};
		}

		// *************************************************************** //
		InterpolatedImage::InterpolatedImage(const sf::Image& _src, const ColorEmbedding& _embedding)
			: m_src(_src),
			m_embedding(_embedding)
		{}

		torch::Tensor InterpolatedImage::getPixel(float x, float y) const
		{
			const unsigned lowX = static_cast<unsigned>(std::floor(x));
			const unsigned highX = static_cast<unsigned>(std::ceil(x));

			const unsigned lowY = static_cast<unsigned>(std::floor(y));
			const unsigned highY = static_cast<unsigned>(std::ceil(y));

			const float tX = x - lowX;
			const float tY = y - lowY;

			return (1.f - tY) * ((1.f - tX) * m_embedding.get(m_src.getPixel(lowX, lowY))
				+ tX * m_embedding.get(m_src.getPixel(highX, lowY)))
				+ tY * ((1.f - tX) * m_embedding.get(m_src.getPixel(lowX, highY))
					+ tX * m_embedding.get(m_src.getPixel(highX, highY)));
		}
	}

}