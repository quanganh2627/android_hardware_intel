
#include <math.h>
#include <unipoint.h>

static double
scale_tanh(const double in, const double a)
{
  double i;
  double j;

  if (a == 0.0)
    i = 1.0;
  else
    i = (fabs(in) / a) - 1.0;

  j = tanh(i) + 1.0;

  return j;
}

static point
warp_velocity(const point v_in, const point precision, const point v_max)
{
  point retval;

  if (fabs(v_in.X) <= v_max.X) {
    retval.X = fabs(scale_tanh(v_in.X, precision.X));
  } else {
    retval.X = 1.0;
  }

  if (fabs(v_in.Y) <= v_max.Y) {
    retval.Y = fabs(scale_tanh(v_in.Y, precision.Y));
  } else {
    retval.Y = 1.0;
  }

  return retval;
}

point
ballistic_transform(const point v_max, const point gain, const point precision,
		    const point v_in)
{
  point v_inf;
  point v_out;
  point tmp;

  v_inf.X = precision.X * v_max.X;
  v_inf.Y = precision.Y * v_max.Y;

  v_out = warp_velocity(v_in, v_inf, v_max);
  tmp = warp_velocity(v_max, v_inf, v_max);

  v_out.X /= tmp.X;
  v_out.Y /= tmp.Y;
  v_out.X *= v_in.X;
  v_out.Y *= v_in.Y;
  v_out.X *= gain.X;
  v_out.Y *= gain.Y;
  
  return v_out;
}

/*! \brief Gets the scale factor mapping the given interval to [-1, 1] in
 *   both axes.
 */
point 
get_slope(const point lower, const point upper)
{
	point p2;
	p2.X = 2.0;
	p2.Y = 2.0;
	point slope;

	slope.X = p2.X / (upper.X - lower.X);
	slope.Y = p2.Y / (upper.Y - lower.Y);

	return slope;
}

/*! \brief Normalizes relative motion data to the given interval.
 */
point 
normalize_relative
(const point input, const point lower, const point upper)
{
	point out;
	point slope;
	slope = get_slope(lower, upper);
	out.X = input.X * slope.X;
	out.Y = input.Y * slope.Y;
	return out;
}

/*! \brief Normalizes an absolute position to the range [-1, 1] in the
 *  given interval.
 */  
point
normalize_absolute
(const point input, const point lower, const point upper)
{
	point p1;
	p1.X = 1.0;
	p1.Y = 1.0;
	point slope, intercept, out;

	slope = get_slope(lower, upper);
	intercept.X = p1.X - (slope.X * upper.X);
	intercept.Y = p1.Y - (slope.Y * upper.Y);
	out.X = (input.X * slope.X) + intercept.X;
	out.Y = (input.Y * slope.Y) + intercept.Y;

	return out;
}



