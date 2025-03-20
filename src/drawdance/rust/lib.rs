// SPDX-License-Identifier: GPL-3.0-or-later
#![allow(non_camel_case_types)]
#![allow(clippy::unseparated_literal_suffix)]
use std::{
    ffi::{c_char, CStr, CString},
    fmt::Display,
    str::FromStr,
};

include!("bindings.rs");

extern "C" {
    pub fn DP_cpu_support_init();
    pub fn DP_cmake_config_version() -> *const c_char;
}

pub mod common;
pub mod engine;
pub mod msg;

pub fn init() {
    unsafe { DP_cpu_support_init() };
    unsafe { DP_image_impex_init() };
}

pub fn dp_error() -> String {
    unsafe { CStr::from_ptr(DP_error()) }
        .to_str()
        .unwrap_or_default()
        .to_owned()
}

pub fn dp_error_anyhow() -> anyhow::Error {
    anyhow::anyhow!(dp_error())
}

pub fn dp_error_set(message: &str) {
    let cstring = CString::new(message).unwrap_or_default();
    unsafe { DP_error_set(b"%s\0".as_ptr().cast(), cstring.as_ptr()) }
}

pub fn dp_cmake_config_version() -> String {
    unsafe { CStr::from_ptr(DP_cmake_config_version()) }
        .to_str()
        .unwrap_or_default()
        .to_owned()
}

#[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
pub enum Interpolation {
    #[default]
    FastBilinear,
    Bilinear,
    Bicubic,
    Experimental,
    Nearest,
    Area,
    Bicublin,
    Gauss,
    Sinc,
    Lanczos,
    Spline,
}

impl Interpolation {
    pub fn to_scale_interpolation(self) -> DP_ImageScaleInterpolation {
        match self {
            Self::FastBilinear => DP_IMAGE_SCALE_INTERPOLATION_FAST_BILINEAR,
            Self::Bilinear => DP_IMAGE_SCALE_INTERPOLATION_BILINEAR,
            Self::Bicubic => DP_IMAGE_SCALE_INTERPOLATION_BICUBIC,
            Self::Experimental => DP_IMAGE_SCALE_INTERPOLATION_EXPERIMENTAL,
            Self::Nearest => DP_IMAGE_SCALE_INTERPOLATION_NEAREST,
            Self::Area => DP_IMAGE_SCALE_INTERPOLATION_AREA,
            Self::Bicublin => DP_IMAGE_SCALE_INTERPOLATION_BICUBLIN,
            Self::Gauss => DP_IMAGE_SCALE_INTERPOLATION_GAUSS,
            Self::Sinc => DP_IMAGE_SCALE_INTERPOLATION_SINC,
            Self::Lanczos => DP_IMAGE_SCALE_INTERPOLATION_LANCZOS,
            Self::Spline => DP_IMAGE_SCALE_INTERPOLATION_SPLINE,
        }
    }
}

impl Display for Interpolation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::FastBilinear => "fastbilinear",
                Self::Bilinear => "bilinear",
                Self::Bicubic => "bicubic",
                Self::Experimental => "experimental",
                Self::Nearest => "nearest",
                Self::Area => "area",
                Self::Bicublin => "bicublin",
                Self::Gauss => "gauss",
                Self::Sinc => "sinc",
                Self::Lanczos => "lanczos",
                Self::Spline => "spline",
            }
        )
    }
}

impl FromStr for Interpolation {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "fastbilinear" => Ok(Self::FastBilinear),
            "bilinear" => Ok(Self::Bilinear),
            "bicubic" => Ok(Self::Bicubic),
            "experimental" => Ok(Self::Experimental),
            "nearest" => Ok(Self::Nearest),
            "area" => Ok(Self::Area),
            "bicublin" => Ok(Self::Bicublin),
            "gauss" => Ok(Self::Gauss),
            "sinc" => Ok(Self::Sinc),
            "lanczos" => Ok(Self::Lanczos),
            "spline" => Ok(Self::Spline),
            _ => Err(format!(
                "invalid interpolation '{s}', should be one of 'fastbilinear', \
                'bilinear', 'bicubic', 'experimental', 'nearest', 'area', \
                'bicublin', 'gauss', 'sinc', 'lanczos' or 'spline'"
            )),
        }
    }
}
