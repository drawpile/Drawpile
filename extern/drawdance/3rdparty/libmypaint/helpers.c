/* libmypaint - The MyPaint Brush Library
 * Copyright (C) 2007-2008 Martin Renold <martinxyz@gmx.ch>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HELPERS_C
#define HELPERS_C

#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <math.h>
#include "fastapprox/fastpow.h"

#include "helpers.h"

/*const float T_MATRIX[3][36] = {{0.000578913,0.001952085,0.009886235,0.032720398,0.100474668,0.183366464,0.233267126,0.172815304,0.021160832,-0.170961409,-0.358555623,-0.487793958,-0.674399544,-0.886748322,-0.97045709,-0.872696304,-0.559560624,-0.134497482,0.395369748,0.969077244,1.563646415,1.918490755,2.226446938,2.219830783,1.916051812,1.395620385,0.990444867,0.604042138,0.353697296,0.192706913,0.098266461,0.042521122,0.021860797,0.011569942,0.004800182,0.002704537},*/
/*{-0.000491981,-0.00166858,-0.008527451,-0.028611512,-0.089589024,-0.169698855,-0.232545306,-0.211643919,-0.117700145,0.039996723,0.233957719,0.411776827,0.669587627,1.014305033,1.33449208,1.570104952,1.575060777,1.504833712,1.290156767,1.008658851,0.712494742,0.377174433,0.138783274,-0.025203917,-0.099437546,-0.104503807,-0.088552175,-0.059244144,-0.036402168,-0.020300987,-0.010518378,-0.004600355,-0.002372843,-0.001255839,-0.000521027,-0.000293559},*/
/*{0.00344444,0.011708103,0.059997877,0.202735833,0.644403983,1.282371701,1.953710066,2.206582549,2.085203252,1.555923679,0.970316553,0.491245277,0.242873415,0.070279007,-0.061461896,-0.131571399,-0.164055717,-0.176617196,-0.165922717,-0.144226781,-0.119603243,-0.085357673,-0.061960232,-0.041673571,-0.026318559,-0.01522674,-0.009013144,-0.004849822,-0.002624901,-0.001371407,-0.00067843,-0.000287423,-0.000146799,-7.76941E-05,-3.2234E-05,-1.81614E-05}};*/

/*const float spectral_r[36] = {0.001476566,0.001476571,0.001476697,0.001477428,0.001480081,0.00148896,0.001510625,0.001553174,0.001622652,0.001723776,0.001858242,0.002028291,0.002237804,0.002500589,0.002843823,0.003312684,0.003983853,0.004975295,0.0064965,0.008912057,0.012881757,0.019616497,0.031083884,0.050157206,0.079379607,0.117964522,0.16013148,0.196518985,0.222295104,0.237687584,0.245789155,0.249669549,0.251605188,0.252511963,0.252870747,0.252999473};*/

/*const float spectral_g[36] = {0.004457553,0.004459131,0.004461979,0.004471645,0.004506172,0.004629205,0.004938512,0.005602633,0.006868923,0.009209474,0.013450892,0.021153289,0.034963807,0.059025032,0.094985053,0.129917789,0.136598211,0.112473456,0.080301564,0.055184078,0.038887514,0.028984983,0.022965813,0.019364324,0.01723558,0.016001357,0.015318335,0.014937495,0.014732014,0.014630031,0.014587647,0.014573872,0.014574704,0.014576967,0.014575977,0.014579698};*/

/*const float spectral_b[36] = {0.089623258,0.08963333,0.089677437,0.089887675,0.090595886,0.092619509,0.096261517,0.099409964,0.0953724,0.077775801,0.053270305,0.03267985,0.019295409,0.011424407,0.006963547,0.004447989,0.003004361,0.002142226,0.001608547,0.001266496,0.001041612,0.00089261,0.000792505,0.000726769,0.000685047,0.000659708,0.000644691,0.000636345,0.000631858,0.000629574,0.000628471,0.000627966,0.000627715,0.000627593,0.000627548,0.000627528};*/

static const float T_MATRIX_SMALL[3][10] = {{0.026595621243689,0.049779426257903,0.022449850859496,-0.218453689278271
,-0.256894883201278,0.445881722194840,0.772365886289756,0.194498761382537
,0.014038157587820,0.007687264480513}
,{-0.032601672674412,-0.061021043498478,-0.052490001018404
,0.206659098273522,0.572496335158169,0.317837248815438,-0.021216624031211
,-0.019387668756117,-0.001521339050858,-0.000835181622534}
,{0.339475473216284,0.635401374177222,0.771520797089589,0.113222640692379
,-0.055251113343776,-0.048222578468680,-0.012966666339586
,-0.001523814504223,-0.000094718948810,-0.000051604594741}};

static const float spectral_r_small[10] = {0.009281362787953,0.009732627042016,0.011254252737167,0.015105578649573
,0.024797924177217,0.083622585502406,0.977865045723212,1.000000000000000
,0.999961046144372,0.999999992756822};

static const float spectral_g_small[10] = {0.002854127435775,0.003917589679914,0.012132151699187,0.748259205918013
,1.000000000000000,0.865695937531795,0.037477469241101,0.022816789725717
,0.021747419446456,0.021384940572308};

static const float spectral_b_small[10] = {0.537052150373386,0.546646402401469,0.575501819073983,0.258778829633924
,0.041709923751716,0.012662638828324,0.007485593127390,0.006766900622462
,0.006699764779016,0.006676219883241};


float rand_gauss (RngDouble * rng)
{
  double sum = 0.0;
  sum += rng_double_next(rng);
  sum += rng_double_next(rng);
  sum += rng_double_next(rng);
  sum += rng_double_next(rng);
  return sum * 1.73205080757 - 3.46410161514;
}

// C fmodf function is not "arithmetic modulo"; it doesn't handle negative dividends as you might expect
// if you expect 0 or a positive number when dealing with negatives, use
// this function instead.
float mod_arith(float a, float N)
{
    float ret = a - N * floor (a / N);
    return ret;
}

// Returns the smallest angular difference
float smallest_angular_difference(float angleA, float angleB)
{
    float a;
    a = angleB - angleA;
    a = mod_arith((a + 180), 360) - 180;
    a += (a>180) ? -360 : (a<-180) ? 360 : 0;
    return a;
}

// stolen from GIMP (gimpcolorspace.c)
// (from gimp_rgb_to_hsv)
void
rgb_to_hsv_float (float *r_ /*h*/, float *g_ /*s*/, float *b_ /*v*/)
{
  float max, min, delta;
  float h, s, v;
  float r, g, b;

  h = 0.0; // silence gcc warning

  r = *r_;
  g = *g_;
  b = *b_;

  r = CLAMP(r, 0.0, 1.0);
  g = CLAMP(g, 0.0, 1.0);
  b = CLAMP(b, 0.0, 1.0);

  max = MAX3(r, g, b);
  min = MIN3(r, g, b);

  v = max;
  delta = max - min;

  if (delta > 0.0001)
    {
      s = delta / max;

      if (r == max)
        {
          h = (g - b) / delta;
          if (h < 0.0)
            h += 6.0;
        }
      else if (g == max)
        {
          h = 2.0 + (b - r) / delta;
        }
      else if (b == max)
        {
          h = 4.0 + (r - g) / delta;
        }

      h /= 6.0;
    }
  else
    {
      s = 0.0;
      h = 0.0;
    }

  *r_ = h;
  *g_ = s;
  *b_ = v;
}

// (from gimp_hsv_to_rgb)
void
hsv_to_rgb_float (float *h_, float *s_, float *v_)
{
  int    i;
  double f, w, q, t;
  float h, s, v;
  float r, g, b;
  r = g = b = 0.0; // silence gcc warning

  h = *h_;
  s = *s_;
  v = *v_;

  h = h - floor(h);
  s = CLAMP(s, 0.0, 1.0);
  v = CLAMP(v, 0.0, 1.0);

  double hue;

  if (s == 0.0)
    {
      r = v;
      g = v;
      b = v;
    }
  else
    {
      hue = h;

      if (hue == 1.0)
        hue = 0.0;

      hue *= 6.0;

      i = (int) hue;
      f = hue - i;
      w = v * (1.0 - s);
      q = v * (1.0 - (s * f));
      t = v * (1.0 - (s * (1.0 - f)));

      switch (i)
        {
        case 0:
          r = v;
          g = t;
          b = w;
          break;
        case 1:
          r = q;
          g = v;
          b = w;
          break;
        case 2:
          r = w;
          g = v;
          b = t;
          break;
        case 3:
          r = w;
          g = q;
          b = v;
          break;
        case 4:
          r = t;
          g = w;
          b = v;
          break;
        case 5:
          r = v;
          g = w;
          b = q;
          break;
        }
    }

  *h_ = r;
  *s_ = g;
  *v_ = b;
}

// (from gimp_rgb_to_hsl)
void
rgb_to_hsl_float (float *r_, float *g_, float *b_)
{
  double max, min, delta;

  float h, s, l;
  float r, g, b;

  // silence gcc warnings
  h=0;

  r = *r_;
  g = *g_;
  b = *b_;

  r = CLAMP(r, 0.0, 1.0);
  g = CLAMP(g, 0.0, 1.0);
  b = CLAMP(b, 0.0, 1.0);

  max = MAX3(r, g, b);
  min = MIN3(r, g, b);

  l = (max + min) / 2.0;

  if (max == min)
    {
      s = 0.0;
      h = 0.0; //GIMP_HSL_UNDEFINED;
    }
  else
    {
      if (l <= 0.5)
        s = (max - min) / (max + min);
      else
        s = (max - min) / (2.0 - max - min);

      delta = max - min;

      if (delta == 0.0)
        delta = 1.0;

      if (r == max)
        {
          h = (g - b) / delta;
        }
      else if (g == max)
        {
          h = 2.0 + (b - r) / delta;
        }
      else if (b == max)
        {
          h = 4.0 + (r - g) / delta;
        }

      h /= 6.0;

      if (h < 0.0)
        h += 1.0;
    }

  *r_ = h;
  *g_ = s;
  *b_ = l;
}

static double
hsl_value (double n1,
           double n2,
           double hue)
{
  double val;

  if (hue > 6.0)
    hue -= 6.0;
  else if (hue < 0.0)
    hue += 6.0;

  if (hue < 1.0)
    val = n1 + (n2 - n1) * hue;
  else if (hue < 3.0)
    val = n2;
  else if (hue < 4.0)
    val = n1 + (n2 - n1) * (4.0 - hue);
  else
    val = n1;

  return val;
}


/**
 * gimp_hsl_to_rgb:
 * @hsl: A color value in the HSL colorspace
 * @rgb: The value converted to a value in the RGB colorspace
 *
 * Convert a HSL color value to an RGB color value.
 **/
void
hsl_to_rgb_float (float *h_, float *s_, float *l_)
{
  float h, s, l;
  float r, g, b;

  h = *h_;
  s = *s_;
  l = *l_;

  h = h - floor(h);
  s = CLAMP(s, 0.0, 1.0);
  l = CLAMP(l, 0.0, 1.0);

  if (s == 0)
    {
      /*  achromatic case  */
      r = l;
      g = l;
      b = l;
    }
  else
    {
      double m1, m2;

      if (l <= 0.5)
        m2 = l * (1.0 + s);
      else
        m2 = l + s - l * s;

      m1 = 2.0 * l - m2;

      r = hsl_value (m1, m2, h * 6.0 + 2.0);
      g = hsl_value (m1, m2, h * 6.0);
      b = hsl_value (m1, m2, h * 6.0 - 2.0);
    }

  *h_ = r;
  *s_ = g;
  *l_ = b;
}

void
rgb_to_hcy_float (float *r_, float *g_, float *b_) {
	
	float _HCY_RED_LUMA = 0.2162;
	float _HCY_GREEN_LUMA = 0.7152;
	float _HCY_BLUE_LUMA = 0.0722;
	float h, c, y;
	float r, g, b;
	float p, n, d;

	r = *r_;
	g = *g_;
	b = *b_;

	// Luma is just a weighted sum of the three components.
	y = _HCY_RED_LUMA*r + _HCY_GREEN_LUMA*g + _HCY_BLUE_LUMA*b;

	// Hue. First pick a sector based on the greatest RGB component, then add
	// the scaled difference of the other two RGB components.
	p = MAX3(r, g, b);
	n = MIN3(r, g, b);
	d = p - n; // An absolute measure of chroma: only used for scaling

	if (n == p){
		h = 0.0;
	} else if (p == r){
		h = (g - b)/d;
		if (h < 0){
			h += 6.0;
		}
	} else if (p == g){
		h = ((b - r)/d) + 2.0;
	} else {  // p==b
		h = ((r - g)/d) + 4.0;
	}
	h /= 6.0;
	h = fmod(h,1.0);

	// Chroma, relative to the RGB gamut envelope.
	if ((r == g) && (g == b)){
		// Avoid a division by zero for the achromatic case.
		c = 0.0;
	} else {
		// For the derivation, see the GLHS paper.
		c = MAX((y-n)/y, (p-y)/(1-y));
	}

	*r_ = h;
	*g_ = c;
	*b_ = y;
}

void
hcy_to_rgb_float (float *h_, float *c_, float *y_) {
	
	float _HCY_RED_LUMA = 0.2162;
	float _HCY_GREEN_LUMA = 0.7152;
	float _HCY_BLUE_LUMA = 0.0722;
	float h, c, y;
	float r, g, b;
	float th, tm;

	h = *h_;
	c = *c_;
	y = *y_;

	h = h - floor(h);
	c = CLAMP(c, 0.0f, 1.0f);
	y = CLAMP(y, 0.0f, 1.0f);

	if (c == 0)	{
	  /*  achromatic case  */
	  r = y;
	  g = y;
	  b = y;
	}

	h = fmod(h, 1.0);
	h *= 6.0;

	if (h < 1){
		// implies (p==r and h==(g-b)/d and g>=b)
		th = h;
		tm = _HCY_RED_LUMA + _HCY_GREEN_LUMA * th;
	} else if (h < 2) {
		// implies (p==g and h==((b-r)/d)+2.0 and b<r)
		th = 2.0 - h;
		tm = _HCY_GREEN_LUMA + _HCY_RED_LUMA * th;
	} else if (h < 3){
		// implies (p==g and h==((b-r)/d)+2.0 and b>=g)
		th = h - 2.0;
		tm = _HCY_GREEN_LUMA + _HCY_BLUE_LUMA * th;
	} else if (h < 4) {
		// implies (p==b and h==((r-g)/d)+4.0 and r<g)
		th = 4.0 - h;
		tm = _HCY_BLUE_LUMA + _HCY_GREEN_LUMA * th;
	} else if (h < 5){
		// implies (p==b and h==((r-g)/d)+4.0 and r>=g)
		th = h - 4.0;
		tm = _HCY_BLUE_LUMA + _HCY_RED_LUMA * th;
	} else {
		// implies (p==r and h==(g-b)/d and g<b)
		th = 6.0 - h;
		tm = _HCY_RED_LUMA + _HCY_BLUE_LUMA * th;
	}

	float n,p,o;
	// Calculate the RGB components in sorted order
	if (tm >= y){
		p = y + y*c*(1-tm)/tm;
		o = y + y*c*(th-tm)/tm;
		n = y - (y*c);
	}else{
		p = y + (1-y)*c;
		o = y + (1-y)*c*(th-tm)/(1-tm);
		n = y - (1-y)*c*tm/(1-tm);
	}

	// Back to RGB order
	if (h < 1){
		r = p;
		g = o;
		b = n;
	} else if (h < 2){
		r = o;
		g = p;
		b = n;
	} else if (h < 3){
		r = n;
		g = p;
		b = o;
	} else if (h < 4){
		r = n;
		g = o;
		b = p;
	} else if (h < 5){
		r = o;
		g = n;
		b = p;
	}else{ 
		r = p;
		g = n;
		b = o;
	}

	*h_ = r;
	*c_ = g;
	*y_ = b;
}


void
rgb_to_spectral (float r, float g, float b, float *spectral_) {
  float offset = 1.0 - WGM_EPSILON;
  r = r * offset + WGM_EPSILON;
  g = g * offset + WGM_EPSILON;
  b = b * offset + WGM_EPSILON;
  //upsample rgb to spectral primaries
  float spec_r[10] = {0};
  for (int i=0; i < 10; i++) {
    spec_r[i] = spectral_r_small[i] * r;
  }
  float spec_g[10] = {0};
  for (int i=0; i < 10; i++) {
    spec_g[i] = spectral_g_small[i] * g;
  }
  float spec_b[10] = {0};
  for (int i=0; i < 10; i++) {
    spec_b[i] = spectral_b_small[i] * b;
  }
  //collapse into one spd
  for (int i=0; i<10; i++) {
    spectral_[i] += spec_r[i] + spec_g[i] + spec_b[i];
  }

}

void
spectral_to_rgb (float *spectral, float *rgb_) {
  float offset = 1.0 - WGM_EPSILON;
  // We need this tmp. array to allow auto vectorization.
  float tmp[3] = {0};
  for (int i=0; i<10; i++) {
    tmp[0] += T_MATRIX_SMALL[0][i] * spectral[i];
    tmp[1] += T_MATRIX_SMALL[1][i] * spectral[i];
    tmp[2] += T_MATRIX_SMALL[2][i] * spectral[i];
  }
  for (int i=0; i<3; i++) {
    rgb_[i] = CLAMP((tmp[i] - WGM_EPSILON) / offset, 0.0f, 1.0f);
  }
}


//function to make it easy to blend two spectral colors via weighted geometric mean
//a is the current smudge state, b is the get_color or brush color
float * mix_colors(float *a, float *b, float fac, float paint_mode)
{
  static float result[4] = {0};  
  
  float opa_a = fac;
  float opa_b = 1.0-opa_a;
  result[3] = CLAMP(opa_a * a[3] + opa_b * b[3], 0.0f, 1.0f);
  // Guard against NaN from division by zero
  float sfac_a = a[3] == 0 ? 0.0 : opa_a * a[3] / (a[3] + b[3] * opa_b);
  float sfac_b = 1 - sfac_a;

  if (paint_mode > 0.0) { 
    float spec_a[10] = {0};
    float spec_b[10] = {0};

    rgb_to_spectral(a[0], a[1], a[2], spec_a);
    rgb_to_spectral(b[0], b[1], b[2], spec_b);
    
    //blend spectral primaries subtractive WGM
    float spectralmix[10] = {0};
    for (int i=0; i < 10; i++) {
      // 'mix_colors' is called infrequently enough that we
      // can afford to not use the faster approximations here.
      spectralmix[i] = powf(spec_a[i], sfac_a) * powf(spec_b[i], sfac_b);
    }
    
    //convert to RGB
    float rgb_result[3] = {0};
    spectral_to_rgb(spectralmix, rgb_result);

    for (int i=0; i < 3; i++) {
      result[i] = rgb_result[i];
    }
  }
  
  if (paint_mode < 1.0) {
    for (int i=0; i<3; i++) {
      result[i] = result[i] * paint_mode + (1-paint_mode) * (a[i] * opa_a + b[i] * opa_b);
    }
  }

  return result;
}

#endif //HELPERS_C
