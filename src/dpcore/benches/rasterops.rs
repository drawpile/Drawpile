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
use dpcore::paint::{rasterop, Blendmode, BrushMask, Pixel15};

fn mask_blend(mask: &[u16], mode: Blendmode) {
    let mut base = [[128, 128, 128, 128]; 64 * 64];
    rasterop::mask_blend(&mut base, [255, 255, 255, 255], mask, mode, 127);
}

fn pixel_blend(over: &[Pixel15], mode: Blendmode) {
    let mut base = [[128, 128, 128, 128]; 64 * 64];
    rasterop::pixel_blend(&mut base, over, 128, mode);
}

fn mask_blend_benchmark(c: &mut Criterion) {
    let mask = BrushMask::new_round_pixel(64);

    c.bench_function("mask normal", |b| {
        b.iter(|| mask_blend(&mask.mask, Blendmode::Normal))
    });
    c.bench_function("mask erase", |b| {
        b.iter(|| mask_blend(&mask.mask, Blendmode::Erase))
    });
    c.bench_function("mask multiply", |b| {
        b.iter(|| mask_blend(&mask.mask, Blendmode::Multiply))
    });
    c.bench_function("mask behind", |b| {
        b.iter(|| mask_blend(&mask.mask, Blendmode::Behind))
    });
    c.bench_function("mask colorerase", |b| {
        b.iter(|| mask_blend(&mask.mask, Blendmode::ColorErase))
    });
}

fn pixel_blend_benchmark(c: &mut Criterion) {
    let over = vec![[255, 255, 255, 255]; 64 * 64];

    c.bench_function("pixel normal", |b| {
        b.iter(|| pixel_blend(&over, Blendmode::Normal))
    });
    c.bench_function("pixel erase", |b| {
        b.iter(|| pixel_blend(&over, Blendmode::Erase))
    });
    c.bench_function("pixel multiply", |b| {
        b.iter(|| pixel_blend(&over, Blendmode::Multiply))
    });
    c.bench_function("pixel behind", |b| {
        b.iter(|| pixel_blend(&over, Blendmode::Behind))
    });
    c.bench_function("pixel colorerase", |b| {
        b.iter(|| pixel_blend(&over, Blendmode::ColorErase))
    });
}

criterion_group!(benches, mask_blend_benchmark, pixel_blend_benchmark);
criterion_main!(benches);
