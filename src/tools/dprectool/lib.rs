// SPDX-License-Identifier: GPL-3.0-or-later
use anyhow::Result;
use drawdance::{
    dp_cmake_config_version,
    engine::{Player, Recorder},
    msg::{InternalMessage, Message},
    DP_MessageType, DP_PlayerPass, DP_PlayerType, DP_RecorderType, DP_MSG_DRAW_DABS_CLASSIC,
    DP_MSG_DRAW_DABS_MYPAINT, DP_MSG_DRAW_DABS_PIXEL, DP_MSG_DRAW_DABS_PIXEL_SQUARE,
    DP_MSG_FILL_RECT, DP_MSG_PUT_IMAGE, DP_PLAYER_COMPATIBLE, DP_PLAYER_MINOR_INCOMPATIBILITY,
    DP_PLAYER_PASS_ALL, DP_PLAYER_PASS_CLIENT_PLAYBACK, DP_PLAYER_PASS_FEATURE_ACCESS,
    DP_PLAYER_TYPE_BINARY, DP_PLAYER_TYPE_GUESS, DP_PLAYER_TYPE_TEXT, DP_PROTOCOL_VERSION,
    DP_RECORDER_TYPE_BINARY, DP_RECORDER_TYPE_TEXT,
};
use std::{
    collections::{HashMap, HashSet},
    ffi::{c_int, c_uint, CStr},
    str::FromStr,
};

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
        output_path: &str,
        output_path_is_default: bool,
        player: &Player,
    ) -> DP_RecorderType {
        match self {
            Self::Binary => DP_RECORDER_TYPE_BINARY,
            Self::Text => DP_RECORDER_TYPE_TEXT,
            _ => Self::guess_recorder_format(output_path, output_path_is_default, player),
        }
    }

    fn guess_recorder_format(
        output_path: &str,
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

#[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
pub enum Pass {
    #[default]
    All,
    Template,
    Client,
}

impl Pass {
    fn to_player_type(self) -> DP_PlayerPass {
        match self {
            Self::All => DP_PLAYER_PASS_ALL,
            Self::Template => DP_PLAYER_PASS_FEATURE_ACCESS,
            Self::Client => DP_PLAYER_PASS_CLIENT_PLAYBACK,
        }
    }
}

impl std::str::FromStr for Pass {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "all" => Ok(Self::All),
            "template" => Ok(Self::Template),
            "client" => Ok(Self::Client),
            _ => Err(
                "invalid pass option, should be one of 'all', 'template' or 'client'".to_owned(),
            ),
        }
    }
}

#[no_mangle]
pub extern "C" fn dprectool_main() -> c_int {
    drawdance::init();

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
        /// Specifies which messages are passed through when converting. One of
        /// 'all' (the default) to pass through all messages, 'template' to pass
        /// through only messages that are relevant for session templates and
        /// 'client' to pass through only messages that are relevant for
        /// playback in the client.
        optional -p,--pass pass: Pass
        /// Include only visible messages from the given user ids,
        /// comma-separated. Use -u/--users to find out which ids there are.
        optional --include-user-ids include_user_ids: String
        /// Print message frequency table and exit.
        optional --msg-freq
        /// Print information about which users were part of the recordings.
        optional -u,--users
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
                eprintln!("{}", e);
                1
            }
        };
    }

    let acl_override = !flags.acl;
    if flags.users {
        return match print_user_list(input_format, input_path, acl_override) {
            Ok(_) => 0,
            Err(e) => {
                eprintln!("{}", e);
                1
            }
        };
    }

    let pass = flags.pass.unwrap_or_default();
    if flags.msg_freq {
        return match print_message_frequency(input_format, input_path, acl_override, pass) {
            Ok(_) => 0,
            Err(e) => {
                eprintln!("{}", e);
                1
            }
        };
    }

    let mut include_context_ids: HashSet<c_uint> = HashSet::new();
    if let Some(include_user_ids) = flags.include_user_ids {
        for s in include_user_ids.split(',') {
            match s.parse::<c_uint>() {
                Ok(context_id) => include_context_ids.insert(context_id),
                Err(e) => {
                    eprintln!("Can't parse include-user-id '{}': {}", s, e);
                    return 2;
                }
            };
        }
    }

    let output_path_is_default = flags.out.is_none();
    let output_path = flags.out.unwrap_or_else(|| "-".to_owned());
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
        pass,
        include_context_ids,
    ) {
        Ok(_) => 0,
        Err(e) => {
            eprintln!("{}", e);
            1
        }
    }
}

fn print_version(input_format: InputFormat, input_path: String) -> Result<()> {
    let player = make_player(input_format, input_path)?;

    let compat_flag = match player.compatibility() {
        DP_PLAYER_COMPATIBLE => "C",
        DP_PLAYER_MINOR_INCOMPATIBILITY => "M",
        _ => "I",
    };
    let format_version = player
        .format_version()
        .unwrap_or_else(|| "(unknown)".to_owned());
    let writer_version = player
        .writer_version()
        .unwrap_or_else(|| "(no writer version)".to_owned());

    println!("{} {} {}", compat_flag, format_version, writer_version);

    Ok(())
}

#[derive(Default, Clone)]
struct UserStats {
    context_id: c_uint,
    name: String,
    total_count: usize,
    total_size: usize,
    stroke_count: usize,
    undo_count: usize,
    redo_count: usize,
}

impl UserStats {
    fn add(&mut self, msg: &Message) {
        let size = msg.length();
        self.total_count += 1;
        self.total_size += size;
        match msg.to_internal() {
            InternalMessage::PenUp() => self.stroke_count += 1,
            InternalMessage::Undo(undo) => {
                if undo.is_redo() {
                    self.redo_count += 1;
                } else {
                    self.undo_count += 1;
                }
            }
            _ => (),
        }
    }
}

fn print_user_list(
    input_format: InputFormat,
    input_path: String,
    acl_override: bool,
) -> Result<()> {
    let mut player = make_player(input_format, input_path).and_then(Player::check_compatible)?;
    player.set_acl_override(acl_override);
    player.set_pass(Pass::All.to_player_type());

    let mut users: HashMap<String, UserStats> = HashMap::new();
    let mut context_id_to_key: HashMap<c_uint, String> = HashMap::new();
    let mut context_id_to_name: HashMap<c_uint, String> = HashMap::new();
    while let Some(msg) = player.step()? {
        let context_id = msg.context_id();
        if let InternalMessage::Join(join) = msg.to_internal() {
            let name = join.name();
            context_id_to_key.insert(context_id, format!("{}:{}", context_id, name));
            context_id_to_name.insert(context_id, name);
        };

        let key = context_id_to_key
            .get(&context_id)
            .map_or_else(|| context_id.to_string(), Clone::clone);

        let entry = users.entry(key).or_insert_with(|| UserStats {
            context_id,
            name: context_id_to_name
                .get(&context_id)
                .map(Clone::clone)
                .unwrap_or_default(),
            ..Default::default()
        });
        entry.add(&msg);
    }

    println!(
        "id  | bytes sent           | message count        \
             | stroke count         | undo count           \
             | redo count           | name"
    );

    let mut keys: Vec<String> = users.keys().cloned().collect();
    keys.sort_unstable();
    for key in keys {
        let value = &users[&key];
        println!(
            "{:<3} | {:20} | {:20} | {:20} | {:20} | {:20} | {}",
            value.context_id,
            value.total_size,
            value.total_count,
            value.stroke_count,
            value.undo_count,
            value.redo_count,
            value.name
        );
    }

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
    pass: Pass,
) -> Result<()> {
    let mut player = make_player(input_format, input_path).and_then(Player::check_compatible)?;
    player.set_acl_override(acl_override);
    player.set_pass(pass.to_player_type());

    let mut total = MessageCount::default();
    let mut types: HashMap<DP_MessageType, MessageCount> = HashMap::new();
    while let Some(msg) = player.step()? {
        let length = msg.length();
        total.add(length);
        types.entry(msg.message_type()).or_default().add(length);
    }

    let mut keys: Vec<DP_MessageType> = types.keys().copied().collect();
    keys.sort_unstable();
    for key in keys {
        let value = types[&key];
        println!(
            "{:02x} {:17} {:9} {:9} ({:.2}%)",
            key,
            Message::message_type_name(key),
            value.count,
            value.length,
            value.length as f64 / total.length as f64 * 100.0_f64
        );
    }
    println!("Total count: {}", total.count);
    println!(
        "Total length: {} ({:.2} MB)",
        total.length,
        total.length as f64 / (1024.0_f64 * 1024.0_f64)
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
    pass: Pass,
    include_context_ids: HashSet<c_uint>,
) -> Result<()> {
    let mut player = make_player(input_format, input_path).and_then(Player::check_compatible)?;

    let rtype = output_format.to_recorder_type(&output_path, output_path_is_default, &player);
    let mut recorder = if output_path == "-" {
        Recorder::new_from_stdout(rtype, player.header())
    } else {
        Recorder::new_from_path(rtype, player.header(), output_path)
    }?;

    player.set_acl_override(acl_override);
    player.set_pass(pass.to_player_type());
    while let Some(msg) = player.step()? {
        if should_include(&msg, &include_context_ids) && !recorder.push_noinc(msg) {
            break;
        }
    }

    recorder.dispose()?;
    Ok(())
}

fn make_player(input_format: InputFormat, input_path: String) -> Result<Player> {
    let ptype = input_format.to_player_type();
    if input_path == "-" {
        Player::new_from_stdin(ptype)
    } else {
        Player::new_from_path(ptype, input_path)
    }
}

fn should_include(msg: &Message, include_context_ids: &HashSet<c_uint>) -> bool {
    include_context_ids.is_empty()
        || include_context_ids.contains(&msg.context_id())
        || !matches!(
            msg.message_type(),
            DP_MSG_PUT_IMAGE
                | DP_MSG_FILL_RECT
                | DP_MSG_DRAW_DABS_CLASSIC
                | DP_MSG_DRAW_DABS_PIXEL
                | DP_MSG_DRAW_DABS_PIXEL_SQUARE
                | DP_MSG_DRAW_DABS_MYPAINT
        )
}
