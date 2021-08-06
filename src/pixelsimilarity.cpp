#include "pixelsimilarity.hpp"
#include "math/convolution.hpp"
#include "math/vectorext.hpp"

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
KernelDistance::KernelDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const sf::Vector2u& _kernelSize)
	: DistanceBase(_src, _dst),
	m_kernelSize(_kernelSize),
	m_kernelHalSize(_kernelSize.x / 2, _kernelSize.y / 2)
{}

Matrix<float> KernelDistance::operator()(unsigned x, unsigned y) const
{
	auto distance = [](const sf::Color& c1, const sf::Color& c2)
	{
		return c1 == c2 ? 0.f : 1.f;
	};

	auto sum = [](const Matrix<float>& result)
	{
		return std::accumulate(result.begin(), result.end(), 0.f);
	};

	return applyConvolution(m_src, makeKernel(x, y), distance, sum);
}

Matrix<sf::Color> KernelDistance::makeKernel(unsigned x, unsigned y) const
{
	Matrix<sf::Color> kernel(m_kernelSize);

	for (unsigned i = 0; i < m_kernelSize.y; ++i)
		for (unsigned j = 0; j < m_kernelSize.x; ++j)
		{
			const int xInd = static_cast<int>(x + j) - m_kernelHalSize.x;
			const int yInd = static_cast<int>(y + i) - m_kernelHalSize.y;
			kernel(j, i) = getPixelPadded(m_dst, xInd, yInd);
		}

	return kernel;
};

// ************************************************************* //
BlurDistance::BlurDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const sf::Vector2u& _kernelSize)
	: DistanceBase(_src, _dst)
{
	const sf::Vector2u size = _src.getSize();

	const sf::Vector2i kernelHalf(_kernelSize.x / 2, _kernelSize.y / 2);
	Matrix<float> kernel(_kernelSize, 1.f);
	// the center pixel always takes precedence
	kernel(kernelHalf.x, kernelHalf.y) = 100.f;

	const float kernelSum = std::accumulate(kernel.begin(), kernel.end(), 0.f);

	auto sample = [](const float f, const sf::Color& color)
	{
		return f * toVec(color);
	};

	auto sum = [kernelSum](const Matrix<sf::Vector3f>& result)
	{
		return std::accumulate(result.begin(), result.end(), sf::Vector3f{})
			/ kernelSum;
	};

	m_dstBlurred = math::applyConvolution(_dst, kernel, sample, sum);
	m_srcBlurred = math::applyConvolution(_src, kernel, sample, sum);
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
