/* This file is part of Isomesh library, released under MIT license.
  Copyright (c) 2018 Pavel Asyutchenko (sventeam@yandex.ru) */
/** \file
	\brief Common definitions used across all Isomesh library
*/
#pragma once

#include <cstdint>
#include <functional>

#include <glm/glm.hpp>

namespace isomesh
{

/** \brief Isosurface function wrapper

	This is a thin wrapper structure that packs together an isosurface defining
	function and its gradient. An isosurface function should have these properties:
	- Be continuous over R^3
	- Have its gradient (or at least some approximation) defined everywhere
	- Have negative sign inside the surface and positive outside
*/
struct SurfaceFunction {
	std::function<double (glm::dvec3)> f;
	std::function<glm::dvec3 (glm::dvec3)> grad;
	double operator () (glm::dvec3 p) const { return f (p); }
};

/** \brief Voxel materials enumeration

	Empty = air
*/
enum class Material : uint8_t {
	// No voxel (i.e. air)
	Empty = 0,
	Stone,
	Soil,

	// Not a material
	Count
};

}

// std::get<> and tuple interface for GLM vectors
namespace std
{

template<size_t I, int D, typename T> constexpr
T &get (glm::vec<D, T, glm::defaultp> &v) noexcept {
	static_assert (I < size_t (D), "Index should be less than vector dimension");
	static_assert (D >= 1 && D <= 4, "Dimension should be not higher than 4");
	if constexpr (I == 0)
		return v.x;
	else if constexpr (I == 1)
		return v.y;
	else if constexpr (I == 2)
		return v.z;
	else
		return v.w;
}

template<size_t I, int D, typename T> constexpr
T &&get (glm::vec<D, T, glm::defaultp> &&v) noexcept {
	static_assert (I < size_t (D), "Index should be less than vector dimension");
	static_assert (D >= 1 && D <= 4, "Dimension should be not higher than 4");
	if constexpr (I == 0)
		return std::move (v.x);
	else if constexpr (I == 1)
		return std::move (v.y);
	else if constexpr (I == 2)
		return std::move (v.z);
	else
		return std::move (v.w);
}

template<size_t I, int D, typename T> constexpr
const T &get (const glm::vec<D, T, glm::defaultp> &v) noexcept {
	static_assert (I < size_t (D), "Index should be less than vector dimension");
	static_assert (D >= 1 && D <= 4, "Dimension should be not higher than 4");
	if constexpr (I == 0)
		return v.x;
	else if constexpr (I == 1)
		return v.y;
	else if constexpr (I == 2)
		return v.z;
	else
		return v.w;
}

template<int D, typename T>
struct tuple_size<glm::vec<D, T, glm::defaultp> > :
	std::integral_constant<size_t, size_t (D)> {};

template<size_t I, int D, typename T>
struct tuple_element<I, glm::vec<D, T, glm::defaultp> > {
	using type = T;
};

}
