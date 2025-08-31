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
	const sf::Vector2u& _kernelSize,
	float _rotation)
	: KernelDistance(_src,_dst, Matrix<float>(_kernelSize,1.f), _rotation)
{}

KernelDistance::KernelDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const math::Matrix<float>& _kernel,
	float _rotation)
	: DistanceBase(_src, _dst),
	m_kernelHalSize(_kernel.size.x / 2, _kernel.size.y / 2),
	m_kernelWeights(_kernel),
	m_sampleCoords(_kernel.size),
	m_kernelSum(std::accumulate(m_kernelWeights.begin(), m_kernelWeights.end(), 0.f))
{
	const int kernelSizeX = m_kernelWeights.size.x;
	const int kernelSizeY = m_kernelWeights.size.y;
	for (int j = 0; j < kernelSizeY; ++j)
		for (int i = 0; i < kernelSizeX; ++i)
		{
			const float cos = std::cos(_rotation);
			const float sin = std::sin(_rotation);
			// signs for y get flipped during rotation since img coords start in the upper left
			const sf::Vector2f pos(i - static_cast<int>(m_kernelHalSize.x), 
				-(j - static_cast<int>(m_kernelHalSize.y)));
			const sf::Vector2f rotated(pos.x * cos - pos.y * sin, pos.x * sin + pos.y * cos);
			m_sampleCoords(i, j) = sf::Vector2i(std::round(rotated.x), -std::round(rotated.y));
		}
}

Matrix<float> KernelDistance::operator()(unsigned x, unsigned y) const
{
	auto distance = [](const std::pair<float,sf::Color>& c1, const sf::Color& c2)
	{
		return c1.second == c2 ? 0.f : c1.first;
	};

	auto sum = [this](const Matrix<float>& result)
	{
		return std::accumulate(result.begin(), result.end(), 0.f)
			/ m_kernelSum;
	};

	return applyConvolution(m_src, makeKernel(x, y), distance, sum);
}

KernelDistance::Kernel KernelDistance::makeKernel(unsigned x, unsigned y) const
{
	Kernel kernel(m_kernelWeights.size);

	for (unsigned j = 0; j < m_kernelWeights.size.y; ++j)
		for (unsigned i = 0; i < m_kernelWeights.size.x; ++i)
		{
			const int xInd = x + m_sampleCoords(i,j).x;
			const int yInd = y + m_sampleCoords(i,j).y;
			kernel(i, j) = std::make_pair(m_kernelWeights(i,j), getPixelPadded(m_dst, xInd, yInd));
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

// ************************************************************* //

constexpr float pi = 3.14159265f;
RotInvariantKernelDistance::RotInvariantKernelDistance(const sf::Image& _src,
	const sf::Image& _dst,
	const math::Matrix<float>& _kernel)
	: GroupMinDistance({ 
		KernelDistance(_src, _dst, _kernel, pi * 0.f),
		KernelDistance(_src, _dst, _kernel, pi * 0.25f),
		KernelDistance(_src, _dst, _kernel, pi * 0.5f),
		KernelDistance(_src, _dst, _kernel, pi * 0.75f),
		KernelDistance(_src, _dst, _kernel, pi * 1.f),
		KernelDistance(_src, _dst, _kernel, pi * 1.25f),
		KernelDistance(_src, _dst, _kernel, pi * 1.5f),
		KernelDistance(_src, _dst, _kernel, pi * 1.75f)})
{
}

// ************************************************************* //
MapDistance::MapDistance(const math::Matrix<sf::Vector2u>& _map)
	: m_map(_map)
{
	m_scaleFactor = 1.f / std::sqrt(static_cast<float>(math::dot(m_map.size, m_map.size)));
}

Matrix<float> MapDistance::operator()(unsigned x, unsigned y) const
{
	Matrix<float> distances(getSize());
	const sf::Vector2u origin = m_map(x, y);
	for (unsigned iy = 0; iy < m_map.size.y; ++iy)
		for(unsigned ix = 0; ix < m_map.size.x; ++ix)
		{
			const sf::Vector2u dist = sf::Vector2u(ix, iy) - origin;
			distances(ix, iy) = m_scaleFactor * std::sqrt(static_cast<float>(math::dot(dist, dist)));
		}

	return distances;
}