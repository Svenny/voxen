#include <voxen/common/terrain/qef_solver.hpp>

#include <cstring>
#include <tuple>

namespace voxen
{

namespace jacobi_detail
{

// Faster but less accurate (Jacobi does not fully converge with this)
template<typename T>
static std::pair<T, T> calcSinCosFast(T tan_twophi)
{
	T inv = T(0.5) / std::hypot(T(1), tan_twophi);
	T c = std::sqrt(T(0.5) + inv);
	T s = std::copysign(std::sqrt(T(0.5) - inv), tan_twophi);
	return { s, c };
}

// More accurate but much slower
template<typename T>
static std::pair<T, T> calcSinCosAccurate(T tan_twophi)
{
	T phi = T(0.5) * std::atan(tan_twophi);
	return { std::sin(phi), std::cos(phi) };
}

template<typename T>
static void rotate(glm::mat<3, 3, T> &A, glm::mat<3, 3, T> &H, T c, T s, int i, int j)
{
#define jacobiRotate(i, j, k) { \
	float aik = A[glm::min(i, k)][glm::max(i, k)]; \
	float ajk = A[glm::min(j, k)][glm::max(j, k)]; \
	A[glm::min(i, k)][glm::max(i, k)] = c * aik + s * ajk; \
	A[glm::min(j, k)][glm::max(j, k)] = -s * aik + c * ajk; \
}
	if (i == 0 && j == 1) {
		jacobiRotate(0, 1, 2);
	} else if (i == 0 && j == 2) {
		jacobiRotate(0, 2, 1);
	} else { // (i == 1 && j == 2), other cases are impossible
		jacobiRotate(1, 2, 0);
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
static void rotate(glm::mat<4, 4, T> &A, glm::mat<4, 4, T> &H, T c, T s, int i, int j)
{
#define jacobiRotate(i, j, k) { \
	float aik = A[glm::min(i, k)][glm::max(i, k)]; \
	float ajk = A[glm::min(j, k)][glm::max(j, k)]; \
	A[glm::min(i, k)][glm::max(i, k)] = c * aik + s * ajk; \
	A[glm::min(j, k)][glm::max(j, k)] = -s * aik + c * ajk; \
}
	if (i == 0 && j == 1) {
		jacobiRotate(0, 1, 2);
		jacobiRotate(0, 1, 3);
	} else if (i == 0 && j == 2) {
		jacobiRotate(0, 2, 1);
		jacobiRotate(0, 2, 3);
	} else if (i == 0 && j == 3) {
		jacobiRotate(0, 3, 1);
		jacobiRotate(0, 3, 2);
	} else if (i == 1 && j == 2) {
		jacobiRotate(1, 2, 0);
		jacobiRotate(1, 2, 3);
	} else if (i == 1 && j == 3) {
		jacobiRotate(1, 3, 0);
		jacobiRotate(1, 3, 2);
	} else { // (i == 2 && j == 3), other cases are impossible
		jacobiRotate(2, 3, 0);
		jacobiRotate(2, 3, 1);
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
static std::pair<glm::vec<D, T>, glm::mat<D, D, T> > jacobi(glm::mat<D, D, T> A, T tolerance,
                                                            int max_iters, bool use_fast_sincos)
{
	using namespace jacobi_detail;
	glm::mat<D, D, T> E { T(1) };
	for (int k = 0; k < max_iters; k++) {
		T max_el = std::abs(A[0][1]);
		int max_i = 0;
		int max_j = 1;
		for (int i = 0; i < D; i++) {
			for (int j = i + 1; j < D; j++) {
				T el = std::abs(A[i][j]);
				if (el > max_el) {
					max_el = el;
					max_i = i;
					max_j = j;
				}
			}
		}
		if (max_el <= tolerance) {
			break;
		}

		int i = max_i;
		int j = max_j;
		T tan_twophi = (A[i][j] + A[i][j]) / (A[i][i] - A[j][j]);

		T s, c;
		if (use_fast_sincos) {
			std::tie(s, c) = calcSinCosFast(tan_twophi);
		} else {
			std::tie(s, c) = calcSinCosAccurate(tan_twophi);
		}
		rotate<T>(A, E, c, s, i, j);
	}

	glm::vec<D, T> e;
	for (int i = 0; i < D; i++) {
		e[i] = A[i][i];
	}
	return { e, E };
}

/* Transform NxM matrix to upper triangular form using Householder
 reflections. The process is known as QR decomposition, this routine
 computes R matrix and stores it in place of A.
 A is assumed to be column-major with N columns and M rows.
 used_rows should not exceed M.
*/
template<typename T, int N, int M>
static void householder(T A[N][M], int used_rows)
{
	T v[M];
	// Zero out all subdiagonal elements in i-th column
	for (int i = 0; i < N; i++) {
		// Sum of squares of i-th subcolumn
		T norm { 0 };
		for (int j = i; j < used_rows; j++) {
			norm += A[i][j] * A[i][j];
		}
		// Make v - reflection vector
		memset(v, 0, sizeof(v));
		T invgamma { 0 };
		if (norm < std::numeric_limits<T>::min()) {
			v[i] = T(1);
			invgamma = T(2);
		}
		else {
			T mult = T(1) / glm::sqrt(norm);
			for (int j = i; j < used_rows; j++) {
				v[j] = A[i][j] * mult;
			}
			if (v[i] >= T(0)) {
				v[i] += T(1);
			} else {
				v[i] -= T(1);
			}
			invgamma = T(1) / glm::abs(v[i]);
		}
		// For each column do A[j] -= v * dot (A[j], v) / gamma
		for (int j = i; j < N; j++) {
			T mult { 0 };
			for (int k = i; k < used_rows; k++) {
				mult += A[j][k] * v[k];
			}
			mult *= invgamma;

			for (int k = i; k < used_rows; k++) {
				A[j][k] -= mult * v[k];
			}
		}
	}
}

QefSolver3D::QefSolver3D() noexcept
{
	reset();
}

QefSolver3D::QefSolver3D(const State &data) noexcept
{
	reset();
	merge(data);
}

void QefSolver3D::reset() noexcept
{
	memset(A, 0, sizeof(A));
	m_usedRows = 0;
	m_pointsSum = glm::vec3 { 0 };
	m_pointsCount = 0;
	m_featureDim = 0;
}

void QefSolver3D::merge(const State &data) noexcept
{
	if (data.dim > m_featureDim) {
		// Mergee is of higher dimension, our mass point may be dismissed
		m_featureDim = data.dim;
		m_pointsSum = glm::vec3(data.mpx, data.mpy, data.mpz);
		m_pointsCount = data.mp_cnt;
	} else if (data.dim == m_featureDim) {
		// Mergee is of the same dimension, add mass points together
		m_pointsSum += glm::vec3(data.mpx, data.mpy, data.mpz);
		m_pointsCount += data.mp_cnt;
	} // else { /*do nothing*/ }
	// When mergee is of lesser dimension its mass point may be dismissed

	// We need four free rows
	if (m_usedRows > kMaxRows - 4) {
		compressMatrix();
	}

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

QefSolver3D::State QefSolver3D::state() noexcept
{
	compressMatrix();

	// Is it really safe to assume nobody will ever add 2^30 points? :P
	assert (m_pointsCount < (1u << 30u));

	return State {
		.a_11 = A[0][0], .a_12 = A[1][0], .a_13 = A[2][0], .b_1 = A[3][0],
		                 .a_22 = A[1][1], .a_23 = A[2][1], .b_2 = A[3][1],
		                                  .a_33 = A[2][2], .b_3 = A[3][2],
		                                                   .r2  = A[3][3],
		.mpx = m_pointsSum.x,
		.mpy = m_pointsSum.y,
		.mpz = m_pointsSum.z,
		.mp_cnt = m_pointsCount,
		.dim = m_featureDim
	};
}

void QefSolver3D::addPlane(glm::vec3 point, glm::vec3 normal) noexcept
{
	if (m_usedRows == kMaxRows) {
		compressMatrix();
	}

	int id = m_usedRows;
	A[0][id] = normal.x;
	A[1][id] = normal.y;
	A[2][id] = normal.z;
	A[3][id] = glm::dot(normal, point);

	m_usedRows++;
	m_pointsSum += point;
	m_pointsCount++;
}

float QefSolver3D::eval(glm::vec3 point) const noexcept
{
	float error = 0;
	for (int i = 0; i < m_usedRows; i++) {
		float dot = A[0][i] * point.x + A[1][i] * point.y + A[2][i] * point.z;
		float diff = dot - A[3][i];
		error += diff * diff;
	}
	return error;
}

glm::vec3 QefSolver3D::solve(glm::vec3 min_point, glm::vec3 max_point)
{
	compressMatrix();
	glm::mat3 AT, ATA;
	{
		glm::mat3 M;
		M[0] = glm::vec3(A[0][0], A[0][1], A[0][2]);
		M[1] = glm::vec3(A[1][0], A[1][1], A[1][2]);
		M[2] = glm::vec3(A[2][0], A[2][1], A[2][2]);
		AT = glm::transpose(M);
		ATA = AT * M;
	}
	auto[e, E] = jacobi(ATA, m_jacobiTolerance, m_maxJacobiIters, m_useFastFormulas);
	glm::mat3 sigma { 0.0f };
	m_featureDim = 3;
	for (int i = 0; i < 3; i++) {
		if (std::abs(e[i]) >= m_pinvTolerance)
			sigma[i][i] = 1.0f / e[i];
		else m_featureDim--;
	}
	glm::mat3 ATAp = E * sigma * glm::transpose(E);
	glm::vec3 p = m_pointsSum / float(m_pointsCount);
	glm::vec3 b(A[3][0], A[3][1], A[3][2]);
	glm::vec3 c = ATAp * (AT * b - ATA * p);
	// TODO: replace this hack with proper bounded solver
	return glm::clamp(c + p, min_point, max_point);
}

void QefSolver3D::compressMatrix() noexcept
{
	householder<float, 4, kMaxRows>(A, m_usedRows);
	m_usedRows = 4;
}

}
