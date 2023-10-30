// SPDX-License-Identifier: GPL-3.0-or-later
use anyhow::Result;
use drawdance::{
    dp_cmake_config_version,
    engine::{PaintEngine, Player},
    DP_PLAYER_TYPE_GUESS, DP_PROTOCOL_VERSION,
};
use std::{
    ffi::{c_int, CStr, OsStr},
    fs::metadata,
    path::Path,
    str::FromStr,
};

#[derive(Copy, Clone, Debug)]
pub struct ImageSize {
    width: usize,
    height: usize,
}

impl FromStr for ImageSize {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if let Some((a, b)) = s.to_lowercase().split_once('x') {
            let width: usize = a.parse().unwrap_or(0);
            let height: usize = b.parse().unwrap_or(0);
            if width != 0 && height != 0 {
                return Ok(ImageSize { width, height });
            }
        }
        Err(format!(
            "Invalid image size '{s}', must be given as WIDTHxHEIGHT"
        ))
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
pub enum OutputFormat {
    #[default]
    Guess,
    Ora,
    Png,
    Jpg,
    Jpeg,
}

impl OutputFormat {
    fn suffix(self) -> &'static str {
        match self {
            Self::Ora => "ora",
            Self::Png => "png",
            Self::Jpg => "jpg",
            Self::Guess | Self::Jpeg => "jpeg",
        }
    }
}

impl FromStr for OutputFormat {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "guess" => Ok(Self::Guess),
            "ora" => Ok(Self::Ora),
            "png" => Ok(Self::Png),
            "jpg" => Ok(Self::Jpg),
            "jpeg" => Ok(Self::Jpeg),
            _ => Err(format!(
                "invalid output format '{s}', should be one of 'guess', \
                'ora', 'png', 'jpg' or 'jpeg'"
            )),
        }
    }
}

#[derive(Clone, Copy)]
enum Every {
    None,
    UndoPoint,
    Message,
}

#[no_mangle]
pub extern "C" fn drawpile_cmd_main() -> c_int {
    drawdance::init();

    let flags = xflags::parse_or_exit! {
        /// Displays version information and exits.
        optional -v,--version
        /// Print extra debugging information.
        optional -V,--verbose
        /// Output file format. One of 'guess' (the default), 'ora', 'png',
        /// 'jpg' or 'jpeg'. The last two are the same format, just with a
        /// different extension. Guessing will either use the extension from the
        /// value given by -o/--out or default to jpeg.
        optional -f,--format output_format: OutputFormat
        /// Output file pattern. Use '-' for stdout. The default is to use the
        /// input file name with a different file extension.
        optional -o,--out output_pattern: String
        /// Merging annotations is not supported. Passing this option will just
        /// print a warning and has no effect otherwise.
        optional -a,--merge-annotations
        /// Export image every n sequences. A sequence is anything that can be
        /// undone, like a single brush stroke. This option is mutually
        /// incompatible with --every-msg.
        optional -e,--every-seq every_seq: i64
        /// Export image every n messages. A message is a fine-grained drawing
        /// command, a single brush stroke can consist of dozens of these. This
        /// option is mutually incompatible with --every-seq.
        optional -m,--every-msg every_seq: i64
        /// Performs ACL filtering. This will filter out any commands that the
        /// user wasn't allowed to actually perform, such as drawing on a layer
        /// that they didn't have permission to draw on. The Drawpile client
        /// would also filter these out when playing back a recording.
        optional -A,--acl
        /// Maximum image size. Any images larger than this will be scaled down,
        /// retaining the aspect ratio. This option is not supported for the ora
        /// output format.
        optional -s,--maxsize max_size: ImageSize
        /// Make all images the same size. If --maxsize is given, that size is
        /// used, otherwise it is guessed from the first saved image. All images
        /// will be scaled, retaining aspect ratio. Blank space is left
        /// transparent or black, depending on the output format. This option is
        /// not supported for the output ora format.
        optional -S,--fixedsize
        /// Input recording file(s).
        repeated input: String
    };

    if flags.version {
        println!("drawpile-cmd {}", dp_cmake_config_version());
        println!(
            "Protocol version: {}",
            CStr::from_bytes_with_nul(DP_PROTOCOL_VERSION)
                .unwrap()
                .to_str()
                .unwrap()
        );
        return 0;
    }

    if flags.merge_annotations {
        eprintln!("Warning: -a/--merge-annotations has no effect");
    }

    let (every, steps) = if flags.every_seq.is_some() && flags.every_msg.is_some() {
        eprintln!("-e/--every-seq and -m/--every-msg are mutually incompatible");
        return 2;
    } else if let Some(every_seq) = flags.every_seq {
        if every_seq < 1 {
            eprintln!("-e/--every-seq must be >= 1");
            return 2;
        }
        (Every::UndoPoint, every_seq)
    } else if let Some(every_msg) = flags.every_msg {
        if every_msg < 1 {
            eprintln!("-m/--every-msg must be >= 1");
            return 2;
        }
        (Every::Message, every_msg)
    } else {
        (Every::None, 0_i64)
    };

    let input_paths = flags.input;
    if input_paths.is_empty() {
        eprintln!("No input file given");
        return 2;
    }

    let mut format = flags.format.unwrap_or_default();
    let pre_out_pattern = if let Some(out) = flags.out {
        if metadata(&out).map(|m| m.is_dir()).unwrap_or(false) {
            format!(
                "{}/{}-:idx:.{}",
                out,
                Path::new(&input_paths[0])
                    .file_stem()
                    .unwrap_or_default()
                    .to_string_lossy(),
                format.suffix()
            )
        } else {
            out
        }
    } else {
        let first_path = Path::new(&input_paths[0]);
        format!(
            "{}{}-:idx:.{}",
            get_dir_prefix(first_path),
            first_path.file_stem().unwrap_or_default().to_string_lossy(),
            format.suffix()
        )
    };

    if format == OutputFormat::Guess {
        format = if let Some(ext) = Path::new(&pre_out_pattern)
            .extension()
            .map(OsStr::to_string_lossy)
        {
            match ext.to_lowercase().as_str() {
                "ora" => OutputFormat::Ora,
                "png" => OutputFormat::Png,
                "jpg" => OutputFormat::Jpg,
                "jpeg" => OutputFormat::Jpeg,
                _ => {
                    eprintln!("Can't guess output format for extension '.{}'", ext);
                    return 2;
                }
            }
        } else {
            eprintln!("Can't guess output format from '{}'", pre_out_pattern);
            return 2;
        }
    }

    if flags.maxsize.is_some() && format == OutputFormat::Ora {
        eprintln!("The ora output format doesn't support -s/--max-size");
        return 2;
    }

    let out_path = Path::new(&pre_out_pattern);
    let out_dir = get_dir_prefix(out_path);
    let out_stem = out_path.file_stem().unwrap_or_default().to_string_lossy();
    let out_suffix = format.suffix();
    let out_pattern = if pre_out_pattern.contains(":idx:") {
        format!("{}{}.{}", out_dir, out_stem, out_suffix)
    } else {
        format!("{}{}-:idx:.{}", out_dir, out_stem, out_suffix)
    };

    let acl_override = !flags.acl;

    match dump_recordings(
        &input_paths,
        acl_override,
        every,
        steps,
        flags.maxsize,
        flags.fixedsize,
        format,
        &out_pattern,
    ) {
        Ok(_) => 0,
        Err(e) => {
            eprintln!("{}", e);
            1
        }
    }
}

fn get_dir_prefix(path: &Path) -> String {
    let dir = path.parent().map(Path::to_string_lossy).unwrap_or_default();
    if dir.is_empty() {
        String::new()
    } else {
        format!("{}/", dir)
    }
}

fn dump_recordings(
    input_paths: &Vec<String>,
    acl_override: bool,
    every: Every,
    steps: i64,
    max_size: Option<ImageSize>,
    fixed_size: bool,
    format: OutputFormat,
    out_pattern: &str,
) -> Result<()> {
    let mut index = 1;
    for input_path in input_paths {
        dump_recording(
            &mut index,
            input_path,
            acl_override,
            every,
            steps,
            max_size,
            fixed_size,
            format,
            out_pattern,
        )?;
    }
    Ok(())
}

fn dump_recording(
    index: &mut i32,
    input_path: &String,
    acl_override: bool,
    every: Every,
    steps: i64,
    max_size: Option<ImageSize>,
    fixed_size: bool,
    format: OutputFormat,
    out_pattern: &str,
) -> Result<()> {
    let mut player = make_player(input_path).and_then(Player::check_compatible)?;
    player.set_acl_override(acl_override);

    let mut pe = PaintEngine::new(Some(player));
    pe.begin_playback()?;

    loop {
        let pos = match every {
            Every::None => skip_playback_to_end(&mut pe)?,
            Every::Message => pe.step_playback(steps)?,
            Every::UndoPoint => pe.skip_playback(steps)?,
        };

        pe.render();
        let path = out_pattern.replace(":idx:", &index.to_string());
        *index += 1;

        match format {
            OutputFormat::Ora => pe.write_ora(&path)?,
            OutputFormat::Png | OutputFormat::Jpg | OutputFormat::Jpeg => {
                write_flat_image(&mut pe, max_size, fixed_size, format, &path)?;
            }
            OutputFormat::Guess => panic!("Unhandled output format"),
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

fn skip_playback_to_end(pe: &mut PaintEngine) -> Result<i64> {
    loop {
        if pe.skip_playback(99999)? == -1 {
            break;
        }
    }
    Ok(-1)
}

fn write_flat_image(
    pe: &mut PaintEngine,
    max_size: Option<ImageSize>,
    fixed_size: bool,
    format: OutputFormat,
    path: &str,
) -> Result<()> {
    let result = if let Some(ImageSize { width, height }) = max_size {
        if fixed_size {
            pe.to_scaled_image(width, height, true)
        } else if pe.render_width() <= width && pe.render_height() < height {
            pe.to_image()
        } else {
            pe.to_scaled_image(width, height, false)
        }
    } else {
        pe.to_image()
    };
    match result {
        Ok(img) => match format {
            OutputFormat::Png => img.write_png(path)?,
            OutputFormat::Jpg | OutputFormat::Jpeg => img.write_jpeg(path)?,
            _ => panic!("Unhandled output format"),
        },
        Err(e) => eprintln!("Warning: {}", e),
    }
    Ok(())
}
