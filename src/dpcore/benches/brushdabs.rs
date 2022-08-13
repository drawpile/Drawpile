// This file is part of Drawpile.
// Copyright (C) 2020 Calle Laakkonen
//
// Drawpile is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// As additional permission under section 7, you are allowed to distribute
// the software through an app store, even if that store has restrictive
// terms and conditions that are incompatible with the GPL, provided that
// the source is also available under the GPL with or without this permission
// through a channel without those restrictive terms and conditions.
//
// Drawpile is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Drawpile.  If not, see <https://www.gnu.org/licenses/>.

use criterion::{criterion_group, criterion_main, Criterion};
use dpcore::paint::{BrushMask, ClassicBrushCache};

fn gimp_style_dab_benchmark(c: &mut Criterion) {
    c.bench_function("tiny v2 GIMP style dab", |b| {
        let mut cache = ClassicBrushCache::new(); // note: this produces a single outlier when the LUT is generated
        b.iter(|| {
            BrushMask::new_gimp_style_v2(0.0, 0.0, 1.0, 0.5, &mut cache);
        })
    });

    c.bench_function("small v2 GIMP style dab", |b| {
        let mut cache = ClassicBrushCache::new(); // note: this produces a single outlier when the LUT is generated
        b.iter(|| {
            BrushMask::new_gimp_style_v2(0.0, 0.0, 15.0, 0.5, &mut cache);
        })
    });

    c.bench_function("big v2 GIMP style dab", |b| {
        let mut cache = ClassicBrushCache::new(); // note: this produces a single outlier when the LUT is generated
        b.iter(|| {
            BrushMask::new_gimp_style_v2(0.0, 0.0, 30.0, 0.5, &mut cache);
        })
    });
}

fn round_pixel_dab_benchmark(c: &mut Criterion) {
    c.bench_function("round pixel dab", |b| {
        b.iter(|| {
            BrushMask::new_round_pixel(15);
        })
    });
}

criterion_group!(benches, gimp_style_dab_benchmark, round_pixel_dab_benchmark);
criterion_main!(benches);
