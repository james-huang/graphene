/* graphene-quaternion.c: Quaternion
 *
 * Copyright © 2014  Emmanuele Bassi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * SECTION:graphene-quaternion
 * @title: Quaternion
 * @short_description: Quaternion operations
 *
 * Quaternions are a mathematical entity that can be used to represent
 * rotation transformations in 3D space; unlike the usual Euler representation
 * with roll, pitch, and yaw, quaternions do not suffer from the so-called
 * ["Gimbal Lock"](http://en.wikipedia.org/wiki/Gimbal_lock) problem.
 *
 * See also: #graphene_euler_t
 */

#include "graphene-private.h"

#include "graphene-quaternion.h"

#include "graphene-euler.h"
#include "graphene-matrix.h"
#include "graphene-point3d.h"
#include "graphene-simd4f.h"
#include "graphene-simd4x4f.h"
#include "graphene-vec3.h"
#include "graphene-vec4.h"

#include <math.h>

/**
 * graphene_quaternion_alloc: (constructor)
 *
 * Allocates a new #graphene_quaternion_t.
 *
 * The contents of the returned value are undefined.
 *
 * Returns: (transfer full): the newly allocated #graphene_quaternion_t
 *
 * Since: 1.0
 */
graphene_quaternion_t *
graphene_quaternion_alloc (void)
{
  return calloc (1, sizeof (graphene_quaternion_t));
}

/**
 * graphene_quaternion_free:
 * @q: a #graphene_quaternion_t
 *
 * Releases the resources allocated by graphene_quaternion_alloc().
 *
 * Since: 1.0
 */
void
graphene_quaternion_free (graphene_quaternion_t *q)
{
  free (q);
}

/**
 * graphene_quaternion_init:
 * @q: a #graphene_quaternion_t
 * @x: the first component of the quaternion
 * @y: the second component of the quaternion
 * @z: the third component of the quaternion
 * @w: the fourth component of the quaternion
 *
 * Initializes a #graphene_quaternion_t using the given four values.
 *
 * Returns: (transfer none): the initialized quaternion
 *
 * Since: 1.0
 */
graphene_quaternion_t *
graphene_quaternion_init (graphene_quaternion_t *q,
                          float                  x,
                          float                  y,
                          float                  z,
                          float                  w)
{
  q->x = x;
  q->y = y;
  q->z = z;
  q->w = w;

  return q;
}

/**
 * graphene_quaternion_init_identity:
 * @q: a #graphene_quaternion_t
 *
 * Initializes a #graphene_quaternion_t using the identity
 * transformation.
 *
 * Returns: (transfer none): the initialized quaternion
 *
 * Since: 1.0
 */
graphene_quaternion_t *
graphene_quaternion_init_identity (graphene_quaternion_t *q)
{
  q->x = q->y = q->z = 0.f;
  q->w = 1.f;

  return q;
}

/**
 * graphene_quaternion_init_from_quaternion:
 * @q: a #graphene_quaternion_t
 * @src: a #graphene_quaternion_t
 *
 * Initializes a #graphene_quaternion_t with the values from @src.
 *
 * Returns: (transfer none): the initialized quaternion
 *
 * Since: 1.0
 */
graphene_quaternion_t *
graphene_quaternion_init_from_quaternion (graphene_quaternion_t       *q,
                                          const graphene_quaternion_t *src)
{
  *q = *src;

  return q;
}

static graphene_quaternion_t *
graphene_quaternion_init_from_simd4f (graphene_quaternion_t *q,
                                      graphene_simd4f_t      v)
{
  graphene_simd4f_dup_4f (v, (float *) q);

  return q;
}

/**
 * graphene_quaternion_init_from_vec4:
 * @q: a #graphene_quaternion_t
 * @src: a #graphene_vec4_t
 *
 * Initializes a #graphene_quaternion_t with the values from @src.
 *
 * Returns: (transfer none): the initialized quaternion
 *
 * Since: 1.0
 */
graphene_quaternion_t *
graphene_quaternion_init_from_vec4 (graphene_quaternion_t *q,
                                    const graphene_vec4_t *src)
{
  return graphene_quaternion_init_from_simd4f (q, src->value);
}

/**
 * graphene_quaternion_to_vec4:
 * @q: a #graphene_quaternion_t
 * @res: (out caller-allocates): return location for a
 *   #graphene_vec4_t
 *
 * Copies the components of a #graphene_quaternion_t into a
 * #graphene_vec4_t.
 *
 * Since: 1.0
 */
void
graphene_quaternion_to_vec4 (const graphene_quaternion_t *q,
                             graphene_vec4_t             *res)
{
  res->value = graphene_simd4f_init (q->x, q->y, q->z, q->w);
}

/**
 * graphene_quaternion_init_from_matrix:
 * @q: a #graphene_quaternion_t
 * @m: a #graphene_matrix_t
 *
 * Initializes a #graphene_quaternion_t using the rotation components
 * of a transformation matrix.
 *
 * Returns: (transfer none): the initialized quaternion
 *
 * Since: 1.0
 */
graphene_quaternion_t *
graphene_quaternion_init_from_matrix (graphene_quaternion_t   *q,
                                      const graphene_matrix_t *m)
{
  float xx, yy, zz;

  xx = graphene_matrix_get_value (m, 0, 0);
  yy = graphene_matrix_get_value (m, 1, 1);
  zz = graphene_matrix_get_value (m, 2, 2);

  q->w = 0.5f * sqrtf (MAX (1 + xx + yy + zz, 0.f));
  q->x = 0.5f * sqrtf (MAX (1 + xx - yy - zz, 0.f));
  q->y = 0.5f * sqrtf (MAX (1 - xx + yy - zz, 0.f));
  q->z = 0.5f * sqrtf (MAX (1 - xx - yy + zz, 0.f));

  if (graphene_matrix_get_value (m, 2, 1) > graphene_matrix_get_value (m, 1, 2))
    q->x = -q->x;

  if (graphene_matrix_get_value (m, 0, 2) > graphene_matrix_get_value (m, 2, 0))
    q->y = -q->y;

  if (graphene_matrix_get_value (m, 1, 0) > graphene_matrix_get_value (m, 0, 1))
    q->z = -q->z;

  return q;
}

/**
 * graphene_quaternion_to_matrix:
 * @q: a #graphene_quaternion_t
 * @m: (out caller-allocates): a #graphene_matrix_t
 *
 * Converts a quaternion into a transformation matrix expressing
 * the rotation defined by the #graphene_quaternion_t.
 *
 * Since: 1.0
 */
void
graphene_quaternion_to_matrix (const graphene_quaternion_t *q,
                               graphene_matrix_t           *m)
{
  graphene_simd4f_t m_x, m_y, m_z, m_w;

  m_x = graphene_simd4f_init (1.f - 2.f * (q->y * q->y + q->z * q->z),
                                    2.f * (q->x * q->y + q->w * q->z),
                                    2.f * (q->x * q->z - q->w * q->y),
                              0.f);
  m_y = graphene_simd4f_init (      2.f * (q->x * q->y - q->w * q->z),
                              1.f - 2.f * (q->x * q->x + q->z * q->z),
                                    2.f * (q->y * q->z + q->w * q->x),
                              0.f);
  m_z = graphene_simd4f_init (      2.f * (q->x * q->z + q->w * q->y),
                                    2.f * (q->y * q->z - q->w * q->x),
                              1.f - 2.f * (q->x * q->x + q->y * q->y),
                              0.f);
  m_w = graphene_simd4f_init (0.f, 0.f, 0.f, 1.f);

  m->value = graphene_simd4x4f_init (m_x, m_y, m_z, m_w);
}

/**
 * graphene_quaternion_slerp:
 * @a: a #graphene_quaternion_t
 * @b: a #graphene_quaternion_t
 * @factor: the linear interpolation factor
 * @res: (out caller-allocates): return location for the interpolated
 *   quaternion
 *
 * Interpolates between the two given quaternions using a spherical
 * linear interpolation, or [SLERP](http://en.wikipedia.org/wiki/Slerp),
 * using the given interpolation @factor.
 *
 * Since: 1.0
 */
void
graphene_quaternion_slerp (const graphene_quaternion_t *a,
                           const graphene_quaternion_t *b,
                           float                        factor,
                           graphene_quaternion_t       *res)
{
  float theta, r_sin_theta, right_v, left_v, dot;
  graphene_simd4f_t v_a, v_b, left, right, sum;

  v_a = graphene_simd4f_init (a->x, a->y, a->z, a->w);
  v_b = graphene_simd4f_init (b->x, b->y, b->z, b->w);

  dot = CLAMP (graphene_simd4f_get_x (graphene_simd4f_dot4 (v_a, v_b)), -1.f, 1.f);
  if (dot == 1.f)
    {
      *res = *a;
      return;
    }

  theta = acos (dot);
  r_sin_theta = 1.f / sqrtf (1.f - dot * dot);
  right_v = sinf (factor * theta) * r_sin_theta;
  left_v = cosf (factor * theta) - dot * right_v;

  left = graphene_simd4f_init (a->x, a->y, a->z, a->w);
  right = graphene_simd4f_init (b->x, b->y, b->z, b->w);

  left = graphene_simd4f_mul (left, graphene_simd4f_splat (left_v));
  right = graphene_simd4f_mul (right, graphene_simd4f_splat (right_v));
  sum = graphene_simd4f_add (left, right);

  graphene_quaternion_init_from_simd4f (res, sum);
}

/**
 * graphene_quaternion_init_from_angles:
 * @q: a #graphene_quaternion_t
 * @deg_x: rotation angle on the X axis (yaw), in degrees
 * @deg_y: rotation angle on the Y axis (pitch), in degrees
 * @deg_z: rotation angle on the Z axis (roll), in degrees
 *
 * Initializes a #graphene_quaternion_t using the values of
 * the [Euler angles](http://en.wikipedia.org/wiki/Euler_angles)
 * on each axis.
 *
 * See also: graphene_quaternion_init_from_euler()
 *
 * Returns: (transfer none): the initialized quaternion
 *
 * Since: 1.0
 */
graphene_quaternion_t *
graphene_quaternion_init_from_angles (graphene_quaternion_t *q,
                                      float                  deg_x,
                                      float                  deg_y,
                                      float                  deg_z)
{
  float rad_x, rad_y, rad_z;
  float sin_x, sin_y, sin_z;
  float cos_x, cos_y, cos_z;

  rad_x = GRAPHENE_DEG_TO_RAD (deg_x) * .5f;
  rad_y = GRAPHENE_DEG_TO_RAD (deg_y) * .5f;
  rad_z = GRAPHENE_DEG_TO_RAD (deg_z) * .5f;

  graphene_sincos (rad_x, &sin_x, &cos_x);
  graphene_sincos (rad_y, &sin_y, &cos_y);
  graphene_sincos (rad_z, &sin_z, &cos_z);

  q->x = sin_x * cos_y * cos_z + cos_x * sin_y * sin_z;
  q->y = cos_x * sin_y * cos_z - sin_x * cos_y * sin_z;
  q->z = cos_x * cos_y * sin_z + sin_x * sin_y * cos_z;
  q->w = cos_x * cos_y * cos_z - sin_x * sin_y * sin_z;

  return q;
}

/**
 * graphene_quaternion_to_angles:
 * @q: a #graphene_quaternion_t
 * @deg_x: (out) (optional): return location for the rotation angle on
 *   the X axis (yaw), in degress
 * @deg_y: (out) (optional): return location for the rotation angle on
 *   the Y axis (pitch), in degrees
 * @deg_z: (out) (optional): return location for the rotation angle on
 *   the Z axis (roll), in degrees
 *
 * Converts a #graphene_quaternion_t to its corresponding rotations
 * on the [Euler angles](http://en.wikipedia.org/wiki/Euler_angles)
 * on each axis.
 *
 * Since: 1.2
 */
void
graphene_quaternion_to_angles (const graphene_quaternion_t *q,
                               float                       *deg_x,
                               float                       *deg_y,
                               float                       *deg_z)
{
  graphene_vec4_t v;
  graphene_vec4_t sq;
  float qx, qy, qz, qw, sqx, sqy, sqz, sqw;

  graphene_quaternion_to_vec4 (q, &v);
  graphene_vec4_multiply (&v, &v, &sq);

  qx = graphene_vec4_get_x (&v);
  qy = graphene_vec4_get_y (&v);
  qz = graphene_vec4_get_z (&v);
  qw = graphene_vec4_get_w (&v);

  sqx = graphene_vec4_get_x (&sq);
  sqy = graphene_vec4_get_y (&sq);
  sqz = graphene_vec4_get_z (&sq);
  sqw = graphene_vec4_get_w (&sq);

  if (deg_x != NULL)
    {
      float res = atan2f (2 * (qx * qw - qy * qz), (sqw - sqx - sqy + sqz));

      *deg_x = GRAPHENE_RAD_TO_DEG (res);
    }

  if (deg_y != NULL)
    {
      float res = asinf (CLAMP (2 * ( qx * qz + qy * qw), -1, 1));

      *deg_y = GRAPHENE_RAD_TO_DEG (res);
    }

  if (deg_z != NULL)
    {
      float res = atan2f (2 * (qz * qw - qx * qy), (sqw + sqx - sqy - sqz));

      *deg_z = GRAPHENE_RAD_TO_DEG (res);
    }
}

/**
 * graphene_quaternion_init_from_angle_vec3:
 * @q: a #graphene_quaternion_t
 * @angle: the rotation on a given axis, in degrees
 * @axis: the axis of rotation, expressed as a vector
 *
 * Initializes a #graphene_quaternion_t using an @angle on a
 * specific @axis.
 *
 * Returns: (transfer none): the initialized quaternion
 *
 * Since: 1.0
 */
graphene_quaternion_t *
graphene_quaternion_init_from_angle_vec3 (graphene_quaternion_t *q,
                                          float                  angle,
                                          const graphene_vec3_t *axis)
{
  float rad, sin_a, cos_a;
  graphene_simd4f_t axis_n;

  rad = GRAPHENE_DEG_TO_RAD (angle) / 2.f;
  graphene_sincos (rad, &sin_a, &cos_a);

  axis_n = graphene_simd4f_mul (graphene_simd4f_normalize3 (axis->value),
                                graphene_simd4f_splat (sin_a));

  q->x = graphene_simd4f_get_x (axis_n);
  q->y = graphene_simd4f_get_y (axis_n);
  q->z = graphene_simd4f_get_z (axis_n);
  q->w = cos_a;

  return q;
}

/**
 * graphene_quaternion_to_angle_vec3:
 * @q: a #graphene_quaternion_t
 * @angle: (out): return location for the angle, in degrees
 * @axis: (out caller-allocates): return location for the rotation axis
 *
 * Converts a quaternion into an @angle, @axis pair.
 *
 * Since: 1.0
 */
void
graphene_quaternion_to_angle_vec3 (const graphene_quaternion_t *q,
                                   float                       *angle,
                                   graphene_vec3_t             *axis)
{
  graphene_quaternion_t q_n;
  float cos_a;

  graphene_quaternion_normalize (q, &q_n);

  cos_a = q_n.w;

  if (angle != NULL)
    *angle = GRAPHENE_RAD_TO_DEG (acosf (cos_a) * 2.f);

  if (axis != NULL)
    {
      float sin_a = sqrtf (1.f - cos_a * cos_a);

      if (fabsf (sin_a) < 0.00005)
       sin_a = 1.f;

      graphene_vec3_init (axis, q_n.x / sin_a, q_n.y / sin_a, q_n.z / sin_a);
    }
}

/**
 * graphene_quaternion_init_from_euler:
 * @q: the #graphene_quaternion_t to initialize
 * @e: a #graphene_euler_t
 *
 * Initializes a #graphene_quaternion_t using the given #graphene_euler_t.
 *
 * Returns: (transfer none): the initialized #graphene_quaternion_t
 *
 * Since: 1.2
 */
graphene_quaternion_t *
graphene_quaternion_init_from_euler (graphene_quaternion_t  *q,
                                     const graphene_euler_t *e)
{
  graphene_euler_order_t order = graphene_euler_get_order (e);
  float ex = GRAPHENE_DEG_TO_RAD (graphene_euler_get_x (e)) / 2.f;
  float ey = GRAPHENE_DEG_TO_RAD (graphene_euler_get_y (e)) / 2.f;
  float ez = GRAPHENE_DEG_TO_RAD (graphene_euler_get_z (e)) / 2.f;
  float x = 0.f, y = 0.f, z = 0.f, w = 1.f;
  float c1, c2, c3, s1, s2, s3;

  graphene_sincos (ex, &s1, &c1);
  graphene_sincos (ey, &s2, &c2);
  graphene_sincos (ez, &s3, &c3);

  switch (order)
    {
    case GRAPHENE_EULER_ORDER_XYZ:
      x = s1 * c2 * c3 + c1 * s2 * s3;
      y = c1 * s2 * c3 - s1 * c2 * s3;
      z = c1 * c2 * s3 + s1 * s2 * c3;
      w = c1 * c2 * c3 - s1 * s2 * s3;
      break;

    case GRAPHENE_EULER_ORDER_YXZ:
      x = s1 * c2 * c3 + c1 * s2 * s3;
      y = c1 * s2 * c3 - s1 * c2 * s3;
      z = c1 * c2 * s3 - s1 * s2 * c3;
      w = c1 * c2 * c3 + s1 * s2 * s3;
      break;

    case GRAPHENE_EULER_ORDER_ZXY:
      x = s1 * c2 * c3 - c1 * s2 * s3;
      y = c1 * s2 * c3 + s1 * c2 * s3;
      z = c1 * c2 * s3 + s1 * s2 * c3;
      w = c1 * c2 * c3 - s1 * s2 * s3;
      break;

    case GRAPHENE_EULER_ORDER_ZYX:
      x = s1 * c2 * c3 - c1 * s2 * s3;
      y = c1 * s2 * c3 + s1 * c2 * s3;
      z = c1 * c2 * s3 - s1 * s2 * c3;
      w = c1 * c2 * c3 + s1 * s2 * s3;
      break;

    case GRAPHENE_EULER_ORDER_YZX:
      x = s1 * c2 * c3 + c1 * s2 * s3;
      y = c1 * s2 * c3 + s1 * c2 * s3;
      z = c1 * c2 * s3 - s1 * s2 * c3;
      w = c1 * c2 * c3 - s1 * s2 * s3;
      break;

    case GRAPHENE_EULER_ORDER_XZY:
      x = s1 * c2 * c3 - c1 * s2 * s3;
      y = c1 * s2 * c3 - s1 * c2 * s3;
      z = c1 * c2 * s3 + s1 * s2 * c3;
      w = c1 * c2 * c3 + s1 * s2 * s3;
      break;

    case GRAPHENE_EULER_ORDER_DEFAULT:
      break;
    }

  q->x = x;
  q->y = y;
  q->z = z;
  q->w = w;

  return q;
}

/**
 * graphene_quaternion_equal:
 * @a: a #graphene_quaternion_t
 * @b: a #graphene_quaternion_t
 *
 * Checks whether the given quaternions are equal.
 *
 * Returns: `true` if the quaternions are equal
 *
 * Since: 1.0
 */
bool
graphene_quaternion_equal (const graphene_quaternion_t *a,
                           const graphene_quaternion_t *b)
{
  if (a == b)
    return true;

  if (a == NULL || b == NULL)
    return false;

  return fabsf (a->x - b->x) < 0.00001 &&
         fabsf (a->y - b->y) < 0.00001 &&
         fabsf (a->z - b->z) < 0.00001 &&
         fabsf (a->w - b->w) < 0.00001;
}

/**
 * graphene_quaternion_dot:
 * @a: a #graphene_quaternion_t
 * @b: a #graphene_quaternion_t
 *
 * Computes the dot product of two #graphene_quaternion_t.
 *
 * Returns: the value of the dot products
 *
 * Since: 1.0
 */
float
graphene_quaternion_dot (const graphene_quaternion_t *a,
                         const graphene_quaternion_t *b)
{
  graphene_simd4f_t v_a, v_b;

  v_a = graphene_simd4f_init (a->x, a->y, a->z, a->w);
  v_b = graphene_simd4f_init (b->x, b->y, b->z, b->w);

  return graphene_simd4f_get_x (graphene_simd4f_dot4 (v_a, v_b));
}

/**
 * graphene_quaternion_invert:
 * @q: a #graphene_quaternion_t
 * @res: (out caller-allocates): return location for the inverted
 *   quaternion
 *
 * Inverts a #graphene_quaternion_t.
 *
 * Since: 1.0
 */
void
graphene_quaternion_invert (const graphene_quaternion_t *q,
                            graphene_quaternion_t       *res)
{
  res->x = -q->x;
  res->y = -q->y;
  res->z = -q->z;
}

/**
 * graphene_quaternion_normalize:
 * @q: a #graphene_quaternion_t
 * @res: (out caller-allocates): return location for the normalized
 *   quaternion
 *
 * Normalizes a #graphene_quaternion_t.
 *
 * Since: 1.0
 */
void
graphene_quaternion_normalize (const graphene_quaternion_t *q,
                               graphene_quaternion_t       *res)
{
  graphene_simd4f_t v_q;

  v_q = graphene_simd4f_init (q->x, q->y, q->z, q->w);
  v_q = graphene_simd4f_normalize4 (v_q);

  graphene_quaternion_init_from_simd4f (res, v_q);
}
