#define LUA_LIB
#define GLM_ENABLE_EXPERIMENTAL

#include <cmath>

extern "C" {
	#include "linalg.h"
	#include "math3dfunc.h"
}

#include "util.h"

#ifndef M_PI
#define M_PI 3.1415926536
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/scalar_relational.hpp>
#include <glm/ext/vector_relational.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/ext/vector_common.hpp>
#include <glm/gtx/matrix_decompose.hpp>

static const glm::vec4 XAXIS(1, 0, 0, 0);
static const glm::vec4 YAXIS(0, 1, 0, 0);
static const glm::vec4 ZAXIS(0, 0, 1, 0);
static const glm::vec4 WAXIS(0, 0, 0, 1);

static const glm::vec4 NXAXIS = -XAXIS;
static const glm::vec4 NYAXIS = -YAXIS;
static const glm::vec4 NZAXIS = -ZAXIS;

static inline glm::mat4x4 &
allocmat(struct lastack *LS) {
	float * buf = lastack_allocmatrix(LS);
	return *(glm::mat4x4 *)buf;
}

static inline glm::quat &
allocquat(struct lastack *LS) {
	float * buf = lastack_allocquat(LS);
	return *(glm::quat *)buf;
}

static inline glm::vec4 &
allocvec4(struct lastack *LS) {
	float * buf = lastack_allocvec4(LS);
	return *(glm::vec4 *)buf;
}

static inline glm::vec3 &
allocvec3(struct lastack *LS) {
	float * buf = lastack_allocvec4(LS);
	return *(glm::vec3 *)buf;
}

void
math3d_make_srt(struct lastack *LS, const float *scale, const float *rot, const float *translate) {
	glm::mat4x4 &srt = allocmat(LS);
	if (scale) {
		srt = glm::mat4x4(1);
		srt[0][0] = scale[0];
		srt[1][1] = scale[1];
		srt[2][2] = scale[2];
	}
	if (rot) {
		const glm::quat * q = (const glm::quat *)rot;
		if (scale) {
			srt = glm::mat4x4(*q) * srt;
		} else {
			srt = glm::mat4x4(*q);
		}
	} else if (scale == NULL) {
		srt = glm::mat4x4(1);
	}
	if (translate) {
		srt[3][0] = translate[0];
		srt[3][1] = translate[1];
		srt[3][2] = translate[2];
		srt[3][3] = 1;
	}
}

void
math3d_make_quat_from_euler(struct lastack *LS, float x, float y, float z) {
	glm::vec3 r(x,y,z);
	glm::quat q(r);
	lastack_pushquat(LS, &q[0]);
}

void
math3d_make_quat_from_axis(struct lastack *LS, const float *axis, float radian) {
	glm::vec3 a(axis[0],axis[1],axis[2]);
	glm::quat &q = allocquat(LS);

	q = glm::angleAxis(radian, a);
}

#define MAT(v) (*(const glm::mat4x4 *)(v))
#define VEC(v) (*(const glm::vec4 *)(v))
#define VEC3(v) (*(const glm::vec3 *)(v))
#define QUAT(v) (*(const glm::quat *)(v))

void
math3d_mul_matrix(struct lastack *LS, const float val0[16], const float val1[16], float result[16]) {
	glm::mat4x4 &mat = *(glm::mat4x4 *)result;
	mat = MAT(val0) * MAT(val1);
}

void
math3d_mul_vec4(struct lastack *LS, const float val0[4], const float val1[4], float result[4]) {
	glm::vec4 &vec = *(glm::vec4 *)result;
	vec = VEC(val0) * VEC(val1);
}

void
math3d_mul_quat(struct lastack *LS, const float val0[4], const float val1[4], float result[4]) {
	glm::quat &quat = *(glm::quat *)result;
	quat = QUAT(val0) * QUAT(val1);
}

void
math3d_add_vec(struct lastack *LS, const float lhs[4], const float rhs[4], float r[4]){
	*(glm::vec4*)r = VEC(lhs) + VEC(rhs);
}

void
math3d_sub_vec(struct lastack *LS, const float lhs[4], const float rhs[4], float r[4]){
	*(glm::vec4*)r = VEC(lhs) - VEC(rhs);
}

// epsilon for pow2
//#define EPSILON 0.00001f
// glm::equal(dot , 1.0f, EPSILON)

static inline int
equal_one(float f) {
	union {
		float f;
		uint32_t n;
	} u;
	u.f = f;
	return ((u.n + 0x1f) & ~0x3f) == 0x3f800000;	// float 1
}

int
math3d_decompose_scale(const float mat[16], float scale[4]) {
	const glm::mat4& m = MAT(mat);
	int ii;
	scale[3] = 0;
	for (ii = 0; ii < 3; ++ii) {
		const float* v = &m[ii].x;
		float dot = glm::dot(VEC3(v), VEC3(v));
		if (equal_one(dot)) {
			scale[ii] = 1.0f;
		} else {
			scale[ii] = sqrtf(dot);
			if (scale[ii] == 0) {
				// invalid scale, use 1 instead
				scale[0] = scale[1] = scale[2] = 1.0f;
				return 1;
			}
		}
		if (glm::determinant(m) < 0){
			scale[ii] *= -1;
		}
	}
	if (scale[0] == 1.0f && scale[1] == 1.0f && scale[2] == 1.0f) {
		return 1;
	}
	return 0;
}

void
math3d_decompose_rot(const float mat[16], float quat[4]) {
	glm::quat &q = *(glm::quat *)quat;
	glm::mat4 rotMat(MAT(mat));
	float scale[4];
	if (math3d_decompose_scale(mat, scale) == 0) {
		int ii;
		for (ii = 0; ii < 3; ++ii) {
			rotMat[ii] /= scale[ii];
		}
	}
	q = glm::quat_cast(rotMat);
}

// see: https://github.com/microsoft/DirectXMath/blob/7c30ba5932e081ca4d64ba4abb8a8986a7444ec9/Inc/DirectXMathMatrix.inl#L1111
static bool inline decompose(const glm::mat4& matrix, glm::vec3& scale, glm::quat& rotation, glm::vec3& translation)
{
	bool result = true;

	glm::vec3* scaleBase = &scale;
	float* pfScales = (float*)scaleBase;
	const float EPSILON = 0.0001f;
	float det;

	glm::vec3 vectorBasis[3];
	glm::vec3** pVectorBasis = (glm::vec3**)&vectorBasis;

	glm::mat4 matTemp(1.f);
	glm::vec3 canonicalBasis[3] = {
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f),
	};
	glm::vec3* pCanonicalBasis = &canonicalBasis[0];

	translation = matrix[3];

	pVectorBasis[0] = (glm::vec3*)&matTemp[0];
	pVectorBasis[1] = (glm::vec3*)&matTemp[1];
	pVectorBasis[2] = (glm::vec3*)&matTemp[2];

	*(pVectorBasis[0]) = matrix[0];//new Vector3(matrix.M11, matrix.M12, matrix.M13);
	*(pVectorBasis[1]) = matrix[1];//new Vector3(matrix.M21, matrix.M22, matrix.M23);
	*(pVectorBasis[2]) = matrix[2];//new Vector3(matrix.M31, matrix.M32, matrix.M33);

	scale[0] = glm::length(*pVectorBasis[0]);
	scale[1] = glm::length(*pVectorBasis[1]);
	scale[2] = glm::length(*pVectorBasis[2]);

	uint32_t a, b, c;
	float x = pfScales[0], y = pfScales[1], z = pfScales[2];
	if (x < y)
	{
		if (y < z)
		{
			a = 2;
			b = 1;
			c = 0;
		}
		else
		{
			a = 1;

			if (x < z)
			{
				b = 2;
				c = 0;
			}
			else
			{
				b = 0;
				c = 2;
			}
		}
	}
	else
	{
		if (x < z)
		{
			a = 2;
			b = 0;
			c = 1;
		}
		else
		{
			a = 0;

			if (y < z)
			{
				b = 2;
				c = 1;
			}
			else
			{
				b = 1;
				c = 2;
			}
		}
	}

	if (pfScales[a] < EPSILON)
	{
		*(pVectorBasis[a]) = pCanonicalBasis[a];
	}

	*pVectorBasis[a] = glm::normalize(*pVectorBasis[a]);

	if (pfScales[b] < EPSILON)
	{
		uint32_t cc;
		glm::vec3 absV = glm::abs(*pVectorBasis[a]);
		if (absV.x < absV.y)
		{
			if (absV.y < absV.z)
			{
				cc = 0;
			}
			else
			{
				if (absV.x < absV.z)
				{
					cc = 0;
				}
				else
				{
					cc = 2;
				}
			}
		}
		else
		{
			if (absV.x < absV.z)
			{
				cc = 1;
			}
			else
			{
				if (absV.y < absV.z)
				{
					cc = 1;
				}
				else
				{
					cc = 2;
				}
			}
		}

		*pVectorBasis[b] = glm::cross(*pVectorBasis[a], *(pCanonicalBasis + cc));
	}

	*pVectorBasis[b] = glm::normalize(*pVectorBasis[b]);

	if (pfScales[c] < EPSILON)
	{
		*pVectorBasis[c] = glm::cross(*pVectorBasis[a], *pVectorBasis[b]);
	}

	*pVectorBasis[c] = glm::normalize(*pVectorBasis[c]);

	det = glm::determinant(matTemp);

	// use Kramer's rule to check for handedness of coordinate system
	if (det < 0.0f)
	{
		// switch coordinate system by negating the scale and inverting the basis vector on the x-axis
		pfScales[a] = -pfScales[a];
		*pVectorBasis[a] = -(*pVectorBasis[a]);

		det = -det;
	}

	det -= 1.0f;
	det *= det;

	if ((EPSILON < det))
	{
		// Non-SRT matrix encountered
		rotation = glm::quat(0.f, 0.f, 0.f, 0.f);

		result = false;
	}
	else
	{
		// generate the quaternion from the matrix
		rotation = glm::quat(glm::mat3(matTemp));
	}

	return result;
}

void
math3d_decompose_matrix(struct lastack *LS, const float *mat) {
	const glm::mat4x4 &m = *(const glm::mat4x4 *)mat;

	lastack_preallocfloat4(LS, 3);
	auto& trans = allocvec3(LS);
	auto& q = allocquat(LS);
	auto& scale = allocvec3(LS);
	//decompose(m, *(glm::vec3*)scale, q, *(glm::vec3*)trans);
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(m, scale, q, trans, skew, perspective);
}

float
math3d_length(const float *v) {
	return glm::length(VEC3(v));
}

void
math3d_floor(struct lastack *LS, const float v[4]) {
	auto& vv = allocvec4(LS);
	vv = glm::floor(VEC(v));
	vv[3] = 0;
}

void
math3d_ceil(struct lastack *LS, const float v[4]) {
	auto& vv = allocvec4(LS);
	vv = glm::ceil(VEC(v));
	vv[3] = 0;
}

float
math3d_dot(const float v1[4], const float v2[4]) {
	return glm::dot(VEC3(v1), VEC3(v2));
}

void
math3d_cross(struct lastack *LS, const float v1[4], const float v2[4]) {
	glm::vec4 &r = allocvec4(LS);
	r = glm::vec4(glm::cross(VEC3(v1), VEC3(v2)), 0.0);
}

void
math3d_mulH(struct lastack *LS, const float mat[16], const float vec[4]) {
	glm::vec4 &r = allocvec4(LS);

	if (vec[3] != 1.f){
		float tmp[4] = { vec[0], vec[1], vec[2], 1 };
		r = MAT(mat) * VEC(tmp);
	} else {
		r = MAT(mat) * VEC(vec);
	}

	if (r.w != 0) {
		r /= fabs(r.w);
		r.w = 1.f;
	}
}

void
math3d_normalize_vector(struct lastack *LS, const float v[4]) {
	glm::vec3 v3 = glm::normalize(VEC3(v));
	glm::vec4 &r = allocvec4(LS);
	r[0] = v3[0];
	r[1] = v3[1];
	r[2] = v3[2];
	r[3] = v[3];
}

void
math3d_normalize_quat(struct lastack *LS, const float v[4]) {
	glm::quat &q = allocquat(LS);
	q = glm::normalize(QUAT(v));
}

void
math3d_transpose_matrix(struct lastack *LS, const float mat[16]) {
	glm::mat4x4 &r = allocmat(LS);
	r = glm::transpose(MAT(mat));
}

void
math3d_inverse_matrix(struct lastack *LS, const float mat[16]) {
	glm::mat4x4 &r = allocmat(LS);
	r = glm::inverse(MAT(mat));
}

void
math3d_inverse_matrix_fast(struct lastack *LS, const float mat[16]) {
	glm::mat4x4 &r = allocmat(LS);
	auto &m = MAT(mat);
	glm::mat3x3 m3(m);

	auto d01 = glm::dot(m3[0], m3[1]);
	auto d12 = glm::dot(m3[1], m3[2]);
	auto d20 = glm::dot(m3[2], m3[0]);
	//assert(is_zero() && is_zero(glm::dot(m3[1], m3[2])) && is_zero(glm::dot(m3[2], m3[0])));
	assert(is_zero(d01, 10e-6f) && is_zero(d12, 10e-6f) && is_zero(d20, 10e-6f));
	glm::transpose(m3);
	r = glm::mat4(m3) * glm::translate(glm::mat4(1.f), glm::vec3(-m[3]));
}

void
math3d_inverse_quat(struct lastack *LS, const float quat[4]) {
	glm::quat &q = allocquat(LS);
	q = glm::inverse(QUAT(quat));
}

void
math3d_lookat_matrix(struct lastack *LS, int direction, const float eye[3], const float at[3], const float *up) {
	glm::mat4x4 &m = allocmat(LS);
	if (up == NULL) {
		static const float default_up[3] = {0,1,0};
		up = default_up;
	}
	if (direction) {
		const glm::vec3 vat = VEC3(eye) + VEC3(at);
		m = glm::lookAtLH(VEC3(eye), vat, VEC3(up));
	} else {
		m = glm::lookAtLH(VEC3(eye), VEC3(at), VEC3(up));
	}
}

void
math3d_quat_to_matrix(struct lastack *LS, const float quat[4]) {
	glm::mat4x4 &m = allocmat(LS);
	m = glm::mat4x4(QUAT(quat));
}

void
math3d_matrix_to_quat(struct lastack *LS, const float mat[16]) {
	glm::quat &q = allocquat(LS);
	q = glm::quat_cast(MAT(mat));
}

void
math3d_reciprocal(struct lastack *LS, const float v[4]) {
	glm::vec4 &vv = allocvec4(LS);
	vv = 1.f / VEC(v);
	vv[3] = v[3];
}

void
math3d_quat_to_viewdir(struct lastack *LS, const float q[4]) {
	glm::vec4 &d = allocvec4(LS);
	d = glm::rotate(QUAT(q), glm::vec4(0, 0, 1, 0));
}

void
math3d_rotmat_to_viewdir(struct lastack *LS, const float m[16]) {
	glm::vec4 &d = allocvec4(LS);
	d = MAT(m) * glm::vec4(0, 0, 1, 0);
}

void
math3d_quat_between_2vectors(struct lastack *LS, const float v0[3], const float v1[3]){
	glm::quat &q = allocquat(LS);
	q = glm::quat(VEC3(v0), VEC3(v1));
}

void
math3d_viewdir_to_quat(struct lastack *LS, const float v[3]) {
	float vv[3] = {0, 0, 1};
	math3d_quat_between_2vectors(LS, vv, v);
}

void
math3d_perspectiveLH(struct lastack *LS, float fov, float aspect, float near, float far, int homogeneous_depth){
	glm::mat4x4 &mat = allocmat(LS);
	mat = homogeneous_depth ?
		glm::perspectiveLH_NO(fov, aspect, near, far) :
		glm::perspectiveLH_ZO(fov, aspect, near, far);
}

void
math3d_frustumLH(struct lastack *LS, float left, float right, float bottom, float top, float near, float far, int homogeneous_depth) {
	glm::mat4x4 &mat = allocmat(LS);
	mat = homogeneous_depth ?
		glm::frustumLH_NO(left, right, bottom, top, near, far) :
		glm::frustumLH_ZO(left, right, bottom, top, near, far);
}

void
math3d_orthoLH(struct lastack *LS, float left, float right, float bottom, float top, float near, float far, int homogeneous_depth) {
	glm::mat4x4 &mat = allocmat(LS);
	mat = homogeneous_depth ?
		glm::orthoLH_NO(left, right, bottom, top, near, far) :
		glm::orthoLH_ZO(left, right, bottom, top, near, far);
}

void
math3d_base_axes(struct lastack *LS, const float forward[4]) {
	lastack_preallocfloat4(LS, 2);
	glm::vec4 &up = allocvec4(LS);
	glm::vec4 &right = allocvec4(LS);

	if (is_equal(VEC(forward), ZAXIS)) {
		up = YAXIS;
		right = XAXIS;
	} else {
		if (is_equal(VEC(forward), YAXIS)) {
			up = NZAXIS;
			right = XAXIS;
		} else if (is_equal(VEC(forward), NYAXIS)) {
			up = ZAXIS;
			right = XAXIS;
		} else {
			right = glm::vec4(glm::normalize(glm::cross(VEC3(&YAXIS.x), VEC3(forward))), 0);
			up = glm::vec4(glm::normalize(glm::cross(VEC3(forward), VEC3(&right.x))), 0);
		}
	}
}

void
math3d_quat_transform(struct lastack *LS, const float quat[4], const float v[4]){
	glm::vec4 &vv = allocvec4(LS);
	vv = glm::rotate(QUAT(quat), VEC(v));
}

void
math3d_rotmat_transform(struct lastack *LS, const float mat[16], const float v[4]){
	glm::vec4 &vv = allocvec4(LS);
	vv = MAT(mat) * VEC(v);
}

void
math3d_minmax(struct lastack *LS, const float mat[16], const float v[4], float minv[4], float maxv[4]){
	const glm::vec4 vv = mat ? MAT(mat) * VEC(v) : VEC(v);
	*(glm::vec4*)maxv = glm::max(vv, VEC(maxv));
	*(glm::vec4*)minv = glm::min(vv, VEC(minv));
}

void 
math3d_lerp(struct lastack *LS, const float v0[4], const float v1[4], float ratio, float r[4]){
	*(glm::vec4*)r = glm::lerp(VEC(v0), VEC(v1), ratio);
}

void 
math3d_quat_lerp(struct lastack *LS, const float v0[4], const float v1[4], float ratio, float r[4]){
	*(glm::quat*)r = glm::lerp(QUAT(v0), QUAT(v1), ratio);
}

void 
math3d_quat_slerp(struct lastack *LS, const float v0[4], const float v1[4], float ratio, float r[4]){
	*(glm::quat*)r = glm::slerp(QUAT(v0), QUAT(v1), ratio);
}

// x: pitch(-90, 90), y: yaw(-180, 180), z: roll(-180, 180),
static glm::vec3 limit_euler_angles(const glm::quat& q) {
	static const float MY_PI = 3.14159265358979323846264338327950288f;
	float x_ = q.x;
	float y_ = q.y;
	float z_ = q.z;
	float w_ = q.w;
	// Derivation from http://www.geometrictools.com/Documentation/EulerAngles.pdf
	// Order of rotations: Z first, then X, then Y
	float check = 2.0f * (-y_ * z_ + w_ * x_);
	if (check < -0.995f) {
		return {
			-0.5f * MY_PI,
			0.0f,
			-atan2f(2.0f * (x_ * z_ - w_ * y_), 1.0f - 2.0f * (y_ * y_ + z_ * z_))
		};
	} else if (check > 0.995f) {
		return {
			0.5f * MY_PI,
			0.0f,
			atan2f(2.0f * (x_ * z_ - w_ * y_), 1.0f - 2.0f * (y_ * y_ + z_ * z_))
		};
	} else {
		return {
			asinf(check),
			atan2f(2.0f * (x_ * z_ + w_ * y_), 1.0f - 2.0f * (x_ * x_ + y_ * y_)),
			atan2f(2.0f * (x_ * y_ + w_ * z_), 1.0f - 2.0f * (x_ * x_ + z_ * z_))
		};
	}
}

void 
math3d_quat_to_euler(struct lastack *LS, const float q[4], float euler[4]) {
	*(glm::vec3*)euler = limit_euler_angles(QUAT(q)); // glm::eulerAngles(QUAT(q)); // 
}

void
math3d_dir2radian(struct lastack *LS, const float v[4], float radians[2]){
	const float PI = float(M_PI);
	const float HALF_PI = 0.5f * PI;
	
	if (is_equal(v[1], 1.f)){
		radians[0] = -HALF_PI;
		radians[1] = 0.f;
	} else if (is_equal(v[1], -1.f)){
		radians[0] = HALF_PI;
		radians[1] = 0.f;
	} else if (is_equal(v[0], 1.f)){
		radians[0] = 0.f;
		radians[1] = HALF_PI;
	} else if (is_equal(v[0], -1.f)){
		radians[0] = 0.f;
		radians[1] = -HALF_PI;
	} else if (is_equal(v[2], 1.f)){
		radians[0] = 0.f;
		radians[1] = 0.f;
	} else if (is_equal(v[2], -1.f)){
		radians[0] = 0.f;
		radians[1] = PI;
	} else {
		radians[0] = is_zero(v[1]) ? 0.f : std::asin(-v[1]);
		radians[1] = is_zero(v[0]) ? 0.f : std::atan2(v[0], v[2]);
	}
}

//plane
float
math3d_plane_normalize(struct lastack *LS, const float plane[4], float nplane[4]){
	const float l = glm::length(VEC3(plane));
	*(glm::vec4*)(nplane) = (*(glm::vec4*)plane) / l;
	return l;
}

float
math3d_plane_point_distance(struct lastack *LS, const float plane[4], const float point[4]){
	assert(std::abs(1.f - glm::length(VEC3(plane))) < 1e-7);
	return glm::dot(VEC3(plane), VEC3(point)) - plane[3];
}

//aabb
#define AABB_MIN(_V) *((glm::vec4 *)(_V))
#define AABB_MAX(_V) *((glm::vec4 *)(_V) + 1)

#define CAABB_MIN(_V) *((const glm::vec4 *)(_V))
#define CAABB_MAX(_V) *((const glm::vec4 *)(_V) + 1)

void 
math3d_aabb_append(struct lastack *LS, const float v[4], float *aabb){
	auto &minv = AABB_MIN(aabb);
	auto &maxv = AABB_MAX(aabb);

	minv = glm::min(minv, VEC(v));
	maxv = glm::max(maxv, VEC(v));
}

void 
math3d_aabb_merge(struct lastack *LS, const float *lhsaabb, const float *rhsaabb, float *raabb){
	auto &rmin = AABB_MIN(raabb);
	auto &rmax = AABB_MAX(raabb);

	rmin = glm::min(CAABB_MIN(lhsaabb), CAABB_MIN(rhsaabb));
	rmax = glm::max(CAABB_MAX(lhsaabb), CAABB_MAX(rhsaabb));
}

int 
math3d_aabb_isvalid(struct lastack *LS, const float *aabb){
	const auto& minv = CAABB_MIN(aabb);
	const auto& maxv = CAABB_MAX(aabb);

	return (minv.x < maxv.x && minv.y < maxv.y && minv.z < maxv.z) ? 1 : 0;
}

void 
math3d_aabb_transform(struct lastack *LS, const float trans[16], const float aabb[16], float raabb[16]){
	const auto& t = MAT(trans);

	const auto& minv = CAABB_MIN(aabb);
	const auto& maxv = CAABB_MAX(aabb);

	const glm::vec4 &right	= t[0];
	const glm::vec4 &up 	= t[1];
	const glm::vec4 &forward= t[2];
	const glm::vec4 &pos 	= t[3];

	const glm::vec4 xa = right * minv.x;
	const glm::vec4 xb = right * maxv.x;

	const glm::vec4 ya = up * minv.y;
	const glm::vec4 yb = up * maxv.y;

	const glm::vec4 za = forward * minv.z;
	const glm::vec4 zb = forward * maxv.z;

	auto &rmin = AABB_MIN(raabb);
	auto &rmax = AABB_MAX(raabb);

	rmin = glm::min(xa, xb) + glm::min(ya, yb) + glm::min(za, zb) + pos;
	rmax = glm::max(xa, xb) + glm::max(ya, yb) + glm::max(za, zb) + pos;
}

int
math3d_aabb_test_point(struct lastack *LS, const float *aabb, const float *p){
	const auto & minv = AABB_MIN(aabb);
	const auto & maxv = AABB_MAX(aabb);

	for (int ii=0;ii<3;++ii){
		if (minv[ii] > p[ii] || maxv[ii] < p[ii])
			return 0;
	}

	return 1;
}

void
math3d_aabb_center_extents(struct lastack *LS, const float *aabb, float center[4], float extents[4]){
	const auto & minv = AABB_MIN(aabb);
	const auto & maxv = AABB_MAX(aabb);

	*(glm::vec4*)center = (maxv+minv)*0.5f;
	*(glm::vec4*)extents = (maxv-minv)*0.5f;
}

float
math3d_aabb_diagonal_length(struct lastack *LS, const float *aabb){
	const auto & minv = AABB_MIN(aabb);
	const auto & maxv = AABB_MIN(aabb);

	return glm::length(VEC3(&maxv.x) - VEC3(&minv.x));
}

static inline int
plane_intersect(const glm::vec4& plane, const float* aabb) {
	const auto& min = CAABB_MIN(aabb);
	const auto& max = CAABB_MAX(aabb);
	float minD, maxD;
	if (plane.x > 0.0f) {
		minD = plane.x * min.x;
		maxD = plane.x * max.x;
	}
	else {
		minD = plane.x * max.x;
		maxD = plane.x * min.x;
	}

	if (plane.y > 0.0f) {
		minD += plane.y * min.y;
		maxD += plane.y * max.y;
	}
	else {
		minD += plane.y * max.y;
		maxD += plane.y * min.y;
	}

	if (plane.z > 0.0f) {
		minD += plane.z * min.z;
		maxD += plane.z * max.z;
	}
	else {
		minD += plane.z * max.z;
		maxD += plane.z * min.z;
	}

	// in front of the plane
	if (minD >= plane.w) {
		return 1;
	}

	// in back of the plane
	if (maxD <= plane.w) {
		return -1;
	}

	// straddle of the plane
	return 0;
}


int
math3d_aabb_intersect_plane(struct lastack *LS, const float *aabb, const float plane[4]){
	return plane_intersect(VEC(plane), aabb);
}

void
math3d_aabb_intersetion(struct lastack *LS, const float *lhsaabb, const float *rhsaabb){
	glm::mat4 &m = allocmat(LS);
	m[0] = glm::max(CAABB_MIN(lhsaabb), CAABB_MIN(rhsaabb));
	m[1] = glm::min(CAABB_MAX(lhsaabb), CAABB_MAX(rhsaabb));
}

void
math3d_aabb_points(struct lastack *LS, const float *aabb, float *points[8]){
	const auto &minv = CAABB_MIN(aabb);
	const auto &maxv = CAABB_MAX(aabb);

	#define V(_p) *((glm::vec4*)(_p))
	V(points[0]) = minv;
	V(points[1]) = glm::vec4(minv.x, maxv.y, minv.z, 1.f);
	V(points[2]) = glm::vec4(maxv.x, minv.y, minv.z, 1.f);
	V(points[3]) = glm::vec4(maxv.x, maxv.y, minv.z, 1.f);

	V(points[4]) = glm::vec4(minv.x, minv.y, maxv.z, 1.f);
	V(points[5]) = glm::vec4(minv.x, maxv.y, maxv.z, 1.f);
	V(points[6]) = glm::vec4(maxv.x, minv.y, maxv.z, 1.f);
	V(points[7]) = maxv;
	#undef V
}

void
math3d_aabb_expand(struct lastack *LS, const float *aabb, const float e[4]){
	glm::mat4 &m = allocmat(LS);
	m[0] = CAABB_MIN(aabb) - VEC(e);
	m[0][3] = 1.f;
	m[1] = CAABB_MAX(aabb) + VEC(e);
	m[1][3] = 1.f;
}

// vplane [left, right, bottom, top, near, far]
enum PlaneName{
	PN_left = 0,
	PN_right,
	PN_bottom,
	PN_top,
	PN_near,
	PN_far,
};

void
math3d_frustum_planes(struct lastack *LS, const float m[16], float *planes[6], int homogeneous_depth){
	const auto &mat = MAT(m);
	const auto& c0 = mat[0], &c1 = mat[1], & c2 = mat[2], & c3 = mat[3];

	auto& leftplane = planes[PN_left];
	leftplane[0] = c0[0] + c0[3];
	leftplane[1] = c1[0] + c1[3];
	leftplane[2] = c2[0] + c2[3];
	leftplane[3] = c3[0] + c3[3];

	auto& rightplane = planes[PN_right];
	rightplane[0] = c0[3] - c0[0];
	rightplane[1] = c1[3] - c1[0];
	rightplane[2] = c2[3] - c2[0];
	rightplane[3] = c3[3] - c3[0];

	auto& bottomplane = planes[PN_bottom];
	bottomplane[0] = c0[3] + c0[1];
	bottomplane[1] = c1[3] + c1[1];
	bottomplane[2] = c2[3] + c2[1];
	bottomplane[3] = c3[3] + c3[1];

	auto& topplane = planes[PN_top];
	topplane[0] = c0[3] - c0[1];
	topplane[1] = c1[3] - c1[1];
	topplane[2] = c2[3] - c2[1];
	topplane[3] = c3[3] - c3[1];

	auto& nearplane = planes[PN_near];
	if (homogeneous_depth) {
		nearplane[0] = c0[3] + c0[2];
		nearplane[1] = c1[3] + c1[2];
		nearplane[2] = c2[3] + c2[2];
		nearplane[3] = c3[3] + c3[2];
	} else {
		nearplane[0] = c0[2];
		nearplane[1] = c1[2];
		nearplane[2] = c2[2];
		nearplane[3] = c3[2];
	}

	auto& farplane = planes[PN_far];
	farplane[0] = c0[3] - c0[2];
	farplane[1] = c1[3] - c1[2];
	farplane[2] = c2[3] - c2[2];
	farplane[3] = c3[3] - c3[2];

	// normalize
	for (int ii = 0; ii < 6; ++ii){
		auto& p = *((glm::vec4*)planes[ii]);
		// out plane defination: n dot p = d
		// this extract is base: a*nx + b*ny + c*nz + d = 0
		// we need to flip side of the 'd'
		p[3] = -p[3];
		auto len = glm::length(VEC3(planes[ii]));
		if (glm::abs(len) >= glm::epsilon<float>())
			p /= len;
	}
}

// point: [
//	lbn, ltn, rbn, rtn, 
//	lbf, ltf, rbf, rtf, 
//]
static const glm::vec4 ndc_points_ZO[8] = {
	glm::vec4(-1.f,-1.f, 0.f, 1.f),
	glm::vec4(-1.f, 1.f, 0.f, 1.f),
	glm::vec4( 1.f,-1.f, 0.f, 1.f),
	glm::vec4( 1.f, 1.f, 0.f, 1.f),

	glm::vec4(-1.f,-1.f, 1.f, 1.f),
	glm::vec4(-1.f,1.f,  1.f, 1.f),
	glm::vec4(1.f, -1.f, 1.f, 1.f),
	glm::vec4(1.f, 1.f,  1.f, 1.f),
};

static const glm::vec4 ndc_points_NO[8] = {
	glm::vec4(-1.f,-1.f, -1.f, 1.f),
	glm::vec4(-1.f, 1.f, -1.f, 1.f),
	glm::vec4( 1.f, -1.f,-1.f, 1.f),
	glm::vec4( 1.f,  1.f,-1.f, 1.f),

	glm::vec4(-1.f,-1.f, 1.f, 1.f),
	glm::vec4(-1.f, 1.f, 1.f, 1.f),
	glm::vec4( 1.f,-1.f, 1.f, 1.f),
	glm::vec4( 1.f, 1.f, 1.f, 1.f),
};

void 
math3d_frustum_points(struct lastack *LS, const float m[16], float* points[8], int homogeneous_depth){
	auto invmat = glm::inverse(MAT(m));
	const auto &pp = homogeneous_depth ? ndc_points_NO : ndc_points_ZO;
	for (int ii = 0; ii < 8; ++ii){
		auto &p = *(glm::vec4*)points[ii];
		p = invmat * pp[ii];
		p /= p.w;
	}
}

// frustum
int 
math3d_frustum_intersect_aabb(struct lastack *LS, const float* planes[6], const float *aabb){
	for (int ii = 0; ii < 6; ++ii){
		const int r = plane_intersect(VEC(planes[ii]), aabb);
		// intersect or outside frustum
		if (r <= 0){
			return r;
		}
	}

	// aabb in front of all planes, mean inside frustum
	return 1;
}

void
math3d_frusutm_aabb(struct lastack *LS, const float* points[8], float *aabb){
	auto& minv = AABB_MIN(aabb);
	auto& maxv = AABB_MAX(aabb);
	
	minv = glm::vec4(std::numeric_limits<float>::max()), 
	maxv = glm::vec4(std::numeric_limits<float>::lowest());

	for (int ii = 0; ii < 8; ++ii){
		const auto &p = VEC(points[ii]);
		minv = glm::min(minv, p);
		maxv = glm::max(maxv, p);
	}
}

void
math3d_frustum_center(struct lastack *LS, const float* points[8], float *center){
	auto &c = *(glm::vec4*)(center);
	c = glm::vec4(0, 0, 0, 1);
	for (int ii = 0; ii < 8; ++ii){
		c += VEC(points[ii]);
	}

	c /= 8.f;
	c.w = 1.f;
}

float
math3d_frustum_max_radius(struct lastack *LS, const float* points[8], const float center[4]){
	float maxradius = 0;
	const auto &c = VEC(center);
	for (int ii = 0; ii < 8; ++ii){
		const auto &p = VEC(points[ii]);
		maxradius = glm::max(glm::length(p - c), maxradius);
	}

	return maxradius;
}

void math3d_frustum_calc_near_far(struct lastack *LS, const float* planes[6], float nearfar[2]){

}

float
math3d_point2plane(struct lastack *LS, const float pt[4], const float plane[4]){
	return glm::dot(VEC3(pt), VEC3(plane)) + plane[4];
}