// SPDX-License-Identifier: GPL-3.0-or-later
use anyhow::{anyhow, Result};
use drawdance::{
    dp_cmake_config_version,
    engine::{Image, PaintEngine, Player},
    DP_UPixel8, DP_PLAYER_TYPE_GUESS, DP_PROTOCOL_VERSION,
};
use regex::Regex;
use std::{
    collections::VecDeque,
    env::consts::EXE_SUFFIX,
    ffi::{c_char, c_int, CStr, OsStr},
    fmt::Display,
    fs::File,
    io::{self, stdout},
    process::{Command, Stdio},
    str::FromStr,
};

#[derive(Copy, Clone, Debug)]
pub struct Dimensions {
    width: usize,
    height: usize,
}

impl Dimensions {
    fn to_arg(self) -> String {
        format!("{}x{}", self.width, self.height)
    }
}

impl Display for Dimensions {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}x{}", self.width, self.height)
    }
}

impl FromStr for Dimensions {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if let Some((a, b)) = s.to_lowercase().split_once('x') {
            let width: usize = a.parse().unwrap_or(0);
            let height: usize = b.parse().unwrap_or(0);
            return Ok(Dimensions { width, height });
        }
        Err(format!(
            "Invalid dimensions '{s}', must be given as WIDTHxHEIGHT"
        ))
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
pub enum OutputFormat {
    #[default]
    Mp4,
    Webm,
    Custom,
    Raw,
}

impl OutputFormat {
    fn to_args(self) -> Vec<&'static str> {
        match self {
            Self::Mp4 => vec!["-c:v", "libx264", "-pix_fmt", "yuv420p", "-an"],
            Self::Webm => vec![
                "-c:v",
                "libvpx",
                "-pixm_fmt",
                "yuv420p",
                "-crf",
                "10",
                "-b:v",
                "0",
                "-an",
            ],
            _ => Vec::new(),
        }
    }
}

impl FromStr for OutputFormat {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "mp4" => Ok(Self::Mp4),
            "webm" => Ok(Self::Webm),
            "custom" => Ok(Self::Custom),
            "raw" => Ok(Self::Raw),
            _ => Err(format!(
                "invalid output format '{s}', should be one of 'mp4', 'webm', 'custom' or 'raw'"
            )),
        }
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
pub enum LogoLocation {
    #[default]
    BottomLeft,
    TopLeft,
    TopRight,
    BottomRight,
    None,
}

impl LogoLocation {
    fn get_x(self, distance_x: usize) -> String {
        match self {
            Self::BottomLeft | Self::TopLeft => format!("{}", distance_x),
            Self::BottomRight | Self::TopRight => format!("W-w-{}", distance_x),
            Self::None => panic!("Invalid logo location"),
        }
    }

    fn get_y(self, distance_y: usize) -> String {
        match self {
            Self::TopLeft | Self::TopRight => format!("{}", distance_y),
            Self::BottomLeft | Self::BottomRight => format!("H-h-{}", distance_y),
            Self::None => panic!("Invalid logo location"),
        }
    }
}

impl Display for LogoLocation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::BottomLeft => "bottom-left",
                Self::TopLeft => "top-left",
                Self::TopRight => "top-right",
                Self::BottomRight => "bottom-right",
                Self::None => "none",
            }
        )
    }
}

impl FromStr for LogoLocation {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "bottom-left" => Ok(Self::BottomLeft),
            "top-left" => Ok(Self::TopLeft),
            "top-right" => Ok(Self::TopRight),
            "bottom-right" => Ok(Self::BottomRight),
            "none" => Ok(Self::None),
            _ => Err(format!(
                "invalid logo location '{s}', should be one of 'bottom-left', \
                'top-left', 'top-right', 'bottom-right' or 'none'"
            )),
        }
    }
}

#[no_mangle]
pub extern "C" fn drawpile_timelapse_main(default_logo_path: *const c_char) -> c_int {
    drawdance::init();

    let flags = xflags::parse_or_exit! {
        /// Displays version information and exits.
        optional -v,--version
        /// Output format. Use 'mp4' or 'webm' for sensible presets in those two
        /// formats that should work in most places. Use `custom` to manually
        /// specify the output format using -c/--custom-argument. Use `raw` to
        /// not run ffmpeg at all and instead just dump the raw frames. Defaults
        /// to 'mp4'.
        optional -f,--format output_format: OutputFormat
        /// Output file path, '-' for stdout. Required.
        required -o,--out output_path: String
        /// Video dimensions, required. In the form WIDTHxHEIGHT. All frames
        /// will be resized to fit into these dimensions.
        required -d,--dimensions dimensions: Dimensions
        /// Video frame rate. Defaults to 24. Higher values may not work on all
        /// platforms and may not play back properly or be allowed to upload!
        optional -r,--framerate framerate: i32
        /// Interval between each frame, in milliseconds. Defaults to 10000,
        /// higher values mean the timelapse will be faster.
        optional -i,--interval interval: i64
        /// Where to put the logo. One of 'bottom-left' (the default),
        /// 'top-left', 'top-right', 'bottom-right' will put the logo in that
        /// corner. Use 'none' to remove the logo altogether.
        optional -l,--logo-location logo_location: LogoLocation
        /// Path to the logo. Default is to use the Drawpile logo.
        optional -L,--logo-path logo_path: String
        /// Opacity of the logo, in percent. Default is 40.
        optional -O,--logo-opacity logo_opacity: i32
        /// Distance of the logo from the corner, in the format WIDTHxHEIGHT.
        /// Default is 20x20.
        optional -D,--logo-distance logo_distance: Dimensions
        /// Relative scale of the logo, in percent. Default is 100.
        optional -S, --logo-scale logo_scale: i32
        /// The color of the flash when the timelapse is finished, in argb
        /// hexadecimal format. Default is 'ffffffff' for a solid white flash.
        /// Use 'none' for no flash.
        optional -F,--flash flash: String
        /// How many seconds to linger on the final result. Default is 5.0.
        optional -t,--linger-time linger_time: f64
        /// Path to ffmpeg executable. If not given, it just looks in PATH.
        optional -C,--ffmpeg-location ffmpeg_location: String
        /// Custom ffmpeg arguments. Repeat this for every argument you want to
        /// append, like `-c -pix_fmt -c yuv420`.
        repeated -c,--custom-argument custom_arguments: String
        /// Only print out what's about to be done and which arguments would be
        /// passed to ffmpeg, then exit. Useful for inspecting parameters before
        /// waiting through everything being rendered out.
        optional -p,--print-only
        /// Performs ACL filtering. This will filter out any commands that the
        /// user wasn't allowed to actually perform, such as drawing on a layer
        /// that they didn't have permission to draw on. The Drawpile client
        /// would also filter these out when playing back a recording.
        optional -A,--acl
        /// Input recording file(s).
        repeated input: String
    };

    if flags.version {
        println!("drawpile-timelapse {}", dp_cmake_config_version());
        println!(
            "Protocol version: {}",
            CStr::from_bytes_with_nul(DP_PROTOCOL_VERSION)
                .unwrap()
                .to_str()
                .unwrap()
        );
        return 0;
    }

    let framerate = flags.framerate.unwrap_or(24);
    if framerate < 1 {
        eprintln!("Invalid framerate {}", framerate);
        return 2;
    }

    let interval = flags.interval.unwrap_or(10_000);
    if interval < 1 {
        eprintln!("Invalid interval {}", interval);
        return 2;
    }

    let logo_location = flags.logo_location.unwrap_or_default();
    let logo_path = flags.logo_path.unwrap_or_else(|| {
        unsafe { CStr::from_ptr(default_logo_path) }
            .to_str()
            .unwrap_or_default()
            .to_owned()
    });
    let logo_opacity = flags.logo_opacity.unwrap_or(40).clamp(0, 100);
    let logo_scale = flags.logo_scale.unwrap_or(100).max(0);
    let logo_distance = flags.logo_distance.unwrap_or(Dimensions {
        width: 20,
        height: 20,
    });
    let logo_enabled = logo_location != LogoLocation::None
        && !logo_path.is_empty()
        && logo_opacity > 0
        && logo_scale > 0;

    let format = flags.format.unwrap_or_default();
    let dimensions = flags.dimensions;
    if dimensions.width < 1 || dimensions.height < 1 {
        eprintln!("Invalid dimensions {}", dimensions);
        return 2;
    }

    let opt_command = if format == OutputFormat::Raw {
        None
    } else {
        let mut command = Command::new(
            flags
                .ffmpeg_location
                .unwrap_or_else(|| format!("ffmpeg{}", EXE_SUFFIX)),
        );
        command
            .stdin(Stdio::piped())
            .stdout(Stdio::inherit())
            .stderr(Stdio::inherit())
            .arg("-f")
            .arg("rawvideo")
            .arg("-pix_fmt")
            .arg("rgb32")
            .arg("-s:v")
            .arg(dimensions.to_arg())
            .arg("-r")
            .arg(framerate.to_string())
            .arg("-i")
            .arg("pipe:0");

        if logo_enabled {
            let scale_filter = if logo_scale == 100 {
                String::new()
            } else {
                let s = f64::from(logo_scale) / 100.0_f64;
                format!(",scale=iw*{}:ih*{}", s, s)
            };
            command
                .arg("-i")
                .arg(&logo_path)
                .arg("-filter_complex")
                .arg(format!(
                    "[1]lut=a=val*{}{}[a];[0][a]overlay={}:{}",
                    f64::from(logo_opacity) / 100.0_f64,
                    scale_filter,
                    logo_location.get_x(logo_distance.width),
                    logo_location.get_y(logo_distance.height),
                ));
        }

        command
            .args(format.to_args())
            .arg("-y")
            .args(flags.custom_argument)
            .arg(flags.out.clone());
        Some(command)
    };

    let flash_str = flags.flash.unwrap_or_else(|| "ffffff".to_owned());
    let rgb_re = Regex::new(r"(?i)\A[0-9a-f]{6}\z").unwrap();
    let argb_re = Regex::new(r"(?i)\A[0-9a-f]{8}\z").unwrap();
    let flash = if flash_str == "none" {
        None
    } else if rgb_re.is_match(&flash_str) {
        Some(0xff000000_u32 | u32::from_str_radix(&flash_str, 16).unwrap())
    } else if argb_re.is_match(&flash_str) {
        Some(u32::from_str_radix(&flash_str, 16).unwrap())
    } else {
        eprintln!("Invalid flash '{}'", flash_str);
        return 2;
    };

    let linger_time = flags.linger_time.unwrap_or(5.0_f64);
    if linger_time < 0.0 {
        eprintln!("Invalid linger time {}", linger_time);
        return 2;
    }

    let acl_override = !flags.acl;

    if flags.print_only {
        eprintln!();
        eprintln!("dimensions:\n    {}", flags.dimensions.to_arg());
        eprintln!("interval:\n    {} msec", interval);
        if logo_enabled {
            eprintln!("logo location:\n    {}", logo_location);
            eprintln!("logo path:\n    {}", logo_path);
            eprintln!("logo opacity:\n    {}%", logo_opacity);
            eprintln!("logo scale:\n    {}%", logo_scale);
            eprintln!("logo distance:\n    {}", logo_distance);
        }
        if let Some(flash_color) = flash {
            eprintln!("flash:\n    {:x}", flash_color);
        } else {
            eprintln!("flash:\n    none");
        }
        eprintln!("linger time:\n    {} sec", linger_time);
        eprintln!("filter acls:\n    {}", !acl_override);
        if let Some(command) = opt_command {
            eprintln!(
                "ffmpeg command line:\n    {} {}",
                command.get_program().to_string_lossy(),
                command
                    .get_args()
                    .map(OsStr::to_string_lossy)
                    .collect::<Vec<_>>()
                    .join(" ")
            );
        } else {
            eprintln!("raw output to:\n    {}", flags.out);
        }
        eprintln!();
        return 0;
    }

    let input_paths = flags.input;
    if input_paths.is_empty() {
        eprintln!("No input file given");
        return 2;
    }

    let result = if let Some(command) = opt_command {
        make_timelapse_command(
            command,
            &input_paths,
            framerate,
            acl_override,
            interval,
            dimensions.width,
            dimensions.height,
            flash,
            linger_time,
        )
    } else if flags.out == "-" {
        timelapse(
            &mut stdout(),
            &input_paths,
            framerate,
            acl_override,
            interval,
            dimensions.width,
            dimensions.height,
            flash,
            linger_time,
        )
    } else {
        make_timelapse_raw(
            &flags.out,
            &input_paths,
            framerate,
            acl_override,
            interval,
            dimensions.width,
            dimensions.height,
            flash,
            linger_time,
        )
    };

    match result {
        Ok(_) => 0,
        Err(e) => {
            eprintln!("{}", e);
            1
        }
    }
}

fn make_timelapse_command(
    mut command: Command,
    input_paths: &Vec<String>,
    framerate: i32,
    acl_override: bool,
    interval: i64,
    width: usize,
    height: usize,
    flash: Option<u32>,
    linger_time: f64,
) -> Result<()> {
    let mut child = match command.spawn() {
        Ok(c) => c,
        Err(e) => {
            return Err(anyhow!(
                "Could not start '{}': {}",
                command.get_program().to_string_lossy(),
                e
            ));
        }
    };
    let mut pipe = child
        .stdin
        .take()
        .ok_or_else(|| anyhow!("Can't open stdin"))?;
    timelapse(
        &mut pipe,
        input_paths,
        framerate,
        acl_override,
        interval,
        width,
        height,
        flash,
        linger_time,
    )?;
    drop(pipe);
    let status = child.wait()?;
    if status.success() {
        Ok(())
    } else {
        Err(anyhow!(status))
    }
}

fn make_timelapse_raw(
    path: &String,
    input_paths: &Vec<String>,
    framerate: i32,
    acl_override: bool,
    interval: i64,
    width: usize,
    height: usize,
    flash: Option<u32>,
    linger_time: f64,
) -> Result<()> {
    let mut f = File::create(path)?;
    timelapse(
        &mut f,
        input_paths,
        framerate,
        acl_override,
        interval,
        width,
        height,
        flash,
        linger_time,
    )?;
    Ok(())
}

struct TimelapseContext<'a> {
    writer: &'a mut dyn io::Write,
    images: VecDeque<Image>,
}

impl<'a> TimelapseContext<'a> {
    fn push(&mut self, img: Image) -> io::Result<()> {
        self.images.push_back(img);
        while self.images.len() > 2 {
            self.shift()?;
        }
        Ok(())
    }

    fn shift(&mut self) -> io::Result<()> {
        self.images.pop_front().unwrap().dump(self.writer)
    }
}

fn timelapse(
    writer: &mut dyn io::Write,
    input_paths: &Vec<String>,
    framerate: i32,
    acl_override: bool,
    interval: i64,
    width: usize,
    height: usize,
    flash: Option<u32>,
    linger_time: f64,
) -> Result<()> {
    let mut ctx = TimelapseContext {
        writer,
        images: VecDeque::new(),
    };

    for input_path in input_paths {
        timelapse_recording(&mut ctx, input_path, acl_override, interval, width, height)?;
    }

    if !ctx.images.is_empty() {
        let fr = f64::from(framerate);
        let img1 = &ctx.images.front().unwrap();
        let img2 = &ctx.images.back().unwrap();

        if let Some(color) = flash {
            render_flash(ctx.writer, img1, img2, fr, color)?;
        } else {
            img1.dump(ctx.writer)?;
        }
        render_linger(ctx.writer, img2, fr, linger_time)?;
    }

    ctx.writer.flush()?;
    Ok(())
}

fn timelapse_recording(
    ctx: &mut TimelapseContext,
    input_path: &String,
    acl_override: bool,
    interval: i64,
    width: usize,
    height: usize,
) -> Result<()> {
    let mut player = make_player(input_path).and_then(Player::check_compatible)?;
    player.set_acl_override(acl_override);

    let mut pe = PaintEngine::new(Some(player));
    pe.begin_playback()?;

    let mut initial = true;
    loop {
        let pos = if initial {
            pe.skip_playback(1)
        } else {
            pe.play_playback(interval)
        }?;

        pe.render();
        match pe.to_scaled_image(width, height, true) {
            Ok(img) => {
                ctx.push(img)?;
                initial = false;
            }
            Err(e) => eprintln!("Warning: {}", e),
        }

        if pos == -1 {
            return Ok(());
        }
    }
}

fn make_player(input_path: &String) -> Result<Player> {
    if input_path == "-" {
        Player::new_from_stdin(DP_PLAYER_TYPE_GUESS)
    } else {
        Player::new_from_path(DP_PLAYER_TYPE_GUESS, input_path.clone())
    }
}

fn render_flash(
    writer: &mut dyn io::Write,
    img1: &Image,
    img2: &Image,
    fr: f64,
    color: u32,
) -> Result<()> {
    let pixel = DP_UPixel8 { color };
    let mut dst = Image::new(img1.width(), img1.height())?;
    let mut opa = 0.0_f64;

    while opa < 255.0_f64 {
        opa = 255.0_f64.min(opa + 2400.0_f64 / fr);
        dst.blend_with(img1, pixel, opa as u8)?;
        dst.dump(writer)?;
    }

    while opa > 0.0_f64 {
        opa = 0.0_f64.max(opa - 360.0_f64 / fr);
        dst.blend_with(img2, pixel, opa as u8)?;
        dst.dump(writer)?;
    }

    Ok(())
}

fn render_linger(writer: &mut dyn io::Write, img: &Image, fr: f64, linger_time: f64) -> Result<()> {
    let mut lingered = 0.0;
    while lingered <= linger_time {
        lingered += 1.0_f64 / fr;
        img.dump(writer)?;
    }
    Ok(())
}
