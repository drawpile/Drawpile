// SPDX-License-Identifier: GPL-3.0-or-later
use drawdance::{
    dp_cmake_config_version,
    engine::{Player, PlayerError, Recorder, RecorderError},
    msg::Message,
    DP_MessageType, DP_PlayerType, DP_RecorderType, DP_PLAYER_TYPE_BINARY, DP_PLAYER_TYPE_GUESS,
    DP_PLAYER_TYPE_TEXT, DP_PROTOCOL_VERSION, DP_RECORDER_TYPE_BINARY, DP_RECORDER_TYPE_TEXT,
};
use std::{collections::HashMap, ffi::CStr, str::FromStr};

#[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
pub enum InputFormat {
    #[default]
    Guess,
    Binary,
    Text,
}

impl InputFormat {
    fn to_player_type(self) -> DP_PlayerType {
        match self {
            Self::Guess => DP_PLAYER_TYPE_GUESS,
            Self::Binary => DP_PLAYER_TYPE_BINARY,
            Self::Text => DP_PLAYER_TYPE_TEXT,
        }
    }
}

impl std::str::FromStr for InputFormat {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "guess" => Ok(Self::Guess),
            "binary" => Ok(Self::Binary),
            "text" => Ok(Self::Text),
            _ => Err(format!(
                "invalid input format '{s}', should be one of 'guess', \
                'binary' or 'text'"
            )),
        }
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
pub enum OutputFormat {
    #[default]
    Guess,
    Binary,
    Text,
    Version,
}

impl OutputFormat {
    fn to_recorder_type(
        self,
        output_path: &String,
        output_path_is_default: bool,
        player: &Player,
    ) -> DP_RecorderType {
        match self {
            Self::Binary => DP_RECORDER_TYPE_BINARY,
            Self::Text => DP_RECORDER_TYPE_TEXT,
            _ => self.guess_recorder_format(output_path, output_path_is_default, player),
        }
    }

    fn guess_recorder_format(
        self,
        output_path: &String,
        output_path_is_default: bool,
        player: &Player,
    ) -> DP_RecorderType {
        // No explicit output path means we'll write to text so that we don't
        // end up spewing binary data into a terminal.
        if output_path_is_default {
            DP_RECORDER_TYPE_TEXT
        } else {
            // If the file extension is conclusive, use that format.
            let folded_path = output_path.to_ascii_lowercase();
            if folded_path.ends_with(".dprec") {
                DP_RECORDER_TYPE_BINARY
            } else if folded_path.ends_with(".dptxt") {
                DP_RECORDER_TYPE_TEXT
            } else {
                // Use the "opposite" format of what the input is.
                if player.player_type() == DP_PLAYER_TYPE_TEXT {
                    DP_RECORDER_TYPE_BINARY
                } else {
                    DP_RECORDER_TYPE_TEXT
                }
            }
        }
    }
}

impl FromStr for OutputFormat {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "guess" => Ok(Self::Guess),
            "binary" => Ok(Self::Binary),
            "text" => Ok(Self::Text),
            "version" => Ok(Self::Version),
            _ => Err(format!(
                "invalid output format '{s}', should be one of 'guess', \
                'binary', 'text' or 'version'"
            )),
        }
    }
}

#[derive(Debug)]
pub struct ConversionError {
    message: String,
}

impl From<PlayerError> for ConversionError {
    fn from(err: PlayerError) -> Self {
        ConversionError {
            message: format!(
                "Input error: {}",
                match &err {
                    PlayerError::DpError(s) => s,
                    PlayerError::LoadError(_, s) => s,
                    PlayerError::ResultError(_, s) => s,
                    PlayerError::NulError(_) => "Null path",
                }
            ),
        }
    }
}

impl From<RecorderError> for ConversionError {
    fn from(err: RecorderError) -> Self {
        ConversionError {
            message: format!(
                "Output error: {}",
                match &err {
                    RecorderError::DpError(s) => s,
                    RecorderError::NulError(_) => "Null path",
                }
            ),
        }
    }
}

#[no_mangle]
pub extern "C" fn dprectool_main() -> c_int {
    let flags = xflags::parse_or_exit! {
        /// Displays version information and exits.
        optional -v,--version
        /// Output file. Use '-' for stdout, which is the default.
        optional -o,--out output: String
        /// Output format, one of 'guess' (the default), 'binary' (.dprec) or
        /// 'text' (.dptxt). Alternatively, 'version' will print the recording
        /// version and exit.
        optional -f,--format format: OutputFormat
        /// Input format, one of 'guess' (the default), 'binary' (.dprec) or
        /// 'text' (.dptxt).
        optional -e,--input-format input_format: InputFormat
        /// Performs ACL filtering. This will filter out any commands that the
        /// user wasn't allowed to actually perform, such as drawing on a layer
        /// that they didn't have permission to draw on. The Drawpile client
        /// would also filter these out when playing back a recording.
        optional -A,--acl
        /// Print message frequency table and exit.
        optional --msg-freq
        /// Input recording file.
        optional input: String
    };

    if flags.version {
        println!("dprectool {}", dp_cmake_config_version());
        println!(
            "Protocol version: {}",
            CStr::from_bytes_with_nul(DP_PROTOCOL_VERSION)
                .unwrap()
                .to_str()
                .unwrap()
        );
        return 0;
    }

    let input_path = flags.input.unwrap_or_default();
    if input_path.is_empty() {
        eprintln!("No input file given");
        return 2;
    }

    let input_format = flags.input_format.unwrap_or_default();
    let output_format = flags.format.unwrap_or_default();
    if output_format == OutputFormat::Version {
        return match print_version(input_format, input_path) {
            Ok(_) => 0,
            Err(e) => {
                eprintln!("{}", e.message);
                1
            }
        };
    }

    let acl_override = !flags.acl;
    if flags.msg_freq {
        return match print_message_frequency(input_format, input_path, acl_override) {
            Ok(_) => 0,
            Err(e) => {
                eprintln!("{}", e.message);
                1
            }
        };
    }

    let output_path_is_default = flags.out.is_none();
    let output_path = flags.out.unwrap_or("-".to_owned());
    if output_path.is_empty() {
        eprintln!("No output file given");
        return 2;
    }

    if input_path != "-" && output_path != "-" && output_path == input_path {
        eprintln!("Input and output file can't be identical");
        return 2;
    }

    match convert_recording(
        input_format,
        input_path,
        output_format,
        output_path,
        output_path_is_default,
        acl_override,
    ) {
        Ok(_) => 0,
        Err(e) => {
            eprintln!("{}", e.message);
            1
        }
    }
}

fn print_version(input_format: InputFormat, input_path: String) -> Result<(), ConversionError> {
    let player = make_player(input_format, input_path)?;

    let compat_flag = if player.is_compatible() { "C" } else { "I" };
    let format_version = player.format_version().unwrap_or("(unknown)".to_owned());
    let writer_version = player
        .writer_version()
        .unwrap_or("(no writer version)".to_owned());

    println!("{} {} {}", compat_flag, format_version, writer_version);

    Ok(())
}

#[derive(Default, Clone, Copy)]
struct MessageCount {
    count: usize,
    length: usize,
}

impl MessageCount {
    fn add(&mut self, length: usize) {
        self.count += 1;
        self.length += length;
    }
}

fn print_message_frequency(
    input_format: InputFormat,
    input_path: String,
    acl_override: bool,
) -> Result<(), ConversionError> {
    let mut player = make_player(input_format, input_path)?;
    player.set_acl_override(acl_override);

    let mut total = MessageCount::default();
    let mut types: HashMap<DP_MessageType, MessageCount> = HashMap::new();
    loop {
        match player.step()? {
            Some(msg) => {
                let length = msg.length();
                total.add(length);
                types.entry(msg.message_type()).or_default().add(length);
            }
            None => break,
        };
    }

    let mut keys: Vec<DP_MessageType> = types.keys().cloned().collect();
    keys.sort();
    for key in keys {
        let value = types[&key];
        println!(
            "{:02x} {:17} {:9} {:9} ({:.2}%)",
            key,
            Message::message_type_name(key),
            value.count,
            value.length,
            value.length as f64 / total.length as f64 * 100.0f64
        )
    }
    println!("Total count: {}", total.count);
    println!(
        "Total length: {} ({:.2} MB)",
        total.length,
        total.length as f64 / (1024.0f64 * 1024.0f64)
    );

    Ok(())
}

fn convert_recording(
    input_format: InputFormat,
    input_path: String,
    output_format: OutputFormat,
    output_path: String,
    output_path_is_default: bool,
    acl_override: bool,
) -> Result<(), ConversionError> {
    let mut player = make_player(input_format, input_path)?;

    let rtype = output_format.to_recorder_type(&output_path, output_path_is_default, &player);
    let mut recorder = if output_path == "-" {
        Recorder::new_from_stdout(rtype, player.header())
    } else {
        Recorder::new_from_path(rtype, player.header(), output_path)
    }?;

    player.set_acl_override(acl_override);
    loop {
        match player.step()? {
            Some(msg) => {
                if !recorder.push_noinc(msg) {
                    break;
                }
            }
            None => break,
        };
    }

    recorder.dispose()?;
    Ok(())
}

fn make_player(input_format: InputFormat, input_path: String) -> Result<Player, PlayerError> {
    let ptype = input_format.to_player_type();
    if input_path == "-" {
        Player::new_from_stdin(ptype)
    } else {
        Player::new_from_path(ptype, input_path)
    }
}
