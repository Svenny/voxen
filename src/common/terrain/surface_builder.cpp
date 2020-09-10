#include <voxen/common/terrain/surface_builder.hpp>

#include <voxen/util/log.hpp>

#include <array>
#include <algorithm>
#include <limits>
#include <vector>
#include <cstring>

enum Material : uint8_t {Empty, Solid};

static const glm::ivec3 kCellCornerOffset[8] = {
	{ 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 1 },
	{ 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 0 }, { 1, 1, 1 }
};

namespace jacobi_detail
{

// Faster but less accurate (Jacobi does not fully converge with this)
template<typename T>
std::pair<T, T> calcSinCosFast (T tan_twophi) {
	T inv = T (0.5) / hypot (T (1), tan_twophi);
	T c = sqrt (T (0.5) + inv);
	T s = copysign (sqrt (T (0.5) - inv), tan_twophi);
	return { s, c };
}

// More accurate but much slower
template<typename T>
std::pair<T, T> calcSinCosAccurate (T tan_twophi) {
	T phi = T (0.5) * atan (tan_twophi);
	return { sin (phi), cos (phi) };
}

template<typename T>
void rotate (glm::mat<3, 3, T> &A, glm::mat<3, 3, T> &H, T c, T s, int i, int j) {
#define jacobiRotate(i, j, k) { \
	float aik = A[glm::min (i, k)][glm::max (i, k)]; \
	float ajk = A[glm::min (j, k)][glm::max (j, k)]; \
	A[glm::min (i, k)][glm::max (i, k)] = c * aik + s * ajk; \
	A[glm::min (j, k)][glm::max (j, k)] = -s * aik + c * ajk; \
}
	if (i == 0 && j == 1) {
		jacobiRotate (0, 1, 2);
	}
	else if (i == 0 && j == 2) {
		jacobiRotate (0, 2, 1);
	}
	else { // (i == 1 && j == 2), other cases are impossible
		jacobiRotate (1, 2, 0);
	}
#undef jacobiRotate
	float aii = A[i][i];
	float ajj = A[j][j];
	float aij = A[i][j];
	A[i][i] = c * c * aii + s * s * ajj + c * s * (aij + aij);
	A[j][j] = s * s * aii + c * c * ajj - c * s * (aij + aij);
	A[i][j] = 0;
	for (int k = 0; k < 3; k++) {
		float hik = H[i][k];
		float hjk = H[j][k];
		H[i][k] = c * hik + s * hjk;
		H[j][k] = -s * hik + c * hjk;
	}
}

template<typename T>
void rotate (glm::mat<4, 4, T> &A, glm::mat<4, 4, T> &H, T c, T s, int i, int j) {
#define jacobiRotate(i, j, k) { \
	float aik = A[glm::min (i, k)][glm::max (i, k)]; \
	float ajk = A[glm::min (j, k)][glm::max (j, k)]; \
	A[glm::min (i, k)][glm::max (i, k)] = c * aik + s * ajk; \
	A[glm::min (j, k)][glm::max (j, k)] = -s * aik + c * ajk; \
}
	if (i == 0 && j == 1) {
		jacobiRotate (0, 1, 2);
		jacobiRotate (0, 1, 3);
	}
	else if (i == 0 && j == 2) {
		jacobiRotate (0, 2, 1);
		jacobiRotate (0, 2, 3);
	}
	else if (i == 0 && j == 3) {
		jacobiRotate (0, 3, 1);
		jacobiRotate (0, 3, 2);
	}
	else if (i == 1 && j == 2) {
		jacobiRotate (1, 2, 0);
		jacobiRotate (1, 2, 3);
	}
	else if (i == 1 && j == 3) {
		jacobiRotate (1, 3, 0);
		jacobiRotate (1, 3, 2);
	}
	else { // (i == 2 && j == 3), other cases are impossible
		jacobiRotate (2, 3, 0);
		jacobiRotate (2, 3, 1);
	}
#undef jacobiRotate
	float aii = A[i][i];
	float ajj = A[j][j];
	float aij = A[i][j];
	A[i][i] = c * c * aii + s * s * ajj + c * s * (aij + aij);
	A[j][j] = s * s * aii + c * c * ajj - c * s * (aij + aij);
	A[i][j] = 0;
	for (int k = 0; k < 4; k++) {
		float hik = H[i][k];
		float hjk = H[j][k];
		H[i][k] = c * hik + s * hjk;
		H[j][k] = -s * hik + c * hjk;
	}
}

}

template<typename T, int D>
std::pair<glm::vec<D, T>, glm::mat<D, D, T> > jacobi (glm::mat<D, D, T> A, T tolerance,
                                                      int max_iters, bool use_fast_sincos) {
	using namespace jacobi_detail;
	glm::mat<D, D, T> E { T (1) };
	for (int k = 0; k < max_iters; k++) {
		T max_el = std::abs (A[0][1]);
		int max_i = 0;
		int max_j = 1;
		for (int i = 0; i < D; i++) {
			for (int j = i + 1; j < D; j++) {
				T el = std::abs (A[i][j]);
				if (el > max_el) {
					max_el = el;
					max_i = i;
					max_j = j;
				}
			}
		}
		if (max_el <= tolerance)
			break;
		int i = max_i;
		int j = max_j;
		T tan_twophi = (A[i][j] + A[i][j]) / (A[i][i] - A[j][j]);
		T s, c;
		if (use_fast_sincos)
			std::tie (s, c) = calcSinCosFast (tan_twophi);
		else
			std::tie (s, c) = calcSinCosAccurate (tan_twophi);
		rotate<T> (A, E, c, s, i, j);
	}
	glm::vec<D, T> e;
	for (int i = 0; i < D; i++)
		e[i] = A[i][i];
	return { e, E };
}

class QefSolver3D {
public:
	/** \brief QEF solver state for external storage

		This struct holds solver's internal state in compact form, suitable
		for storage in octree nodes. You may use this struct to merge QEF's
		together or to initialize another solver.
		\note Solver's tunable options, such as tolerance or maximal number
		of iterations, are not preserved. You need to used some other way
		if you plan to store these options externally.
	*/
	struct State {
		// Compressed matrix, only nonzero elements
		float a_11, a_12, a_13, b_1;
		float       a_22, a_23, b_2;
		float             a_33, b_3;
		float                    r2;
		// Sum of added points
		float mpx, mpy, mpz;
		// Added points count and feature dimension
		// Using bit fields to fit them in four bytes, saving other four
		uint32_t mp_cnt : 30;
		uint32_t dim : 2;
	};

	QefSolver3D () noexcept;
	QefSolver3D (const State &data) noexcept;
	/** \brief Resets solver to its initial state
	*/
	void reset () noexcept;
	/** \brief Adds data from external storage to the solver state

		\param[in] data Data to be added
	*/
	void merge (const State &data) noexcept;
	/** \brief Obtains solver state for external storage

		\return Solver state in compact form
	*/
	State state () noexcept;
	/** \brief Adds a plane to the solver

		\param[in] normal Normal vector of the plane, must have unit length
		\param[in] point Any point belonging to the plane
	*/
	void addPlane (glm::vec3 point, glm::vec3 normal) noexcept;
	/** \brief Evaluates QEF value at a given point

		\param[in] point Point where QEF needs to be evaluated
		\return Error value
	*/
	float eval (glm::vec3 point) const noexcept;
	/** \brief Finds QEF minimizer

		Solution space is bounded by an axis-aligned box [minPoint; maxPoint]. An
		implementation may still return a value outside of this box, but this can break
		invariants in some algorithms. In case of multiple solutions we advise to prefer
		the one closest to the 'mass point' (centroid of all points added using
		\ref addPlane), making the problem always have a unique solution. This is not
		mandatory though.
		\param[in] minPoint Lower bound of solution space
		\param[in] maxPoint Upper bound of solution space
		\return Point which minimizes QEF value
	*/
	glm::vec3 solve (glm::vec3 min_point, glm::vec3 max_point);

	float pinvTolerance () const noexcept { return m_pinvTolerance; }
	float jacobiTolerance () const noexcept { return m_jacobiTolerance; }
	int maxJacobiIters () const noexcept { return m_maxJacobiIters; }
	bool fastFormulasUsed () const noexcept { return m_useFastFormulas; }

	void setPinvTolerance (float value) noexcept { m_pinvTolerance = glm::max (0.0f, value); }
	void setJacobiTolerance (float value) noexcept { m_jacobiTolerance = glm::max (0.0f, value); }
	void setMaxJacobiIters (int value) noexcept { m_maxJacobiIters = glm::max (1, value); }
	void useFastFormulas (bool value) noexcept { m_useFastFormulas = value; }

private:
	/// Maximal number of used rows
	constexpr static int kMaxRows = 8;
	/** \brief Column-major matrix A* = (A b)
	*/
	float A[4][kMaxRows];
	/// Count of matrix rows occupied with useful data
	int m_usedRows;
	/// Algebraic sum of points added to the solver
	glm::vec3 m_pointsSum;
	/// Count of points added to the solver
	uint32_t m_pointsCount;
	/** \brief Feature dimension

		Minimizer space is a set of all points in solution space where the QEF value
		reaches its minimum (i.e. points which may be returned by \ref solve call).
		Feature dimension is defined as three minus dimension of minimizer space (three
		minus the number of singular values truncated to zero during computing the
		pseudoinverse matrix).
	*/
	uint32_t m_featureDim;
	/// Singular values with absolute value less than tolerance will be truncated to zero
	float m_pinvTolerance = 0.01f;
	/// Stopping condition in Jacobi eigenvalue algorithm
	float m_jacobiTolerance = 0.0025f;
	/// Maximal number of Jacobi eigenvalue algorithm iterations
	int m_maxJacobiIters = 15;
	/// Whether to use more accurate or faster formulae in Jacobi eigenvalue algorithm
	bool m_useFastFormulas = true;

	void compressMatrix () noexcept;
};

QefSolver3D::QefSolver3D () noexcept {
	reset ();
}

QefSolver3D::QefSolver3D (const State &data) noexcept {
	reset ();
	merge (data);
}

void QefSolver3D::reset () noexcept {
	memset (A, 0, sizeof (A));
	m_pointsSum = glm::vec3 { 0 };
	m_pointsCount = 0;
	m_usedRows = 0;
	m_featureDim = 0;
}

void QefSolver3D::merge (const State &data) noexcept {
	// Mergee is of higher dimension, our mass point may be dismissed
	if (data.dim > m_featureDim) {
		m_featureDim = data.dim;
		m_pointsSum = glm::vec3 (data.mpx, data.mpy, data.mpz);
		m_pointsCount = data.mp_cnt;
	}
	// Mergee is of the same dimension, add mass points together
	else if (data.dim == m_featureDim) {
		m_pointsSum += glm::vec3 (data.mpx, data.mpy, data.mpz);
		m_pointsCount += data.mp_cnt;
	}
	// When mergee is of lesser dimension its mass point may be dismissed
	// We need four free rows
	if (m_usedRows > kMaxRows - 4)
		compressMatrix ();
	int id = m_usedRows;
	A[0][id] = data.a_11; A[1][id] = data.a_12; A[2][id] = data.a_13; A[3][id] = data.b_1;
	id++;
	A[0][id] =         0; A[1][id] = data.a_22; A[2][id] = data.a_23; A[3][id] = data.b_2;
	id++;
	A[0][id] =         0; A[1][id] =         0; A[2][id] = data.a_33; A[3][id] = data.b_3;
	id++;
	A[0][id] =         0; A[1][id] =         0; A[2][id] =         0; A[3][id] =  data.r2;
	m_usedRows += 4;
}

QefSolver3D::State QefSolver3D::state () noexcept {
	compressMatrix ();
	State data;
	data.a_11 = A[0][0]; data.a_12 = A[1][0]; data.a_13 = A[2][0]; data.b_1 = A[3][0];
	data.a_22 = A[1][1]; data.a_23 = A[2][1]; data.b_2 = A[3][1];
	data.a_33 = A[2][2]; data.b_3 = A[3][2];
	data.r2 = A[3][3];
	data.mpx = m_pointsSum.x;
	data.mpy = m_pointsSum.y;
	data.mpz = m_pointsSum.z;
	// Is it really safe to assume nobody will ever add 2^30 points? :P
	assert (m_pointsCount < (1 << 30));
	data.mp_cnt = m_pointsCount;
	data.dim = m_featureDim;
	return data;
}

void QefSolver3D::addPlane (glm::vec3 point, glm::vec3 normal) noexcept {
	if (m_usedRows == kMaxRows)
		compressMatrix ();
	int id = m_usedRows;
	A[0][id] = normal.x;
	A[1][id] = normal.y;
	A[2][id] = normal.z;
	A[3][id] = glm::dot (normal, point);
	m_usedRows++;
	m_pointsSum += point;
	m_pointsCount++;
}

float QefSolver3D::eval (glm::vec3 point) const noexcept {
	float error = 0;
	for (int i = 0; i < m_usedRows; i++) {
		float dot = A[0][i] * point.x + A[1][i] * point.y + A[2][i] * point.z;
		float diff = dot - A[3][i];
		error += diff * diff;
	}
	return error;
}

glm::vec3 QefSolver3D::solve (glm::vec3 min_point, glm::vec3 max_point) {
	compressMatrix ();
	glm::mat3 AT, ATA;
	{
		glm::mat3 M;
		M[0] = glm::vec3 (A[0][0], A[0][1], A[0][2]);
		M[1] = glm::vec3 (A[1][0], A[1][1], A[1][2]);
		M[2] = glm::vec3 (A[2][0], A[2][1], A[2][2]);
		AT = glm::transpose (M);
		ATA = AT * M;
	}
	auto[e, E] = jacobi (ATA, m_jacobiTolerance, m_maxJacobiIters, m_useFastFormulas);
	glm::mat3 sigma { 0.0f };
	m_featureDim = 3;
	for (int i = 0; i < 3; i++) {
		if (std::abs (e[i]) >= m_pinvTolerance)
			sigma[i][i] = 1.0f / e[i];
		else m_featureDim--;
	}
	glm::mat3 ATAp = E * sigma * glm::transpose (E);
	glm::vec3 p = m_pointsSum / float (m_pointsCount);
	glm::vec3 b (A[3][0], A[3][1], A[3][2]);
	glm::vec3 c = ATAp * (AT * b - ATA * p);
	// TODO: replace this hack with proper bounded solver
	return glm::clamp (c + p, min_point, max_point);
}

/* Transform NxM matrix to upper triangular form using Householder
 reflections. The process is known as QR decomposition, this routine
 computes R matrix and stores it in place of A.
 A is assumed to be column-major with N columns and M rows.
 used_rows should not exceed M.
*/
template<typename T, int N, int M>
void householder (T A[N][M], int used_rows) {
	T v[M];
	// Zero out all subdiagonal elements in i-th column
	for (int i = 0; i < N; i++) {
		// Sum of squares of i-th subcolumn
		T norm { 0 };
		for (int j = i; j < used_rows; j++)
			norm += A[i][j] * A[i][j];
		// Make v - reflection vector
		memset (v, 0, sizeof (v));
		T invgamma { 0 };
		if (norm < std::numeric_limits<T>::min ()) {
			v[i] = T (1);
			invgamma = T (2);
		}
		else {
			T mult = T (1) / glm::sqrt (norm);
			for (int j = i; j < used_rows; j++)
				v[j] = A[i][j] * mult;
			if (v[i] >= T (0))
				v[i] += T (1);
			else v[i] -= T (1);
			invgamma = T (1) / glm::abs (v[i]);
		}
		// For each column do A[j] -= v * dot (A[j], v) / gamma
		for (int j = i; j < N; j++) {
			T mult { 0 };
			for (int k = i; k < used_rows; k++)
				mult += A[j][k] * v[k];
			mult *= invgamma;
			for (int k = i; k < used_rows; k++)
				A[j][k] -= mult * v[k];
		}
	}
}

void QefSolver3D::compressMatrix () noexcept {
	householder<float, 4, kMaxRows> (A, m_usedRows);
	m_usedRows = 4;
}

struct UniformGridEdge {
	UniformGridEdge () noexcept {}
	/** \brief Main constructor

		\param[in] lesserX,lesserY,lesserZ Local coordinates of the lesser endpoint
		\param[in] gradient Gradient of the surface function in surface-crossing point
		\param[in] offset Surface-crossing point offset from lesser endpoint (in local coordinates)
		\param[in] axis Axis of this edge in GLM order (X=0, Y=1, Z=2)
		\param[in] isLesserEndpointSolid Self-descriptive
		\param[in] solidMaterial Material of the solid endpoint
	*/
	UniformGridEdge (int32_t lesserX, int32_t lesserY, int32_t lesserZ,
	                 const glm::dvec3 &gradient, double offset,
	                 int axis, bool isLesserEndpointSolid /* , Material solidMaterial */) noexcept;
	/// Returns surface normal in the surface-crossing point on this edge
	glm::vec3 surfaceNormal () const noexcept;
	/// Returns local coordinates of the surface-crossing point on this edge
	glm::vec3 surfacePoint () const noexcept;
	/// Returns the material of the solid endpoint
	//Material solidEndpointMaterial () const noexcept { return material; }
	/// Returns local coordinates of the lesser endpoint
	glm::ivec3 lesserEndpoint () const noexcept;
	/// Returns local coordinates of the bigger endpoint
	glm::ivec3 biggerEndpoint () const noexcept;
	/// Returns true if the lesser endpoint is solid, false otherwise
	bool isLesserEndpointSolid () const noexcept { return solidEndpoint == 0; }

private:
	/** \brief Surface normal in zero-crossing point.

		Only X and Z components (and sign of Y) are stored because absolute value of Y
		may be restored using the unit length condition. This saves 4 bytes of size,
		making the grid occupy ~11% less space on average (an empirical estimation).
	*/
	float normalX, normalZ;
	/// Offset from lesser endpoint (in local coordinates)
	float offset;
	/// 0 if Y is positive, 1 if negative
	uint8_t normalYSign : 1;
	/// 0 if solid endpoint is the lesser one, 1 otherwise
	uint8_t solidEndpoint : 1;
	/// Edge axis in GLM order (X=0, Y=1, Z=2)
	uint8_t axis : 2;
	/// Material of solid endpoint of the surface
	//Material material;
	/// Local coordinates of lesser endpoint
	int16_t lesserX, lesserY, lesserZ;

	friend class UniformGridEdgeStorage;
};

/** \brief Compressed storage of uniform grid edges
*/
class UniformGridEdgeStorage {
public:
	using iterator = std::vector<UniformGridEdge>::iterator;
	using const_iterator = std::vector<UniformGridEdge>::const_iterator;
	using size_type = std::vector<UniformGridEdge>::size_type;

	template<typename... Args>
	void addEdge (Args &&... args) { m_edges.emplace_back (std::forward<Args> (args)...); }
	/** \brief Sorts stored edges by lesser endpoints (in YXZ order)

		Using \ref findEdge is possible only when edges are sorted. Instead of calling this
		function you may enforce adding edges in given order (if possible).
	*/
	void sortEdges () noexcept;
	void clear () noexcept { m_edges.clear (); }
	iterator begin () noexcept { return m_edges.begin (); }
	iterator end () noexcept { return m_edges.end (); }
	/** \brief Finds an edge with given lesser endpoint coordinates

		\param[in] x,y,z Local coordinates of the lesser endpoint
		\return Iterator to the found edge or \ref end in case no edge was found
		\attention This function runs binary search. Make sure edges are sorted before calling
	*/
	iterator findEdge (int32_t x, int32_t y, int32_t z) noexcept;
	/// Returns the number of edges in storage
	size_type size () const noexcept { return m_edges.size (); }
	const_iterator begin () const noexcept { return m_edges.begin (); }
	const_iterator end () const noexcept { return m_edges.end (); }
	const_iterator cbegin () const noexcept { return m_edges.cbegin (); }
	const_iterator cend () const noexcept { return m_edges.cend (); }
	/// \copydoc findEdge
	const_iterator findEdge (int32_t x, int32_t y, int32_t z) const noexcept;
private:
	/// Wrapped container
	std::vector<UniformGridEdge> m_edges;
	/// 'Less' comparator for edges, orders them as (Y, X, Z) tuples
	static bool edgeLess (const UniformGridEdge &a, const UniformGridEdge &b) noexcept;
};

UniformGridEdge::UniformGridEdge (int32_t lesserX, int32_t lesserY, int32_t lesserZ,
                                  const glm::dvec3 &gradient, double offset,
                                  int axis, bool isLesserEndpointSolid /* , Material solidMaterial */) noexcept {
	constexpr int32_t max16 = std::numeric_limits<int16_t>::max (); (void)max16;
	constexpr int32_t min16 = std::numeric_limits<int16_t>::min (); (void)min16;
	assert (lesserX <= max16 && lesserY <= max16 && lesserZ <= max16);
	assert (lesserX >= min16 && lesserY >= min16 && lesserZ >= min16);
	assert (axis >= 0 && axis <= 2);
	this->lesserX = int16_t (lesserX);
	this->lesserY = int16_t (lesserY);
	this->lesserZ = int16_t (lesserZ);
	// Normalize gradient to get surface normal. Adding epsilon to avoid
	// possible division by zero in case of zero gradient.
	double grad_len = glm::length (gradient) + std::numeric_limits<double>::epsilon ();
	this->normalX = float (gradient.x / grad_len);
	this->normalZ = float (gradient.z / grad_len);
	this->normalYSign = (gradient.y < 0);
	this->offset = float (offset);
	this->axis = uint8_t (axis);
	this->solidEndpoint = uint8_t (!isLesserEndpointSolid);
	//this->material = solidMaterial;
}

glm::vec3 UniformGridEdge::surfaceNormal () const noexcept {
	float y_squared = 1.0f - normalX * normalX - normalZ * normalZ;
	float normalY = glm::sqrt (glm::max (0.0f, y_squared));
	if (normalYSign)
		normalY = -normalY;
	return { normalX, normalY, normalZ };
}

glm::vec3 UniformGridEdge::surfacePoint () const noexcept {
	glm::vec3 point (lesserX, lesserY, lesserZ);
	point[axis] += offset;
	return point;
}

glm::ivec3 UniformGridEdge::lesserEndpoint () const noexcept {
	return { lesserX, lesserY, lesserZ };
}

glm::ivec3 UniformGridEdge::biggerEndpoint () const noexcept {
	glm::ivec3 point (lesserX, lesserY, lesserZ);
	point[axis]++;
	return point;
}

bool UniformGridEdgeStorage::edgeLess (const UniformGridEdge &a, const UniformGridEdge &b) noexcept {
	// Compare as (Y, X, Z) tuples
	if (a.lesserY < b.lesserY)
		return true;
	if (a.lesserY == b.lesserY) {
		if (a.lesserX < b.lesserX)
			return true;
		if (a.lesserX == b.lesserX)
			return a.lesserZ < b.lesserZ;
	}
	return false;
}

void UniformGridEdgeStorage::sortEdges () noexcept {
	std::sort (m_edges.begin (), m_edges.end (), edgeLess);
}

UniformGridEdgeStorage::iterator UniformGridEdgeStorage::findEdge
	(int32_t x, int32_t y, int32_t z) noexcept {
	constexpr int32_t max16 = std::numeric_limits<int16_t>::max ();
	constexpr int32_t min16 = std::numeric_limits<int16_t>::min ();
	if (x > max16 || y > max16 || z > max16)
		return end ();
	if (x < min16 || y < min16 || z < min16)
		return end ();
	UniformGridEdge sample;
	sample.lesserX = int16_t (x);
	sample.lesserY = int16_t (y);
	sample.lesserZ = int16_t (z);
	auto iter = std::lower_bound (m_edges.begin (), m_edges.end (), sample, edgeLess);
	if (iter == m_edges.end ())
		return iter;
	if (iter->lesserX != x || iter->lesserY != y || iter->lesserZ != z)
		return end ();
	return iter;
}

UniformGridEdgeStorage::const_iterator UniformGridEdgeStorage::findEdge
	(int32_t x, int32_t y, int32_t z) const noexcept {
	constexpr int32_t max16 = std::numeric_limits<int16_t>::max ();
	constexpr int32_t min16 = std::numeric_limits<int16_t>::min ();
	if (x > max16 || y > max16 || z > max16)
		return end ();
	if (x < min16 || y < min16 || z < min16)
		return end ();
	UniformGridEdge sample;
	sample.lesserX = int16_t (x);
	sample.lesserY = int16_t (y);
	sample.lesserZ = int16_t (z);
	auto iter = std::lower_bound (m_edges.begin (), m_edges.end (), sample, edgeLess);
	if (iter == m_edges.end ())
		return iter;
	if (iter->lesserX != x || iter->lesserY != y || iter->lesserZ != z)
		return end ();
	return iter;
}

struct UniformGridAdapter {
	UniformGridEdgeStorage edgesX;
	UniformGridEdgeStorage edgesY;
	UniformGridEdgeStorage edgesZ;

	const voxen::TerrainChunk::VoxelData& voxels;

	UniformGridAdapter(const voxen::TerrainChunk::VoxelData& voxels_data) : voxels(voxels_data) {}

	std::array<Material, 8> materialsOfCell (glm::ivec3 cell) const noexcept {
		// voxels data - YXZ storage
		const uint8_t y = cell.y;
		const uint8_t x = cell.x;
		const uint8_t z = cell.z;
		assert(x < voxen::TerrainChunk::SIZE);
		assert(y < voxen::TerrainChunk::SIZE);
		assert(z < voxen::TerrainChunk::SIZE);

		std::array<Material, 8> mats;
		mats[0] = voxels[y][x][z] == 0 ? Material::Empty : Material::Solid;
		mats[1] = voxels[y][x][z+1] == 0 ? Material::Empty : Material::Solid;
		mats[2] = voxels[y][x+1][z] == 0 ? Material::Empty : Material::Solid;
		mats[3] = voxels[y][x+1][z+1] == 0 ? Material::Empty : Material::Solid;
		mats[4] = voxels[y+1][x][z] == 0 ? Material::Empty : Material::Solid;
		mats[5] = voxels[y+1][x][z+1] == 0 ? Material::Empty : Material::Solid;
		mats[6] = voxels[y+1][x+1][z] == 0 ? Material::Empty : Material::Solid;
		mats[7] = voxels[y+1][x+1][z+1] == 0 ? Material::Empty : Material::Solid;
		return mats;
	}
};

struct DC_OctreeNode {
	DC_OctreeNode () noexcept;
	~DC_OctreeNode () noexcept;

	void subdivide ();
	void collapse ();
	bool isSubdivided () const noexcept { return !is_leaf; }
	bool isHomogenous () const noexcept;
	int16_t depth () const noexcept { return m_depth; }

	DC_OctreeNode *operator[] (int num) const noexcept { return children[num]; }

	union {
		struct {
			glm::vec3 dual_vertex;
			glm::vec3 normal;
			std::array<Material, 8> corners;
			QefSolver3D::State qef;
			uint32_t vertex_id;
		} leaf_data;
		DC_OctreeNode *children[8];
	};

private:
	bool is_leaf;
	int16_t m_depth = 0;
};

DC_OctreeNode::DC_OctreeNode () noexcept : is_leaf (true) {
	for (int i = 0; i < 8; i++)
		children[i] = nullptr;
}

DC_OctreeNode::~DC_OctreeNode () noexcept {
	if (!is_leaf)
		collapse ();
}

void DC_OctreeNode::subdivide () {
	if (!is_leaf)
		throw std::logic_error ("Octree node is already subdivided");
	is_leaf = false;
	for (int i = 0; i < 8; i++) {
		children[i] = new DC_OctreeNode;
		children[i]->m_depth = m_depth + 1;
	}
}

void DC_OctreeNode::collapse () {
	if (is_leaf)
		throw std::logic_error ("Octree node is already collapsed");
	is_leaf = true;
	for (int i = 0; i < 8; i++) {
		delete children[i];
		children[i] = nullptr;
	}
}

bool DC_OctreeNode::isHomogenous () const noexcept {
	if (!is_leaf)
		return false;
	bool all_air = true;
	bool all_solid = true;
	for (int i = 0; i < 8; i++) {
		if (leaf_data.corners[i] == Material::Empty)
			all_solid = false;
		else all_air = false;
	}
	return all_air || all_solid;
}

class DC_Octree {
public:
	explicit DC_Octree (int32_t root_size);

	void build (const UniformGridAdapter &G, QefSolver3D &solver, float epsilon,
	            bool use_octree_simplification = true);
	void contour (voxen::TerrainSurface& surface);

private:
	const int32_t m_rootSize;

	DC_OctreeNode m_root;

	struct BuildArgs {
		const UniformGridAdapter &grid;
		QefSolver3D &solver;
		float epsilon;
		bool use_octree_simplification;
	};

	using CubeMaterials = std::array<std::array<std::array<Material, 3>, 3>, 3>;

	void buildNode (DC_OctreeNode *node, glm::ivec3 min_corner, int32_t size, BuildArgs &args);
	void buildLeaf (DC_OctreeNode *node, glm::ivec3 min_corner, int32_t size, BuildArgs &args);
	// Implements topological safety test from Dual Contouring paper
	static bool checkTopoSafety (const CubeMaterials &mats) noexcept;
};

DC_Octree::DC_Octree (int32_t root_size) : m_rootSize (root_size) {
	if (root_size <= 0 || (root_size & (root_size - 1)))
		throw std::invalid_argument ("Octree size is not a power of two");
}

void DC_Octree::build (const UniformGridAdapter &grid, QefSolver3D &solver, float epsilon,
                       bool use_octree_simplification) {
	BuildArgs args {
		grid, solver,
		epsilon,
		use_octree_simplification
	};
	try {
		if (m_root.isSubdivided ())
			m_root.collapse ();
		glm::ivec3 min_corner (0);
		buildNode (&m_root, min_corner, m_rootSize, args);
	}
	catch (...) {
		if (m_root.isSubdivided())
			m_root.collapse ();
		throw;
	}
}

namespace dc_detail
{

/* Quadruples of node children sharing an edge along some axis. First dimension - axis,
 second dimension - quadruple, third dimension - children. */
constexpr int edgeTable[3][2][4] = {
	{ { 0, 4, 5, 1 }, { 2, 6, 7, 3 } }, // X
	{ { 0, 1, 3, 2 }, { 4, 5, 7, 6 } }, // Y
	{ { 0, 2, 6, 4 }, { 1, 3, 7, 5 } }  // Z
};

template<int D>
void edgeProc (std::array<const DC_OctreeNode *, 4> nodes, voxen::TerrainSurface& surface) {
	/* For a quadruple of nodes sharing an edge along some axis there are two quadruples
	 of their children nodes sharing the same edge. This table maps node to its child. First
	 dimension - axis, second dimension - position of child. Two values - parent number (in
	 edgeProc's numbering) and its child number (in octree numbering). Is it better understandable
	 by looking at the code? */
	constexpr int subTable[3][8][2] = {
		{ { 0, 5 }, { 3, 4 }, { 0, 7 }, { 3, 6 },
			{ 1, 1 }, { 2, 0 }, { 1, 3 }, { 2, 2 } }, // X
		{ { 0, 3 }, { 1, 2 }, { 3, 1 }, { 2, 0 },
			{ 0, 7 }, { 1, 6 }, { 3, 5 }, { 2, 4 } }, // Y
		{ { 0, 6 }, { 0, 7 }, { 1, 4 }, { 1, 5 },
			{ 3, 2 }, { 3, 3 }, { 2, 0 }, { 2, 1 } }  // Z
	};
	// Almost the same as above
	constexpr int cornersTable[3][4][2] = {
		{ { 5, 7 }, { 1, 3 }, { 0, 2 }, { 4, 6 } }, // X
		{ { 3, 7 }, { 2, 6 }, { 0, 4 }, { 1, 5 } }, // Y
		{ { 6, 7 }, { 4, 5 }, { 0, 1 }, { 2, 3 } }  // Z
	};
	/* If at least one node is not present then (by topological safety test guarantee) this edge
	 is not surface-crossing (otherwise there was an unsafe node collapse). */
	if (!nodes[0] || !nodes[1] || !nodes[2] || !nodes[3])
		return;
	const DC_OctreeNode *sub[8];
	bool all_leaves = true;
	for (int i = 0; i < 8; i++) {
		const DC_OctreeNode *n = nodes[subTable[D][i][0]];
		if (n->isSubdivided ()) {
			sub[i] = n->children[subTable[D][i][1]];
			all_leaves = false;
		}
		else sub[i] = n;
	}
	if (!all_leaves) {
		for (int i = 0; i < 2; i++) {
			int i1 = edgeTable[D][i][0];
			int i2 = edgeTable[D][i][1];
			int i3 = edgeTable[D][i][2];
			int i4 = edgeTable[D][i][3];
			edgeProc<D> ({ sub[i1], sub[i2], sub[i3], sub[i4] }, surface);
		}
		return;
	}
	Material mat1 = Material::Empty, mat2 = Material::Empty;
	/* Find the minimal node, i.e. the node with the maximal depth. By looking at its
	 materials on endpoints of this edge we may know whether the edge is surface-crossing
	 and if we need to flip the triangles winding order. */
	int16_t max_depth = -1;
	for (int i = 0; i < 4; i++) {
		if (nodes[i]->depth () > max_depth) {
			max_depth = nodes[i]->depth ();
			mat1 = nodes[i]->leaf_data.corners[cornersTable[D][i][0]];
			mat2 = nodes[i]->leaf_data.corners[cornersTable[D][i][1]];
		}
	}
	if (mat1 == mat2)
		return; // Not a surface-crossing edge
	if (mat1 != Material::Empty && mat2 != Material::Empty)
		return; // Ditto
	/* We assume that lower endpoint is solid. If this is not the case, the triangles'
	 winding order should be flipped to remain facing outside of the surface. */
	bool flip = (mat1 == Material::Empty);
	uint32_t id0 = nodes[0]->leaf_data.vertex_id;
	uint32_t id1 = nodes[1]->leaf_data.vertex_id;
	uint32_t id2 = nodes[2]->leaf_data.vertex_id;
	uint32_t id3 = nodes[3]->leaf_data.vertex_id;
	if (!flip) {
		surface.addTriangle (id0, id1, id2);
		surface.addTriangle (id0, id2, id3);
	}
	else {
		surface.addTriangle (id0, id2, id1);
		surface.addTriangle (id0, id3, id2);
	}
}

constexpr int faceTableX[4][2] = { { 0, 2 }, { 4, 6 }, { 5, 7 }, { 1, 3 } };
constexpr int faceTableY[4][2] = { { 0, 4 }, { 1, 5 }, { 3, 7 }, { 2, 6 } };
constexpr int faceTableZ[4][2] = { { 0, 1 }, { 2, 3 }, { 6, 7 }, { 4, 5 } };

void faceProcX (std::array<const DC_OctreeNode *, 2> nodes, voxen::TerrainSurface& surface) {
	constexpr int subTable[8][2] = {
		{ 0, 2 }, { 0, 3 }, { 1, 0 }, { 1, 1 },
		{ 0, 6 }, { 0, 7 }, { 1, 4 }, { 1, 5 }
	};
	assert (nodes[0] && nodes[1]);
	const DC_OctreeNode *sub[8];
	bool has_lesser = false;
	for (int i = 0; i < 8; i++) {
		const DC_OctreeNode *n = nodes[subTable[i][0]];
		if (n->isSubdivided ()) {
			sub[i] = (*n)[subTable[i][1]];
			has_lesser = true;
		}
		else sub[i] = n;
	}
	if (!has_lesser)
		return;
	for (int i = 0; i < 4; i++) {
		int i1 = faceTableX[i][0];
		int i2 = faceTableX[i][1];
		faceProcX ({ sub[i1], sub[i2] }, surface);
	}
	for (int i = 0; i < 2; i++) {
		int i1 = edgeTable[1][i][0];
		int i2 = edgeTable[1][i][1];
		int i3 = edgeTable[1][i][2];
		int i4 = edgeTable[1][i][3];
		edgeProc<1> ({ sub[i1], sub[i2], sub[i3], sub[i4] }, surface);
	}
	for (int i = 0; i < 2; i++) {
		int i1 = edgeTable[2][i][0];
		int i2 = edgeTable[2][i][1];
		int i3 = edgeTable[2][i][2];
		int i4 = edgeTable[2][i][3];
		edgeProc<2> ({ sub[i1], sub[i2], sub[i3], sub[i4] }, surface);
	}
}

void faceProcY (std::array<const DC_OctreeNode *, 2> nodes, voxen::TerrainSurface& surface) {
	constexpr int subTable[8][2] = {
		{ 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 7 },
		{ 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }
	};
	assert (nodes[0] && nodes[1]);
	const DC_OctreeNode *sub[8];
	bool has_lesser = false;
	for (int i = 0; i < 8; i++) {
		const DC_OctreeNode *n = nodes[subTable[i][0]];
		if (n->isSubdivided ()) {
			sub[i] = (*n)[subTable[i][1]];
			has_lesser = true;
		}
		else sub[i] = n;
	}
	if (!has_lesser)
		return;
	for (int i = 0; i < 4; i++) {
		int i1 = faceTableY[i][0];
		int i2 = faceTableY[i][1];
		faceProcY ({ sub[i1], sub[i2] }, surface);
	}
	for (int i = 0; i < 2; i++) {
		int i1 = edgeTable[0][i][0];
		int i2 = edgeTable[0][i][1];
		int i3 = edgeTable[0][i][2];
		int i4 = edgeTable[0][i][3];
		edgeProc<0> ({ sub[i1], sub[i2], sub[i3], sub[i4] }, surface);
	}
	for (int i = 0; i < 2; i++) {
		int i1 = edgeTable[2][i][0];
		int i2 = edgeTable[2][i][1];
		int i3 = edgeTable[2][i][2];
		int i4 = edgeTable[2][i][3];
		edgeProc<2> ({ sub[i1], sub[i2], sub[i3], sub[i4] }, surface);
	}
}

void faceProcZ (std::array<const DC_OctreeNode *, 2> nodes, voxen::TerrainSurface& surface) {
	constexpr int subTable[8][2] = {
		{ 0, 1 }, { 1, 0 }, { 0, 3 }, { 1, 2 },
		{ 0, 5 }, { 1, 4 }, { 0, 7 }, { 1, 6 }
	};
	assert (nodes[0] && nodes[1]);
	const DC_OctreeNode *sub[8];
	bool has_lesser = false;
	for (int i = 0; i < 8; i++) {
		const DC_OctreeNode *n = nodes[subTable[i][0]];
		if (n->isSubdivided ()) {
			sub[i] = (*n)[subTable[i][1]];
			has_lesser = true;
		}
		else sub[i] = n;
	}
	if (!has_lesser)
		return;
	for (int i = 0; i < 4; i++) {
		int i1 = faceTableZ[i][0];
		int i2 = faceTableZ[i][1];
		faceProcZ ({ sub[i1], sub[i2] }, surface);
	}
	for (int i = 0; i < 2; i++) {
		int i1 = edgeTable[0][i][0];
		int i2 = edgeTable[0][i][1];
		int i3 = edgeTable[0][i][2];
		int i4 = edgeTable[0][i][3];
		edgeProc<0> ({ sub[i1], sub[i2], sub[i3], sub[i4] }, surface);
	}
	for (int i = 0; i < 2; i++) {
		int i1 = edgeTable[1][i][0];
		int i2 = edgeTable[1][i][1];
		int i3 = edgeTable[1][i][2];
		int i4 = edgeTable[1][i][3];
		edgeProc<1> ({ sub[i1], sub[i2], sub[i3], sub[i4] }, surface);
	}
}

void cellProc (const DC_OctreeNode *node, voxen::TerrainSurface& surface) {
	assert (node);
	if (!node->isSubdivided ())
		return;
	const DC_OctreeNode *sub[8];
	for (int i = 0; i < 8; i++) {
		sub[i] = (*node)[i];
		cellProc (sub[i], surface);
	}
	for (int i = 0; i < 4; i++) {
		faceProcX ({ sub[faceTableX[i][0]], sub[faceTableX[i][1]] }, surface);
		faceProcY ({ sub[faceTableY[i][0]], sub[faceTableY[i][1]] }, surface);
		faceProcZ ({ sub[faceTableZ[i][0]], sub[faceTableZ[i][1]] }, surface);
	}
	for (int i = 0; i < 2; i++) {
		edgeProc<0> ({ sub[edgeTable[0][i][0]], sub[edgeTable[0][i][1]],
		               sub[edgeTable[0][i][2]], sub[edgeTable[0][i][3]] }, surface);
		edgeProc<1> ({ sub[edgeTable[1][i][0]], sub[edgeTable[1][i][1]],
		               sub[edgeTable[1][i][2]], sub[edgeTable[1][i][3]] }, surface);
		edgeProc<2> ({ sub[edgeTable[2][i][0]], sub[edgeTable[2][i][1]],
		               sub[edgeTable[2][i][2]], sub[edgeTable[2][i][3]] }, surface);
	}
}

void makeVertices (DC_OctreeNode *node, voxen::TerrainSurface& surface) {
	//TODO(sirgienko) add Material support, when we will have any voxel data
	//MaterialFilter filter;
	if (!node->isSubdivided ()) {
		if (!node->isHomogenous ()) {
			const glm::vec3& vertex = node->leaf_data.dual_vertex;
			const glm::vec3& normal = node->leaf_data.normal;
			//filter.reset ();
			//filter.add (node->leaf_data.corners);
			//Material mat = filter.select ();
			node->leaf_data.vertex_id = surface.addVertex({ vertex, normal });
		}
		else
			node->leaf_data.vertex_id = std::numeric_limits<uint32_t>::max ();
	}
	else for (int i = 0; i < 8; i++)
		makeVertices (node->children[i], surface);
}

}

using namespace dc_detail;

void DC_Octree::contour (voxen::TerrainSurface& surface) {
	makeVertices (&m_root, surface);
	cellProc (&m_root, surface);
}

void DC_Octree::buildNode (DC_OctreeNode *node, glm::ivec3 min_corner,
                           int32_t size, BuildArgs &args) {
	assert (node);
	if (size == 1) {
		buildLeaf (node, min_corner, size, args);
		return;
	}
	node->subdivide ();
	int32_t child_size = size / 2;
	for (int i = 0; i < 8; i++) {
		glm::ivec3 child_min_corner = min_corner + child_size * kCellCornerOffset[i];
		buildNode(node->children[i], child_min_corner, child_size, args);
	}
	// All children are build and possibly simplified, try to do simplification of this node
	std::array<Material, 8> corners;
	corners.fill (Material::Empty);
	bool all_children_homogenous = true;
	for (int i = 0; i < 8; i++) {
		// We can't do any simplification if there is a least one non-leaf child
		if (node->children[i]->isSubdivided ())
			return;
		if (!node->children[i]->isHomogenous ())
			all_children_homogenous = false;
		corners[i] = node->children[i]->leaf_data.corners[i];
	}
	// If all children are homogenous, simply drop them (this octree is not to be used as a storage)
	if (all_children_homogenous) {
		node->collapse ();
		node->leaf_data.corners = corners;
		return;
	}
	if (args.use_octree_simplification) {
		CubeMaterials mats;
		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++) {
				auto offset = kCellCornerOffset[i] + kCellCornerOffset[j];
				mats[offset.y][offset.x][offset.z] = node->children[i]->leaf_data.corners[j];
			}
		}
		if (!checkTopoSafety (mats))
			return;
		args.solver.reset ();
		glm::vec3 avg_normal { 0 };
		for (int i = 0; i < 8; i++) {
			if (!node->children[i]->isHomogenous ()) {
				avg_normal += node->children[i]->leaf_data.normal;
				args.solver.merge (node->children[i]->leaf_data.qef);
			}
		}
		glm::vec3 lower_bound (min_corner);
		glm::vec3 upper_bound (min_corner + size);
		glm::vec3 vertex = args.solver.solve (lower_bound, upper_bound);
		float error = args.solver.eval (vertex);
		if (error > args.epsilon)
			return;
		node->collapse ();
		node->leaf_data.dual_vertex = vertex;
		node->leaf_data.normal = glm::normalize (avg_normal);
		node->leaf_data.corners = corners;
		node->leaf_data.qef = args.solver.state ();
	}
}

void DC_Octree::buildLeaf (DC_OctreeNode *node, glm::ivec3 min_corner,
                           int32_t size, BuildArgs &args) {
	QefSolver3D &solver = args.solver;
	const UniformGridAdapter &G = args.grid;

	solver.reset ();
	glm::vec3 avg_normal { 0 };

	node->leaf_data.corners = G.materialsOfCell(min_corner);

	bool has_edges = false;

	constexpr int edge_table[3][4][2] = {
		{ { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 } }, // X
		{ { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } }, // Y
		{ { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 } }  // Z
	};
	for (int dim = 0; dim <= 2; dim++) {
		for (int i = 0; i < 4; i++) {
			Material mat1 = node->leaf_data.corners[edge_table[dim][i][0]];
			Material mat2 = node->leaf_data.corners[edge_table[dim][i][1]];
			if (mat1 == mat2)
				continue;
			if (mat1 != Material::Empty && mat2 != Material::Empty)
				continue;
			has_edges = true;
			// C++ really needs 'for static' like in D language...
			const auto &storage = (dim == 0 ? G.edgesX : dim == 1 ? G.edgesY : G.edgesZ);
			auto edge_pos = min_corner + kCellCornerOffset[edge_table[dim][i][0]];
			auto iter = storage.findEdge (edge_pos.x, edge_pos.y, edge_pos.z);
			assert (iter != storage.end ());
			glm::vec3 vertex = iter->surfacePoint ();
			glm::vec3 normal = iter->surfaceNormal ();
			solver.addPlane (vertex, normal);
			avg_normal += normal;
		}
	}

	if (has_edges) {
		node->leaf_data.normal = glm::normalize (avg_normal);
		glm::vec3 lower_bound (min_corner);
		glm::vec3 upper_bound (min_corner + size);
		node->leaf_data.dual_vertex = solver.solve (lower_bound, upper_bound);
		node->leaf_data.qef = solver.state ();
	}
}

bool DC_Octree::checkTopoSafety (const CubeMaterials &mats) noexcept {
	constexpr int dcToMc[8] = { 0, 3, 1, 2, 4, 7, 5, 6 };
	constexpr bool isManifold[256] = {
		true, true, true, true, true, false, true, true, true, true, false, true, true, true, true, true,
		true, true, false, true, false, false, false, true, false, true, false, true, false, true, false, true,
		true, false, true, true, false, false, true, true, false, false, false, true, false, false, true, true,
		true, true, true, true, false, false, true, true, false, true, false, true, false, false, false, true,
		true, false, false, false, true, false, true, true, false, false, false, false, true, true, true, true,
		false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
		true, false, true, true, true, false, true, true, false, false, false, false, true, false, true, true,
		true, true, true, true, true, false, true, true, false, false, false, false, false, false, false, true,
		true, false, false, false, false, false, false, false, true, true, false, true, true, true, true, true,
		true, true, false, true, false, false, false, false, true, true, false, true, true, true, false, true,
		false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
		true, true, true, true, false, false, false, false, true, true, false, true, false, false, false, true,
		true, false, false, false, true, false, true, false, true, true, false, false, true, true, true, true,
		true, true, false, false, true, false, false, false, true, true, false, false, true, true, false, true,
		true, false, true, false, true, false, true, false, true, false, false, false, true, false, true, true,
		true, true, true, true, true, false, true, true, true, true, false, true, true, true, true, true,
	};
	uint32_t mask = 0;
	for (int i = 0; i < 8; i++) {
		auto pos = 2 * kCellCornerOffset[i];
		auto mat = mats[pos.y][pos.x][pos.z];
		if (mat != Material::Empty)
			mask |= uint32_t (1 << dcToMc[i]);
	}
	if (!isManifold[mask])
		return false;
	for (int i = 0; i < 8; i++) {
		uint32_t submask = 0;
		for (int j = 0; j < 8; j++) {
			auto pos = kCellCornerOffset[i] + kCellCornerOffset[j];
			auto mat = mats[pos.y][pos.x][pos.z];
			if (mat != Material::Empty)
				submask |= uint32_t (1 << dcToMc[j]);
		}
		if (!isManifold[submask])
			return false;
	}
	// Check edge midpoint signs
	for (int c1 = 0; c1 < 3; c1++) {
		for (int c2 = 0; c2 < 3; c2++) {
			if (mats[1][c1][c2] != mats[0][c1][c2] && mats[1][c1][c2] != mats[2][c1][c2])
				return false;
			if (mats[c1][1][c2] != mats[c1][0][c2] && mats[c1][1][c2] != mats[c1][2][c2])
				return false;
			if (mats[c1][c2][1] != mats[c1][c2][0] && mats[c1][c2][1] != mats[c1][c2][2])
				return false;
		}
	}
	// Check face midpoint signs
	for (int c1 = 0; c1 < 3; c1++) {
		Material mat;
		mat = mats[1][1][c1];
		if (mat != mats[0][0][c1] && mat != mats[0][2][c1] && mat != mats[2][0][c1] && mat != mats[2][2][c1])
			return false;
		mat = mats[1][c1][1];
		if (mat != mats[0][c1][0] && mat != mats[0][c1][2] && mat != mats[2][c1][0] && mat != mats[2][c1][2])
			return false;
		mat = mats[c1][1][1];
		if (mat != mats[c1][0][0] && mat != mats[c1][0][2] && mat != mats[c1][2][0] && mat != mats[c1][2][2])
			return false;
	}
	// Check cube midpoint sign
	auto mat = mats[1][1][1];
	for (int i = 0; i < 8; i++) {
		auto pos = 2 * kCellCornerOffset[i];
		if (mat == mats[pos.y][pos.x][pos.z])
			return true;
	}
	return false;
}

namespace voxen {

void TerrainSurfaceBuilder::calcSurface(const TerrainChunk::VoxelData& voxels, TerrainSurface& surface) {
	// Simple DC algorith for onematerial voxel data

	UniformGridAdapter storage(voxels);
	for (uint32_t i = 0; i < TerrainChunk::SIZE; i++) {
		for (uint32_t j = 0; j < TerrainChunk::SIZE; j++) {
			for (uint32_t k = 0; k < TerrainChunk::SIZE; k++) {
				int32_t y = i;
				int32_t x = j;
				int32_t z = k;

				bool is_cur_solid = (voxels[i][j][k] != 0);
				glm::vec3 average_isopoint(0);
				double x_offset = 0.0;
				double y_offset = 0.0;
				double z_offset = 0.0;

				if (k + 1 < TerrainChunk::SIZE && voxels[i][j][k + 1] != voxels[i][j][k]) {
					z_offset = edgeOffsetZ(voxels, i, j, k);
					average_isopoint += glm::vec3(0, 0, z_offset);
				}

				if (j + 1 < TerrainChunk::SIZE && voxels[i][j + 1][k] != voxels[i][j][k]) {
					x_offset = edgeOffsetX(voxels, i, j, k);
					average_isopoint += glm::vec3(x_offset, 0, 0);
				}

				if (i + 1 < TerrainChunk::SIZE && voxels[i + 1][j][k] != voxels[i][j][k]) {
					y_offset = edgeOffsetY(voxels, i, j, k);
					average_isopoint += glm::vec3(0, y_offset, 0);
				}

				if (x_offset + y_offset + z_offset != 0.0) {
					//TODO(sirgienko) This logic works bad and should be change to correct solution for calculation edge interception and gradient in this points
					const glm::vec3& grad = glm::normalize(average_isopoint);

					if (x_offset != 0.0)
						storage.edgesX.addEdge(x, y, z, grad, x_offset, 0, is_cur_solid);
					if (y_offset != 0.0)
						storage.edgesY.addEdge(x, y, z, grad, y_offset, 1, is_cur_solid);
					if (z_offset != 0.0)
						storage.edgesZ.addEdge(x, y, z, grad, z_offset, 0, is_cur_solid);
				}
			}
		}
	}

	static const double kNoCompressDcEpsilon = 0.0;

	QefSolver3D qef_solver;
	DC_Octree octree (TerrainChunk::CELL_COUNT);
	octree.build(storage, qef_solver, kNoCompressDcEpsilon, true);
	octree.contour (surface);
}

double TerrainSurfaceBuilder::edgeOffsetX(const TerrainChunk::VoxelData& voxels, uint32_t i, uint32_t j, uint32_t k) {
	uint8_t mat1 = voxels[i][j][k];
	uint8_t mat2 = voxels[i][j+1][k];

	int8_t sum1 = 1;
	int8_t sum2 = 1;
	if (i+1 < TerrainChunk::SIZE) {
		if (mat1 == voxels[i+1][j][k])
			sum1++;
		if (mat2 == voxels[i+1][j+1][k])
			sum2++;
	}
	if (i >= 1) {
		if (mat1 == voxels[i-1][j][k])
			sum1++;
		if (mat2 == voxels[i-1][j+1][k])
			sum2++;
	}
	if (k+1 < TerrainChunk::SIZE) {
		if (mat1 == voxels[i][j][k+1])
			sum1++;
		if (mat2 == voxels[i][j+1][k+1])
			sum2++;
	}
	if (k >= 1) {
		if (mat1 == voxels[i][j][k-1])
			sum1++;
		if (mat2 == voxels[i][j+1][k-1])
			sum2++;
	}

	return (1.0 * sum1) / (sum1 + sum2);
}

double TerrainSurfaceBuilder::edgeOffsetY(const TerrainChunk::VoxelData& voxels, uint32_t i, uint32_t j, uint32_t k) {
	uint8_t mat1 = voxels[i][j][k];
	uint8_t mat2 = voxels[i+1][j][k];

	int8_t sum1 = 1;
	int8_t sum2 = 1;
	if (j+1 < TerrainChunk::SIZE) {
		if (mat1 == voxels[i][j+1][k])
			sum1++;
		if (mat2 == voxels[i+1][j+1][k])
			sum2++;
	}
	if (j >= 1) {
		if (mat1 == voxels[i][j-1][k])
			sum1++;
		if (mat2 == voxels[i+1][j-1][k])
			sum2++;
	}
	if (k+1 < TerrainChunk::SIZE) {
		if (mat1 == voxels[i][j][k+1])
			sum1++;
		if (mat2 == voxels[i+1][j][k+1])
			sum2++;
	}
	if (k >= 1) {
		if (mat1 == voxels[i][j][k-1])
			sum1++;
		if (mat2 == voxels[i+1][j][k-1])
			sum2++;
	}

	return (1.0 * sum1) / (sum1 + sum2);
}

double TerrainSurfaceBuilder::edgeOffsetZ(const TerrainChunk::VoxelData& voxels, uint32_t i, uint32_t j, uint32_t k) {
	uint8_t mat1 = voxels[i][j][k];
	uint8_t mat2 = voxels[i][j][k+1];

	int8_t sum1 = 1;
	int8_t sum2 = 1;
	if (i+1 < TerrainChunk::SIZE) {
		if (mat1 == voxels[i+1][j][k])
			sum1++;
		if (mat2 == voxels[i+1][j][k+1])
			sum2++;
	}
	if (i >= 1) {
		if (mat1 == voxels[i-1][j][k])
			sum1++;
		if (mat2 == voxels[i-1][j][k+1])
			sum2++;
	}
	if (j+1 < TerrainChunk::SIZE) {
		if (mat1 == voxels[i][j+1][k])
			sum1++;
		if (mat2 == voxels[i][j+1][k+1])
			sum2++;
	}
	if (j >= 1) {
		if (mat1 == voxels[i][j-1][k])
			sum1++;
		if (mat2 == voxels[i][j-1][k+1])
			sum2++;
	}

	return (1.0 * sum1) / (sum1 + sum2);
}

}
