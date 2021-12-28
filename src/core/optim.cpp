#include "optim.hpp"
#include "../math/vectorext.hpp"

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
		using namespace torch;
		using namespace torch::indexing;

		InterpolatedImage::InterpolatedImage(const sf::Image& _src,
			const ColorEmbedding& _embedding,
			const ZoneMap::PixelList& _pixels,
			unsigned _radius)
			: m_embeddedColors(_src.getSize(), torch::zeros({int64_t(_embedding.dimension())})),
			m_embeddingDim(_embedding.dimension())
		{
		//	sf::Vector2u min = _src.getSize();
		//	sf::Vector2u max = sf::Vector2u(0u, 0u);
			for (size_t px : _pixels)
			{
				Tensor color = _embedding.get(getPixelFlat(_src, px));
				
				const sf::Vector2u pos = getIndex(_src, px);
				const unsigned iMax = std::min(_src.getSize().x, pos.x + _radius);
				const unsigned jMax = std::min(_src.getSize().y, pos.y + _radius);
				for(unsigned j = pos.y > _radius ? pos.y - _radius : 0; j <jMax; ++j)
					for (unsigned i = pos.x > _radius ? pos.x - _radius : 0; i < iMax; ++i)
					{
						const sf::Vector2f dif(static_cast<float>(pos.x) - static_cast<float>(i),
							static_cast<float>(pos.y) - static_cast<float>(j));
						const float d = std::sqrt(math::dot(dif, dif))+1.f;
						m_embeddedColors(i,j) += color * 1.f / d;
					}
			}
		}

	/*	torch::Tensor InterpolatedImage::getPixel(float x, float y) const
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
		}*/

		Tensor InterpolatedImage::getPixels(const Tensor& _positions) const
		{
			Tensor low = _positions.floor();
			Tensor high = _positions.ceil();
			Tensor t = _positions - low;
			Tensor result = torch::zeros({ _positions.size(0), m_embeddingDim });

			for (int64_t i = 0; i < _positions.size(0); ++i)
			{
				auto clampX = [&](float val) 
				{ 
					return static_cast<unsigned>(std::clamp(val, 0.f, static_cast<float>(m_embeddedColors.size.x - 1))); 
				};
				auto clampY = [&](float val)
				{
					return static_cast<unsigned>(std::clamp(val, 0.f, static_cast<float>(m_embeddedColors.size.y - 1)));
				};

				const unsigned lowX = clampX(low.index({i, 0}).item<float>());
				const unsigned highX = clampX(high.index({ i, 0 }).item<float>());
				const unsigned lowY = clampY(low.index({ i, 1 }).item<float>());
				const unsigned highY = clampY(high.index({ i, 1 }).item<float>());
				Tensor xy = m_embeddedColors(lowX, lowY);
				Tensor Xy = m_embeddedColors(highX, lowY);
				Tensor xY = m_embeddedColors(lowX, highY);
				Tensor XY = m_embeddedColors(highX, highY);

				const auto& tx = t.index({ i,0 });
				const auto& ty = t.index({ i,1 });
				result.index_put_({ i, Slice() }, 
					(1.f - ty) *  ((1.f - tx) * xy + tx * Xy)
					+ ty * ((1.f-tx) * xY + tx * XY));
			}

			return result;
		}
	}

}