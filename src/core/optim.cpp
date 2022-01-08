#include "optim.hpp"
#include "../math/vectorext.hpp"

namespace nn {

	namespace details {
	
		MultiLayerPerceptronImpl::MultiLayerPerceptronImpl(const MLPOptions& _options)
			: torch::nn::Cloneable<MultiLayerPerceptronImpl>("mlp"),
			options(_options)
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
			for (size_t i = 0; i + 1 < layers.size(); ++i)
			{
				if (options.residual())
					x = x + timeStep * activation(layers[i](x));
				else
					x = timeStep * activation(layers[i](x));
			}

			return layers.back()(x);
		}

		torch::Tensor MultiLayerPerceptronImpl::inverse(torch::Tensor x)
		{
			auto& last = layers.back();
			x = torch::matmul(x - last->bias, torch::linalg::inv(last->weight).transpose(0, 1));

		/*	for (size_t i = layers.size() - 1; i > 0; --i)
			{
				auto& layer = layers[i];
				x = torch::linalg::inv(layer->weight) * (x - layer->bias);
			}
			torch::atanh()*/

			return x;
		}

		// *************************************************************** //
		ColorEmbedding::ColorEmbedding(const sf::Image& _src, const ZoneMap::PixelList& _pixels)
		{
			torch::Tensor tensor;
			for (size_t ind : _pixels)
			{
				m_embedding.insert(std::pair<sf::Uint32, torch::Tensor>(getPixelFlat(_src,ind).toInteger(), tensor));
			}

			const int64_t s = static_cast<int64_t>(m_embedding.size());
			int64_t cur = 0;
			for (auto& [key, value] : m_embedding)
			{
				value = torch::zeros({ s });
				value.index_put_({ cur++ }, 1.f);
			}
		}

		torch::Tensor ColorEmbedding::get(sf::Color _color) const
		{
			uint32_t what = _color.toInteger();
			auto it = m_embedding.find(_color.toInteger());
			return it != m_embedding.end() ? it->second : torch::zeros(int64_t(m_embedding.size()));
		}

		// *************************************************************** //
		using namespace torch;
		using namespace torch::indexing;

		InterpolatedImage::InterpolatedImage(const sf::Image& _src,
			const ColorEmbedding& _embedding,
			const ZoneMap::PixelList& _pixels,
			unsigned _radius)
			: m_embeddedColors(_src.getSize()),
			m_embeddingDim(_embedding.dimension())
		{
			// regular constructor makes only shallow copies
			for (auto& tensor : m_embeddedColors.elements)
				tensor = torch::zeros({ int64_t(_embedding.dimension()) });
		//	sf::Vector2u min = _src.getSize();
		//	sf::Vector2u max = sf::Vector2u(0u, 0u);
			for (size_t px : _pixels)
			{
				Tensor color = _embedding.get(getPixelFlat(_src, px));

				const sf::Vector2u pos = getIndex(_src, px);
				const unsigned iMax = std::min(_src.getSize().x, pos.x + _radius);
				const unsigned jMax = std::min(_src.getSize().y, pos.y + _radius);
				for(unsigned j = pos.y > _radius ? pos.y - _radius : 0; j < jMax; ++j)
					for (unsigned i = pos.x > _radius ? pos.x - _radius : 0; i < iMax; ++i)
					{
						const sf::Vector2f dif(static_cast<float>(pos.x) - static_cast<float>(i),
							static_cast<float>(pos.y) - static_cast<float>(j));
						const float d = std::sqrt(math::dot(dif, dif))+0.2f;
						m_embeddedColors(i,j) += color * 1.f / d;
					}
			}

			for (auto& tensor : m_embeddedColors.elements)
			{
				const float l = tensor.norm().item<float>();
				if(l != 0.f)
					tensor *= 1.f / l;
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

	template<typename Module>
	Module clone(const Module& _module)
	{
		using ModuleImpl = typename Module::Impl;
		return Module(std::dynamic_pointer_cast<ModuleImpl>(_module->clone()));
	}

	torch::Tensor indexSlice(const torch::Tensor& _tensor, const std::vector<size_t>& _inds)
	{
		torch::Tensor tensor = torch::zeros({ int64_t(_inds.size()), _tensor.sizes()[1] });
		for (size_t i = 0; i < _inds.size(); ++i)
			tensor[i] = _tensor[_inds[i]];
		return tensor;
	}

	TransferMap constructMapOptim(const std::vector<sf::Image>& _srcImages,
		const std::vector<sf::Image>& _dstImages,
		unsigned _numThreads)
	{
		assert(_srcImages.size() == _dstImages.size());

		ZoneMap srcZoneMap(_srcImages[0], _dstImages[0]);
		ZoneMap dstZoneMap(_dstImages[0], _srcImages[0]);

		TransferMap map(_srcImages.front().getSize());
		for (unsigned y = 0; y < map.size.y; ++y)
			for (unsigned x = 0; x < map.size.x; ++x)
				map(x, y) = sf::Vector2u(x, y);

		using namespace details;

		for (const auto& [col, pixelsSrc] : srcZoneMap)
		{
			if (col == 0) continue;

			const auto& pixelsDst = dstZoneMap(col);

			// data setup
			const size_t numImgs = _srcImages.size() - 1;
			std::vector<ColorEmbedding> embeddings;
			embeddings.reserve(numImgs);
			std::vector<InterpolatedImage> dstInterpolated;
			std::vector<InterpolatedImage> dstValidation;
			dstInterpolated.reserve(numImgs);

			std::vector<torch::Tensor> srcColors;
			srcColors.reserve(numImgs);
			std::vector<torch::Tensor> srcPositions;
			srcPositions.reserve(numImgs);
			torch::Tensor dstPositions = torch::zeros( {int64_t(pixelsDst.size()), 2} );
			for (size_t j = 0; j < pixelsDst.size(); ++j)
			{
				const sf::Vector2u pos = getIndex(_dstImages[0], pixelsDst[j]);
				dstPositions.index_put_({ int64_t(j), Slice() }, torch::tensor({ float(pos.x), float(pos.y) }));
			}

			std::vector<torch::data::samplers::RandomSampler> samplers;

			sf::Vector2u originSum(0u, 0u);
			sf::Vector2u targetSum(0u, 0u);
			for (size_t px : pixelsSrc)
				originSum += getIndex(_srcImages[0], px);
			for (size_t px : pixelsDst)
				targetSum += getIndex(_dstImages[0], px);

			const sf::Vector2f origin = sf::Vector2f(originSum) * (1.f / pixelsSrc.size());
			const sf::Vector2f target = sf::Vector2f(targetSum) * (1.f / pixelsDst.size());

			for (size_t i = 1; i < _srcImages.size(); ++i)
			{
				torch::NoGradGuard noGrad;
				embeddings.emplace_back(_srcImages[i], pixelsSrc);
				const ColorEmbedding& embedding = embeddings.back();
				dstInterpolated.emplace_back(_dstImages[i], embedding, pixelsDst);
				dstValidation.emplace_back(_dstImages[i], embedding, pixelsDst, 1);

				const int64_t numPixels = static_cast<int64_t>(pixelsSrc.size());
				const int64_t dims = static_cast<int64_t>(embedding.dimension());
				srcColors.push_back(torch::zeros({ numPixels, dims }));
				srcPositions.push_back(torch::zeros({ numPixels, 2 }, requires_grad(true)));

				for (size_t j = 0; j < pixelsSrc.size(); ++j)
				{
					using namespace torch::indexing;
					const int64_t col = static_cast<int64_t>(j);
					const sf::Vector2u pos = getIndex(_srcImages[i], pixelsSrc[j]);
					srcColors.back().index_put_({ col, Slice() }, embedding.get(_srcImages[i].getPixel(pos.x, pos.y)));
					srcPositions.back().index_put_({ col, Slice() }, torch::tensor({ float(pos.x) - origin.x, float(pos.y) -origin.y }));
				}

				samplers.emplace_back(pixelsSrc.size());
			}

			// network setup
			// starting guess for the translation is the center of each zone
			const float translationX = target.x;
			const float translationY = target.y;

			MultiLayerPerceptron net(MLPOptions(2)
				.total_time(1.0)
				.hidden_layers(1)
				.residual(true));
			{
				// set initial map from center to center
				torch::NoGradGuard noGrad;
				net->layers.back()->bias.index_put_({ Slice() }, torch::tensor({ translationX, translationY }));
				const float theta = 3.1415f / 4 * 0.f;
				const float cos = std::cos(theta);
				const float sin = std::sin(theta);
				net->layers.back()->weight.index_put_({ Slice() }, torch::tensor({ {cos, -sin},{sin, cos} }));
			}
			MultiLayerPerceptron bestNet = clone(net);
			float bestLoss = std::numeric_limits<float>::max();

			// training
			torch::optim::Adam optimizer(net->parameters(),
				torch::optim::AdamOptions(3e-3));
		//	torch::optim::SGD optimizer(net->parameters(),
		//		torch::optim::SGDOptions(5e-3).momentum(0.5));

			size_t batchSize = std::max(pixelsSrc.size() / 4, size_t(12));

		/*	torch::optim::LBFGS optimizer(net->parameters(),
				torch::optim::LBFGSOptions(1e-2)
					.max_iter(64)
					.history_size(256));*/

			const size_t maxEpochs = 200;
			for (size_t epoch = 0; epoch < maxEpochs; ++epoch)
			{
				net->train();
				net->zero_grad();
				for (auto& sampler : samplers)
					sampler.reset();
				float lossF = 0.f;
				while (auto batchInds = samplers[0].next(batchSize))
				{
					std::vector<std::vector<size_t>> batches;
					batches.emplace_back(std::move(*batchInds));
					for (size_t j = 1; j < numImgs; ++j)
						batches.emplace_back(std::move(*samplers[j].next(batchSize)));

					auto closure = [&]()
					{
						torch::Tensor loss = torch::zeros({ 1 });
						lossF = 0.f;
						torch::Tensor output = net->forward(srcPositions[0]);
						for (size_t j = 0; j < numImgs; ++j)
						{
							torch::Tensor targetPos = indexSlice(output, batches[j]);
							loss += torch::mse_loss(dstInterpolated[j].getPixels(targetPos),
								indexSlice(srcColors[j], batches[j]));
							
						}
						loss.backward();
						lossF += loss.item<float>();
						return loss;
					};
					optimizer.step(closure);
				}

				torch::NoGradGuard noGrad;
				net->eval();
				torch::Tensor lossValid = torch::zeros({ 1 });
				torch::Tensor dstPositions = net->forward(srcPositions[0]);
				for (size_t j = 0; j < numImgs; ++j)
				{
					lossValid += torch::mse_loss(dstValidation[j].getPixels(dstPositions), srcColors[j]);
				}
				const float lossValidF = lossValid.item<float>();
				if (lossValidF < bestLoss)
				{
					bestNet = clone(net);
					bestLoss = lossValidF;
				}

				std::cout << "Epoch: " << epoch << " loss: " << lossF
					<< " valid: " << lossValidF << "\n";

			/*	if (epoch > maxEpochs * 2 / 3)
				{
					batchSize = pixelsSrc.size();
				}*/
			}

			torch::NoGradGuard gradGuard;
			// discretize the function
			torch::Tensor source = bestNet->inverse(dstPositions);
			for (int64_t i = 0; i < source.sizes()[0]; ++i)
			{
				sf::Vector2f p(source.index({ i, 0 }).item<float>(), 
					source.index({ i, 1 }).item<float>());
				map(getIndex(_dstImages[0], pixelsDst[i])) = sf::Vector2u(p.x + origin.x, p.y + origin.y);
			}
		}

		return map;
	}

} // namespace nn