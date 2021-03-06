/* This file is part of Isomesh library, released under MIT license.
  Copyright (c) 2018-2019 Pavel Asyutchenko (sventeam@yandex.ru) */
#include <isomesh/data/grid.hpp>

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace isomesh
{

UniformGrid::UniformGrid (uint32_t size, const glm::dvec3 &globalPos, double gridStep) :
	m_size (size), m_halfSize (int32_t (size) / 2), m_globalPos (globalPos), m_gridStep (gridStep) {
	if (size < 2)
		throw std::invalid_argument ("Grid size should be at least two");
	if (size & (size - 1))
		throw std::invalid_argument ("Grid size is not a power of two");
	if (size > 1024)
		throw std::length_error ("Too large grid size (> 1024)");

	m_mat.reset (new Material[(size + 1) * (size + 1) * (size + 1)]);
}

void UniformGrid::fill (const ScalarField &f, const ZeroFinder &solver) {
	std::vector<double> values (dataSize ());
	// Compute function values over the whole grid
	uint32_t idx = 0;
	glm::dvec3 lowest_point = localToGlobal (glm::dvec3 (-m_halfSize));
	glm::dvec3 call_pos = lowest_point;
	for (int32_t y = -m_halfSize; y <= m_halfSize; y++) {
		call_pos.x = lowest_point.x;
		for (int32_t x = -m_halfSize; x <= m_halfSize; x++) {
			call_pos.z = lowest_point.z;
			for (int32_t z = -m_halfSize; z <= m_halfSize; z++) {
				values[idx] = f (call_pos);
				if (values[idx] > 0)
					m_mat[idx] = Material::Empty;
				else
					m_mat[idx] = f.material (call_pos, values[idx]);
				idx++;
				call_pos.z += m_gridStep;
			}
			call_pos.x += m_gridStep;
		}
		call_pos.y += m_gridStep;
	}
	/* Find zero intersections on grid edges.
	 Different signs on edge endpoints means there is at least
	 one zero intersection on this edge. We assume that there is
	 exactly one and find it using the provided solver. */
	// Along X
	m_edgeX.clear ();
	uint32_t idx1 = 0;
	uint32_t idx2 = m_size + 1;
	for (int32_t y = -m_halfSize; y <= m_halfSize; y++) {
		for (int32_t x = -m_halfSize; x < m_halfSize; x++) { // x < -m_halfSize, this is intended
			for (int32_t z = -m_halfSize; z <= m_halfSize; z++) {
				bool sign1 = (values[idx1] <= 0.0);
				bool sign2 = (values[idx2] <= 0.0);
				if (sign1 != sign2) {
					glm::dvec3 p = localToGlobal (glm::dvec3 (x, y, z));
					double x0 = p.x;
					double x1 = p.x + m_gridStep;
					p.x = solver.findAlongX (x0, p.y, p.z, x1, values[idx1], values[idx2], f);
					glm::dvec3 grad = f.grad (p);
					double offset = (p.x - x0) / m_gridStep;
					Material mat = sign1 ? m_mat[idx1] : m_mat[idx2];
					m_edgeX.addEdge (x, y, z, grad, offset, 0, sign1, mat);
				}
				idx1++;
				idx2++;
			}
		}
		idx1 += m_size + 1;
		idx2 += m_size + 1;
	}
	// Along Y
	m_edgeY.clear ();
	idx1 = 0;
	idx2 = (m_size + 1) * (m_size + 1);
	for (int32_t y = -m_halfSize; y < m_halfSize; y++) { // y < -m_halfSize, this is intended
		for (int32_t x = -m_halfSize; x <= m_halfSize; x++) {
			for (int32_t z = -m_halfSize; z <= m_halfSize; z++) {
				bool sign1 = (values[idx1] <= 0.0);
				bool sign2 = (values[idx2] <= 0.0);
				if (sign1 != sign2) {
					glm::dvec3 p = localToGlobal (glm::dvec3 (x, y, z));
					double y0 = p.y;
					double y1 = p.y + m_gridStep;
					p.y = solver.findAlongY (p.x, y0, p.z, y1, values[idx1], values[idx2], f);
					glm::dvec3 grad = f.grad (p);
					double offset = (p.y - y0) / m_gridStep;
					Material mat = sign1 ? m_mat[idx1] : m_mat[idx2];
					m_edgeY.addEdge (x, y, z, grad, offset, 1, sign1, mat);
				}
				idx1++;
				idx2++;
			}
		}
	}
	// Along Z
	m_edgeZ.clear ();
	idx1 = 0;
	idx2 = 1;
	for (int32_t y = -m_halfSize; y <= m_halfSize; y++) {
		for (int32_t x = -m_halfSize; x <= m_halfSize; x++) {
			for (int32_t z = -m_halfSize; z < m_halfSize; z++) { // z < -m_halfSize, this is intended
				bool sign1 = (values[idx1] <= 0.0);
				bool sign2 = (values[idx2] <= 0.0);
				if (sign1 != sign2) {
					glm::dvec3 p = localToGlobal (glm::dvec3 (x, y, z));
					double z0 = p.z;
					double z1 = p.z + m_gridStep;
					p.z = solver.findAlongZ (p.x, p.y, z0, z1, values[idx1], values[idx2], f);
					glm::dvec3 grad = f.grad (p);
					double offset = (p.z - z0) / m_gridStep;
					Material mat = sign1 ? m_mat[idx1] : m_mat[idx2];
					m_edgeZ.addEdge (x, y, z, grad, offset, 2, sign1, mat);
				}
				idx1++;
				idx2++;
			}
			idx1++;
			idx2++;
		}
	}
}

Material UniformGrid::at (int32_t x, int32_t y, int32_t z) const {
	assert (isVertexInGrid ({ x, y, z }));
	return m_mat[pointToIndex (x, y, z)];
}

Material UniformGrid::operator [] (const glm::ivec3 &v) const {
	return at (v.x, v.y, v.z);
}

// ---------------------------------------
// -------------- NEW API ----------------
// ---------------------------------------

uint32_t UniformGrid::pointToIndex (int32_t x, int32_t y, int32_t z) const noexcept {
	assert (isVertexInGrid ({ x, y, z }));
	uint32_t idx = uint32_t (y + m_halfSize) * (m_size + 1);
	idx = (idx + uint32_t (x + m_halfSize)) * (m_size + 1);
	idx += uint32_t (z + m_halfSize);
	return idx;
}

glm::ivec3 UniformGrid::indexToPoint (uint32_t idx) const noexcept {
	assert (idx < (m_size + 1) * (m_size + 1) * (m_size + 1));
	int32_t z = int32_t (idx % (m_size + 1)) - m_halfSize;
	idx /= (m_size + 1);
	int32_t x = int32_t (idx % (m_size + 1)) - m_halfSize;
	idx /= (m_size + 1);
	int32_t y = int32_t (idx) - m_halfSize;
	return glm::ivec3 (x, y, z);
}

// -------------------------
// Operations on vertices
// -------------------------

bool UniformGrid::isVertexInGrid (int32_t x, int32_t y, int32_t z) const noexcept {
	if (glm::abs (x) > m_halfSize)
		return false;
	if (glm::abs (y) > m_halfSize)
		return false;
	if (glm::abs (z) > m_halfSize)
		return false;
	return true;
}

bool UniformGrid::isVertexOnBorder (int32_t x, int32_t y, int32_t z) const noexcept {
	if (glm::abs (x) == m_halfSize)
		return true;
	if (glm::abs (y) == m_halfSize)
		return true;
	if (glm::abs (z) == m_halfSize)
		return true;
	return false;
}

// -----------------------
// Operations on edges
// -----------------------

template<>
bool UniformGrid::isEdgeInGrid<0> (const glm::ivec3 &edgePos) const noexcept {
	if (edgePos.x < -m_halfSize || edgePos.x >= m_halfSize)
		return false;
	if (glm::abs (edgePos.y) > m_halfSize)
		return false;
	if (glm::abs (edgePos.z) > m_halfSize)
		return false;
	return true;
}

template<>
bool UniformGrid::isEdgeInGrid<1> (const glm::ivec3 &edgePos) const noexcept {
	if (edgePos.y < -m_halfSize || edgePos.y >= m_halfSize)
		return false;
	if (glm::abs (edgePos.x) > m_halfSize)
		return false;
	if (glm::abs (edgePos.z) > m_halfSize)
		return false;
	return true;
}

template<>
bool UniformGrid::isEdgeInGrid<2> (const glm::ivec3 &edgePos) const noexcept {
	if (edgePos.z < -m_halfSize || edgePos.z >= m_halfSize)
		return false;
	if (glm::abs (edgePos.x) > m_halfSize)
		return false;
	if (glm::abs (edgePos.y) > m_halfSize)
		return false;
	return true;
}

template<>
bool UniformGrid::isEdgeOnBorder<0> (const glm::ivec3 &edgePos) const noexcept {
	if (glm::abs (edgePos.y) == m_halfSize)
		return true;
	if (glm::abs (edgePos.z) == m_halfSize)
		return true;
	return false;
}

template<>
bool UniformGrid::isEdgeOnBorder<1> (const glm::ivec3 &edgePos) const noexcept {
	if (glm::abs (edgePos.x) == m_halfSize)
		return true;
	if (glm::abs (edgePos.z) == m_halfSize)
		return true;
	return false;
}

template<>
bool UniformGrid::isEdgeOnBorder<2> (const glm::ivec3 &edgePos) const noexcept {
	if (glm::abs (edgePos.x) == m_halfSize)
		return true;
	if (glm::abs (edgePos.y) == m_halfSize)
		return true;
	return false;
}

template<>
std::array<uint32_t, 4> UniformGrid::adjacentCellsForEdge<0> (const glm::ivec3 &edgePos) const noexcept {
	const uint32_t dy = (m_size + 1) * (m_size + 1);
	const uint32_t dz = 1;
	std::array<uint32_t, 4> cells;
	cells[2] = pointToIndex (edgePos);
	cells[0] = cells[2] - dy - dz;
	cells[1] = cells[2] - dz;
	cells[3] = cells[2] - dy;
	if (edgePos.y == -m_halfSize)
		cells[0] = cells[3] = kBadIndex;
	if (edgePos.y == m_halfSize)
		cells[1] = cells[2] = kBadIndex;
	if (edgePos.z == -m_halfSize)
		cells[0] = cells[1] = kBadIndex;
	if (edgePos.z == m_halfSize)
		cells[2] = cells[3] = kBadIndex;
	return cells;
}

template<>
std::array<uint32_t, 4> UniformGrid::adjacentCellsForEdge<1> (const glm::ivec3 &edgePos) const noexcept {
	const uint32_t dx = (m_size + 1);
	const uint32_t dz = 1;
	std::array<uint32_t, 4> cells;
	cells[2] = pointToIndex (edgePos);
	cells[0] = cells[2] - dx - dz;
	cells[1] = cells[2] - dx;
	cells[3] = cells[2] - dz;
	if (edgePos.x == -m_halfSize)
		cells[0] = cells[1] = kBadIndex;
	if (edgePos.x == m_halfSize)
		cells[2] = cells[3] = kBadIndex;
	if (edgePos.z == -m_halfSize)
		cells[0] = cells[3] = kBadIndex;
	if (edgePos.z == m_halfSize)
		cells[1] = cells[2] = kBadIndex;
	return cells;
}

template<>
std::array<uint32_t, 4> UniformGrid::adjacentCellsForEdge<2> (const glm::ivec3 &edgePos) const noexcept {
	const uint32_t dx = (m_size + 1);
	const uint32_t dy = (m_size + 1) * (m_size + 1);
	std::array<uint32_t, 4> cells;
	cells[2] = pointToIndex (edgePos);
	cells[0] = cells[2] - dx - dy;
	cells[1] = cells[2] - dy;
	cells[3] = cells[2] - dx;
	if (edgePos.x == -m_halfSize)
		cells[0] = cells[3] = kBadIndex;
	if (edgePos.x == m_halfSize)
		cells[1] = cells[2] = kBadIndex;
	if (edgePos.y == -m_halfSize)
		cells[0] = cells[1] = kBadIndex;
	if (edgePos.y == m_halfSize)
		cells[2] = cells[3] = kBadIndex;
	return cells;
}

// -----------------------
// Operations on faces
// -----------------------

template<>
bool UniformGrid::isFaceInGrid<0> (const glm::ivec3 &facePos) const noexcept {
	if (glm::abs (facePos.x) > m_halfSize)
		return false;
	if (facePos.y < -m_halfSize || facePos.y >= m_halfSize)
		return false;
	if (facePos.z < -m_halfSize || facePos.z >= m_halfSize)
		return false;
	return true;
}

template<>
bool UniformGrid::isFaceInGrid<1> (const glm::ivec3 &facePos) const noexcept {
	if (glm::abs (facePos.y) > m_halfSize)
		return false;
	if (facePos.x < -m_halfSize || facePos.x >= m_halfSize)
		return false;
	if (facePos.z < -m_halfSize || facePos.z >= m_halfSize)
		return false;
	return true;
}

template<>
bool UniformGrid::isFaceInGrid<2> (const glm::ivec3 &facePos) const noexcept {
	if (glm::abs (facePos.z) > m_halfSize)
		return false;
	if (facePos.x < -m_halfSize || facePos.x >= m_halfSize)
		return false;
	if (facePos.y < -m_halfSize || facePos.y >= m_halfSize)
		return false;
	return true;
}

template<>
bool UniformGrid::isFaceOnBorder<0> (const glm::ivec3 &facePos) const noexcept {
	if (glm::abs (facePos.x) == m_halfSize)
		return true;
	return false;
}

template<>
bool UniformGrid::isFaceOnBorder<1> (const glm::ivec3 &facePos) const noexcept {
	if (glm::abs (facePos.y) == m_halfSize)
		return true;
	return false;
}

template<>
bool UniformGrid::isFaceOnBorder<2> (const glm::ivec3 &facePos) const noexcept {
	if (glm::abs (facePos.z) == m_halfSize)
		return true;
	return false;
}

// -----------------------
// Operations on cells
// -----------------------

bool UniformGrid::isCellInGrid (const glm::ivec3 &cellPos) const noexcept {
	if (cellPos.x < -m_halfSize || cellPos.y < -m_halfSize || cellPos.z < -m_halfSize)
		return false;
	if (cellPos.x >= m_halfSize || cellPos.y >= m_halfSize || cellPos.z >= m_halfSize)
		return false;
	return true;
}

bool UniformGrid::isCellOnBorder (const glm::ivec3 &cellPos) const noexcept {
	if (cellPos.x == -m_halfSize || cellPos.y == -m_halfSize || cellPos.z == -m_halfSize)
		return true;
	if (cellPos.x == m_halfSize - 1 || cellPos.y == m_halfSize - 1 || cellPos.z == m_halfSize - 1)
		return true;
	return false;
}

std::array<uint32_t, 8> UniformGrid::adjacentVerticesForCell (uint32_t cellIdx) const noexcept {
	const uint32_t dx = (m_size + 1);
	const uint32_t dy = (m_size + 1) * (m_size + 1);
	const uint32_t dz = 1;
	std::array<uint32_t, 8> vertices;
	vertices[0] = cellIdx;
	vertices[1] = cellIdx + dz;
	vertices[2] = cellIdx + dx;
	vertices[3] = cellIdx + dx + dz;
	vertices[4] = cellIdx + dy;
	vertices[5] = cellIdx + dy + dz;
	vertices[6] = cellIdx + dx + dy;
	vertices[7] = cellIdx + dx + dy + dz;
	return vertices;
}

std::array<Material, 8> UniformGrid::materialsOfCell (uint32_t cellIdx) const noexcept {
	const uint32_t dx = (m_size + 1);
	const uint32_t dy = (m_size + 1) * (m_size + 1);
	const uint32_t dz = 1;
	std::array<Material, 8> mats;
	mats[0] = m_mat[cellIdx];
	mats[1] = m_mat[cellIdx + dz];
	mats[2] = m_mat[cellIdx + dx];
	mats[3] = m_mat[cellIdx + dx + dz];
	mats[4] = m_mat[cellIdx + dy];
	mats[5] = m_mat[cellIdx + dy + dz];
	mats[6] = m_mat[cellIdx + dx + dy];
	mats[7] = m_mat[cellIdx + dx + dy + dz];
	return mats;
}

}
