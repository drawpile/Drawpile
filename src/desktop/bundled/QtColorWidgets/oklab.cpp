// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileComment: Based on Inkscape. THIS IS NOT LGPL, DO NOT UPSTREAM!
#include "QtColorWidgets/oklab.hpp"
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>

namespace color_widgets {

namespace {
/** Two-dimensional array to store a constant 3x3 matrix. */
using Matrix = const double[3][3];

/** Apply sRGB gamma compression to a linear RGB color component. */
static double from_linear(double c)
{
	if(c <= 0.0031308) {
		return 12.92 * c;
	} else {
		return 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
	}
}

/** Convert an sRGB color component to linear RGB (de-gamma). */
static double to_linear(double c)
{
	if(c > 0.04045) {
		return std::pow((c + 0.055) / 1.055, 2.4);
	} else {
		return c / 12.92;
	}
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: LGPL-2.1-only OR MPL-1.1
// SPDX-SpippetName: Additional math stuff from Inkscape's lib2geom.
static double cube(double x)
{
	return x * x * x;
}

static void sincos(double angle, double &out_sin, double &out_cos)
{
	out_sin = std::sin(angle);
	out_cos = std::cos(angle);
}

static QVector<double> solve_quadratic(double a, double b, double c)
{
	QVector<double> result;

	if(a == 0) {
		// linear equation
		if(b == 0)
			return result;
		result.reserve(1);
		result.append(-c / b);
		return result;
	}

	double delta = b * b - 4 * a * c;

	if(delta == 0) {
		// one root
		result.reserve(1);
		result.append(-b / (2 * a));
	} else if(delta > 0) {
		// two roots
		double delta_sqrt = std::sqrt(delta);

		// Use different formulas depending on sign of b to preserve
		// numerical stability. See e.g.:
		// http://people.csail.mit.edu/bkph/articles/Quadratics.pdf
		int sign = b >= 0 ? 1 : -1;
		double t = -0.5 * (b + sign * delta_sqrt);
		result.reserve(2);
		result.append(t / a);
		result.append(c / t);
	}
	// no roots otherwise

	std::sort(result.begin(), result.end());
	return result;
}

static QVector<double> solve_cubic(double a, double b, double c, double d)
{
	// based on:
	// http://mathworld.wolfram.com/CubicFormula.html

	if(a == 0) {
		return solve_quadratic(b, c, d);
	}
	if(d == 0) {
		// divide by x
		QVector<double> result = solve_quadratic(a, b, c);
		result.push_back(0);
		std::sort(result.begin(), result.end());
		return result;
	}

	QVector<double> result;

	// 1. divide everything by a to bring to canonical form
	b /= a;
	c /= a;
	d /= a;

	// 2. eliminate x^2 term: x^3 + 3Qx - 2R = 0
	double Q = (3 * c - b * b) / 9;
	double R = (-27 * d + b * (9 * c - 2 * b * b)) / 54;

	// 3. compute polynomial discriminant
	double D = Q * Q * Q + R * R;
	double term1 = b / 3;

	if(D > 0) {
		// only one real root
		double S = cbrt(R + std::sqrt(D));
		double T = cbrt(R - std::sqrt(D));
		result.reserve(1);
		result.append(-b / 3 + S + T);
	} else if(D == 0) {
		// 3 real roots, 2 of which are equal
		double rroot = cbrt(R);
		result.reserve(3);
		result.append(-term1 + 2 * rroot);
		result.append(-term1 - rroot);
		result.append(-term1 - rroot);
	} else {
		// 3 distinct real roots
		Q_ASSERT(Q < 0);
		double theta = std::acos(R / std::sqrt(-Q * Q * Q));
		double rroot = 2 * std::sqrt(-Q);
		result.reserve(3);
		result.append(-term1 + rroot * std::cos(theta / 3));
		result.append(-term1 + rroot * std::cos((theta + 2 * M_PI) / 3));
		result.append(-term1 + rroot * std::cos((theta + 4 * M_PI) / 3));
	}

	std::sort(result.begin(), result.end());
	return result;
}
// SPDX-SnippetEnd

/** Matrix of the linear transformation from linear RGB space to linear
 * cone responses, used in the first step of RGB to OKLab conversion.
 */
static constexpr Matrix LRGB2CONE = {
	{0.4122214708, 0.5363325363, 0.0514459929},
	{0.2119034982, 0.6806995451, 0.1073969566},
	{0.0883024619, 0.2817188376, 0.6299787005}};

/** The inverse of the matrix LRGB2CONE. */
static constexpr Matrix CONE2LRGB = {
	{4.0767416613479942676681908333711298900607278264432,
	 -3.30771159040819331315866078424893188865618253342,
	 0.230969928729427886449650619561935920170561518112},
	{-1.2684380040921760691815055595117506020901414005992,
	 2.60975740066337143024050095284233623056192338553,
	 -0.341319396310219620992658250306535533187548361872},
	{-0.0041960865418371092973767821251846315637521173374,
	 -0.70341861445944960601310996913659932654899822384,
	 1.707614700930944853864541790660472961199090408527}};

/** The matrix M2 used in the second step of RGB to OKLab conversion.
 * Taken from https://bottosson.github.io/posts/oklab/ (retrieved 2022).
 */
static constexpr Matrix M2 = {
	{0.2104542553, 0.793617785, -0.0040720468},
	{1.9779984951, -2.428592205, 0.4505937099},
	{0.0259040371, 0.7827717662, -0.808675766}};

/** The inverse of the matrix M2. The first column looks like it wants to be 1
 * but this form is closer to the actual inverse (due to numerics). */
static constexpr Matrix M2_INVERSE = {
	{0.99999999845051981426207542502031373637162589278552,
	 0.39633779217376785682345989261573192476766903603,
	 0.215803758060758803423141461830037892590617787467},
	{1.00000000888176077671607524567047071276183677410134,
	 -0.10556134232365634941095687705472233997368274024,
	 -0.063854174771705903405254198817795633810975771082},
	{1.00000005467241091770129286515344610721841028698942,
	 -0.08948418209496575968905274586339134130669669716,
	 -1.291485537864091739948928752914772401878545675371}};

/** Compute the dot-product between two 3D-vectors. */
template <typename A1, typename A2>
inline constexpr double dot3(const A1 &a1, const A2 &a2)
{
	return a1[0] * a2[0] + a1[1] * a2[1] + a1[2] * a2[2];
}
}

Triplet oklab_to_oklch(Triplet const &ok_lab_color)
{
	Triplet result;
	result[0] = ok_lab_color[0]; // copy the L component
	// Convert a, b to polar coordinates c, h.
	result[1] = std::hypot(ok_lab_color[1], ok_lab_color[2]);
	if(result[1] > 0.001) {
		double hue_angle = std::atan2(ok_lab_color[2], ok_lab_color[1]);
		result[2] = qRadiansToDegrees(hue_angle);
	} else {
		result[2] = 0;
	}
	return result;
}

Triplet oklch_to_oklab(Triplet const &ok_lch_color)
{
	return oklch_radians_to_oklab(
		{ok_lch_color[0], ok_lch_color[1], qDegreesToRadians(ok_lch_color[2])});
}

Triplet oklch_radians_to_oklab(Triplet const &oklch_rad)
{
	Triplet result;
	result[0] = oklch_rad[0]; // Copy the L component
	// c and h are polar coordinates; convert to Cartesian a, b coords.
	sincos(oklch_rad[2], result[2], result[1]);
	result[1] *= oklch_rad[1];
	result[2] *= oklch_rad[1];
	return result;
}

Triplet oklab_to_linear_rgb(Triplet const &oklab_color)
{
	Triplet cones;
	for(unsigned i = 0; i < 3; i++) {
		cones[i] = cube(dot3(M2_INVERSE[i], oklab_color));
	}
	Triplet result;
	for(unsigned i = 0; i < 3; i++) {
		result[i] = std::clamp(dot3(CONE2LRGB[i], cones), 0.0, 1.0);
	}
	return result;
}

Triplet linear_rgb_to_oklab(Triplet const &linear_rgb_color)
{
	Triplet cones;
	for(unsigned i = 0; i < 3; i++) {
		cones[i] = std::cbrt(dot3(LRGB2CONE[i], linear_rgb_color));
	}
	Triplet result;
	for(unsigned i = 0; i < 3; i++) {
		result[i] = dot3(M2[i], cones);
	}
	return result;
}

Triplet oklab_to_rgb(Triplet const &oklab_color)
{
	Triplet rgb = oklab_to_linear_rgb(oklab_color);
	for(double &component : rgb.channels) {
		component = from_linear(component);
	}
	return rgb;
}

Triplet rgb_to_oklab(Triplet const &rgb_color)
{
	Triplet linear;
	for(int i = 0; i < 3; ++i) {
		linear[i] = to_linear(rgb_color[i]);
	}
	return linear_rgb_to_oklab(linear);
}

Triplet oklab_to_okhsl(Triplet const &ok_lab_color)
{
	Triplet result;
	result[2] = std::clamp(ok_lab_color[0], 0.0, 1.0); // Copy the L component.

	// Compute the chroma.
	double const absolute_chroma = std::hypot(ok_lab_color[1], ok_lab_color[2]);
	if(absolute_chroma < 1e-7) {
		// It would be numerically unstable to calculate the hue for this
		// color, so we set the hue and saturation to zero (grayscale color).
		result[0] = 0.0;
		result[1] = 0.0;
		return result;
	}

	// Compute the hue (in the unit interval).
	double hue_angle = std::atan2(ok_lab_color[2], ok_lab_color[1]);
	result[0] = hue_angle / (2.0 * M_PI);

	// Compute the linear saturation.
	double const hue_degrees = qRadiansToDegrees(hue_angle);
	double const chromax = max_chroma(result[2], hue_degrees);
	result[1] = (chromax == 0.0)
					? 0.0
					: std::clamp(absolute_chroma / chromax, 0.0, 1.0);
	return result;
}

Triplet okhsl_to_oklab(Triplet const &ok_hsl_color)
{
	Triplet result;
	result[0] = std::clamp(ok_hsl_color[2], 0.0, 1.0); // Copy the L component.

	// Get max chroma for this hue and lightness and compute the absolute
	// chroma.
	double const chromax = max_chroma(result[0], ok_hsl_color[0] * 360.0);
	double const absolute_chroma = ok_hsl_color[1] * chromax;

	// Convert hue and chroma to the Cartesian a, b coordinates.
	sincos(ok_hsl_color[0] * 2.0 * M_PI, result[2], result[1]);
	result[1] *= absolute_chroma;
	result[2] *= absolute_chroma;
	return result;
}

namespace {
/** @brief
 * Data needed to compute coefficients in the cubic polynomials which express
 * the lines of constant luminosity and hue (but varying chroma) as curves in
 * the linear RGB space.
 */
struct ChromaLineCoefficients {
	// Variable naming: `c%d` contains coefficients of c^%d in the polynomial,
	// where c is the OKLch chroma. l refers to the luminosity, cos and sin to
	// the cosine and sine of the hue angle. Trailing digits are exponents. For
	// example, c2.lcos2 is the coefficient of (l * cos(hue_angle)^2) in the
	// overall coefficient of c^2.
	struct {
		double l2cos, l2sin;
	} c1;
	struct {
		double lcos2, lcossin, lsin2;
	} c2;
	struct {
		double cos3, cos2sin, cossin2, sin3;
	} c3;
};

static ChromaLineCoefficients constexpr LAB_BOUNDS[] = {
	// Red polynomial
	{.c1 =
		 {.l2cos = 5.83279532899080641005754476131631984,
		  .l2sin = 2.3780791275435732378965655753413412},
	 .c2 =
		 {.lcos2 = 1.81614129917652075864819542521099165275,
		  .lcossin = 2.11851258971260413543962953223104329409,
		  .lsin2 = 1.68484527361538384522450980300698198391},
	 .c3 =
		 {.cos3 = 0.257535869797624151773507242289856932594,
		  .cos2sin = 0.414490345667882332785000888243122224651,
		  .cossin2 = 0.126596511492002610582126014059213892767,
		  .sin3 = -0.455702039844046560333204117380816048203}},
	// Green polynomial
	{.c1 =
		 {.l2cos = -2.243030176177044107983968331289088261,
		  .l2sin = 0.00129441240977850026657772225608},
	 .c2 =
		 {.lcos2 = -0.5187087369791308621879921351291952375,
		  .lcossin = -0.7820717390897833607054953914674219281,
		  .lsin2 = -1.8531911425339782749638630868227383795},
	 .c3 =
		 {.cos3 = -0.0817959138495637068389017598370049459,
		  .cos2sin = -0.1239788660641220973883495153116480854,
		  .cossin2 = 0.0792215342150077349794741576353537047,
		  .sin3 = 0.7218132301017783162780535454552058572}},
	// Blue polynomial
	{.c1 =
		 {.l2cos = -0.2406412780923628220925350522352767957,
		  .l2sin = -6.48404701978782955733370693958213669},
	 .c2 =
		 {.lcos2 = 0.015528352128452044798222201797574285162,
		  .lcossin = 1.153466975472590255156068122829360981648,
		  .lsin2 = 8.535379923500727607267514499627438513637},
	 .c3 = {
		 .cos3 = -0.0006573855374563134769075967180540368,
		 .cos2sin = -0.0519029179849443823389557527273309386,
		 .cossin2 = -0.763927972885238036962716856256210617,
		 .sin3 = -3.67825541507929556013845659620477582}}};

/** Stores powers of luminance, hue cosine and hue sine angles. */
struct ConstraintMonomials {
	double l, l2, l3, c, c2, c3, s, s2, s3;
	ConstraintMonomials(double l, double h)
		: l{l}
	{
		l2 = l * l;
		l3 = l2 * l;
		sincos(qDegreesToRadians(h), s, c);
		c2 = c * c;
		c3 = c2 * c;
		s2 = 1.0 - c2; // Use sin^2 = 1 - cos^2.
		s3 = s2 * s;
	}
};

/** @brief Find the coefficients of the cubic polynomial expressing the linear
 * R, G or B component as a function of OKLch chroma.
 *
 * The returned polynomial gives R(c), G(c) or B(c) for all values of c and
 * fixed values of luminance and hue.
 *
 * @param index The index of the component to evaluate (0 for R, 1 for G, 2 for
 * B).
 * @param m The monomials in L, cos(hue) and sin(hue) needed for the
 * calculation.
 * @return an array whose i-th element is the coefficient of c^i in the
 * polynomial.
 */
static void component_coefficients(
	unsigned index, ConstraintMonomials const &m, double *result)
{
	auto const &coeffs = LAB_BOUNDS[index];
	// Multiply the coefficients by the corresponding monomials.
	result[0] = m.l3; // The coefficient of l^3 is always 1
	result[1] = coeffs.c1.l2cos * m.l2 * m.c + coeffs.c1.l2sin * m.l2 * m.s;
	result[2] = coeffs.c2.lcos2 * m.l * m.c2 +
				coeffs.c2.lcossin * m.l * m.c * m.s +
				coeffs.c2.lsin2 * m.l * m.s2;
	result[3] = coeffs.c3.cos3 * m.c3 + coeffs.c3.cos2sin * m.c2 * m.s +
				coeffs.c3.cossin2 * m.c * m.s2 + coeffs.c3.sin3 * m.s3;
}
}

/* Compute the maximum Lch chroma for the given luminosity and hue.
 *
 * Implementation notes:
 * The space of Lch colors is a complicated solid with curved faces in the
 * (L, c, h)-space. So it is not easy to find the maximum chroma for the given
 * luminosity and hue. (By maximum chroma, we mean the maximum value of c such
 * that the color oklch(L c h) still fits in the sRGB gamut.)
 *
 * We consider an abstract ray (L, c, h) where L and h are fixed and c varies
 * from 0 to infinity. Conceptually, we transform this ray to the linear RGB
 * space, which is the unit cube. The ray thus becomes a 3D cubic curve in the
 * RGB cube and the coordinates R(c), G(c) and B(c) are degree 3 polynomials in
 * the chroma variable c. The coefficients of c^i in those polynomials will
 * depend on L and h.
 *
 * To find the smallest positive value of c for which the curve leaves the unit
 * cube, we must solve the equations R(c) = 0, R(c) = 1 and similarly for G(c)
 * and B(c). The desired value is the smallest positive solution among those 6
 * equations.
 *
 * The case of very small or very large luminosity is handled separately.
 */
double max_chroma(double l, double h)
{
	static double const EPS = 1e-7;
	if(l < EPS || l > 1.0 - EPS) { // Black or white allow no chroma.
		return 0;
	}

	double chroma_bound = std::numeric_limits<double>::infinity();
	auto const process_root = [&](double root) -> bool {
		if(root < EPS) { // Ignore roots less than epsilon
			return false;
		}
		if(chroma_bound > root) {
			chroma_bound = root;
		}
		return true;
	};

	// Check relevant chroma constraints for all three coordinates R, G, B.
	auto const monomials = ConstraintMonomials(l, h);
	for(unsigned i = 0; i < 3; i++) {
		double coeffs[4];
		component_coefficients(i, monomials, coeffs);
		// The cubic polynomial is coeffs[3]*c^3 + coeffs[2]*c^2 + coeffs[1]*c +
		// coeffs[0]

		// First we solve for the R/G/B component equal to zero.
		for(double root :
			solve_cubic(coeffs[3], coeffs[2], coeffs[1], coeffs[0])) {
			if(process_root(root)) {
				break;
			}
		}

		// Now solve for the component equal to 1 by subtracting 1.0 from
		// coeffs[0].
		for(double root :
			solve_cubic(coeffs[3], coeffs[2], coeffs[1], coeffs[0] - 1.0)) {
			if(process_root(root)) {
				break;
			}
		}
	}
	if(chroma_bound == std::numeric_limits<double>::infinity()) {
		return 0; // No bound was found, so everything was < EPS
	}
	return chroma_bound;
}

}
