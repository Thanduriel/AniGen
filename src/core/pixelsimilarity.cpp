#include "pixelsimilarity.hpp"
#include "../math/convolution.hpp"
#include "../math/vectorext.hpp"

#include <numeric>
#include <cassert>

using namespace math;

DistanceBase::DistanceBase(const sf::Image& _src, const sf::Image& _dst)
	: m_src(_src),
	m_dst(_dst)
{
	assert(m_src.getSize() == m_dst.getSize());
}

// ************************************************************* //
IdentityDistance::IdentityDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const math::Matrix<float>&)
	: DistanceBase(_src, _dst)
{
}

Matrix<float> IdentityDistance::operator()(unsigned x, unsigned y) const
{
	const sf::Vector2u size = getSize();
	Matrix<float> distances(size);
	const sf::Color dstColor = m_dst.getPixel(x, y);

	for (unsigned iy = 0; iy < size.y; ++iy)
		for (unsigned ix = 0; ix < size.x; ++ix)
			distances(ix, iy) = dstColor == m_src.getPixel(ix, iy) ? 0.f : 1.f;

	return distances;
}

// ************************************************************* //
KernelDistance::KernelDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const sf::Vector2u& _kernelSize)
	: KernelDistance(_src,_dst, Matrix<float>(_kernelSize,1.f))
{}

KernelDistance::KernelDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const math::Matrix<float>& _kernel)
	: DistanceBase(_src, _dst),
	m_kernelWeights(_kernel),
	m_kernelHalSize(_kernel.size.x / 2, _kernel.size.y / 2)
{}

Matrix<float> KernelDistance::operator()(unsigned x, unsigned y) const
{
	auto distance = [](const std::pair<float,sf::Color>& c1, const sf::Color& c2)
	{
		return c1.second == c2 ? 0.f : c1.first;
	};

	auto sum = [](const Matrix<float>& result)
	{
		return std::accumulate(result.begin(), result.end(), 0.f);
	};

	return applyConvolution(m_src, makeKernel(x, y), distance, sum);
}

KernelDistance::Kernel KernelDistance::makeKernel(unsigned x, unsigned y) const
{
	Kernel kernel(m_kernelWeights.size);

	for (unsigned i = 0; i < m_kernelWeights.size.y; ++i)
		for (unsigned j = 0; j < m_kernelWeights.size.x; ++j)
		{
			const int xInd = static_cast<int>(x + j) - m_kernelHalSize.x;
			const int yInd = static_cast<int>(y + i) - m_kernelHalSize.y;
			kernel(j, i) = std::make_pair(m_kernelWeights(j,i), getPixelPadded(m_dst, xInd, yInd));
		}

	return kernel;
};

// ************************************************************* //
BlurDistance::BlurDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const sf::Vector2u& _kernelSize)
	: BlurDistance(_src, _dst, Matrix<float>(_kernelSize,1.f))
{
}

BlurDistance::BlurDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const math::Matrix<float>& _kernel)
	: DistanceBase(_src, _dst)
{
	const sf::Vector2u size = _src.getSize();

	const sf::Vector2u kernelHalf(_kernel.size.x / 2, _kernel.size.y / 2);
	const float kernelSum = std::accumulate(_kernel.begin(), _kernel.end(), 0.f);

	auto sample = [](const float f, const sf::Color& color)
	{
		return f * toVec(color);
	};

	auto sum = [kernelSum](const Matrix<sf::Vector3f>& result)
	{
		return std::accumulate(result.begin(), result.end(), sf::Vector3f{})
			/ kernelSum;
	};

	m_dstBlurred = math::applyConvolution(_dst, _kernel, sample, sum);
	m_srcBlurred = math::applyConvolution(_src, _kernel, sample, sum);
}

Matrix<float> BlurDistance::operator()(unsigned x, unsigned y) const
{
	Matrix<float> distances(getSize());

	const sf::Vector3f color = m_dstBlurred(x, y);

	for (size_t i = 0; i < m_srcBlurred.elements.size(); ++i)
	{
		distances[i] = math::distSq(m_srcBlurred[i], color);
	}
	return distances;
}
