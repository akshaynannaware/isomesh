/* This file is part of Isomesh library, released under MIT license.
  Copyright (c) 2018-2019 Pavel Asyutchenko (sventeam@yandex.ru) */
#pragma once

#include "../common.hpp"

namespace isomesh
{

/** \brief Interface for Quadratic Error Function (QEF) minimizer
*/
class QefSolver3D {
public:
	/** \brief Adds a plane to the solver
	
		\param[in] normal Normal vector of the plane, must have unit length
		\param[in] point Any point belonging to the plane
	*/
	virtual void addPlane (glm::vec3 point, glm::vec3 normal) = 0;
	/** \brief Finds QEF minimizer
	
		Solution space is limited by a box [minPoint; maxPoint] to prevent 'spikes'
		in the isosurface. You may still return a value outside of this box, just
		remember of the possible consequences. In case of multiple solutions we advise
		to prefer the one closest to the 'mass point' (average of all points added
		through \ref addPlane). This makes the problem always have unique solution.
		\param[in] minPoint Lower bound of solution space
		\param[in] maxPoint Upper bound of solution space
		\return Point which minimizes QEF value
	*/
	virtual glm::vec3 solve (glm::vec3 minPoint, glm::vec3 maxPoint) = 0;
	/** \brief Evaluates QEF value at a given point

		\param[in] point Point where QEF needs to be evaluated
		\return Error value
	*/
	virtual float eval (glm::vec3 point) const = 0;
	/** \brief Resets solver to initial state
	*/
	virtual void reset () = 0;
};

class BaseQefSolver3D : public QefSolver3D {
public:
	virtual void addPlane (glm::vec3 point, glm::vec3 normal) override;
	virtual float eval (glm::vec3 point) const override;
	virtual void reset () override;

	const static int kMaxPlanes = 12;
protected:
	glm::vec3 m_normals[kMaxPlanes];
	float m_coefs[kMaxPlanes];
	/// Sum of added points
	glm::vec3 m_massPoint = glm::vec3 (0);
	int m_numPlanes = 0;
};

class GradientDescentQefSolver3D : public BaseQefSolver3D {
public:
	virtual glm::vec3 solve (glm::vec3 minPoint, glm::vec3 maxPoint) override;
	
	void setStepCount (int value) { m_stepCount = value; }
	void setGradStep (float value) { m_gradStep = value; }

protected:
	int m_stepCount = 10;
	float m_gradStep = 0.75f;
};

class QrQefSolver3D : public QefSolver3D {
public:
	struct QefData {
		// QEF decomposed matrix, only nonzero elements
		float a_11, a_12, a_13, b_1;
		float       a_22, a_23, b_2;
		float             a_33, b_3;
		float                    r2;
		// Sum of added points
		float mpx, mpy, mpz;
		// Added points count and feature dimension
		int16_t mp_cnt, dim;
	};

	QrQefSolver3D () noexcept;
	QrQefSolver3D (const QefData &data) noexcept;

	virtual void addPlane (glm::vec3 point, glm::vec3 normal) override;
	virtual glm::vec3 solve (glm::vec3 min_bound, glm::vec3 max_bound) override;
	virtual float eval (glm::vec3 point) const override;
	virtual void reset () override;

	void setTolerance (float value) { m_tolerance = value; }

	void merge (const QefData &data) noexcept;
	
	QefData data () noexcept;
	
protected:
	constexpr static int kRows = 8;
	// Column-major
	float A[4][kRows];
	glm::vec3 m_pointsSum;
	int m_addedPoints;
	int m_usedRows;
	int m_featureDim;
	float m_tolerance = 0.01f;

	void compressMatrix ();
};

}
