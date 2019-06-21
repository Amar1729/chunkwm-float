// Plugin for better floating window management for chunkwm.
// created using chunkwm-rs bridge.

// #[allow(unused_variables)]

extern crate core_graphics;

#[macro_use]
extern crate structopt;

#[macro_use]
extern crate chunkwm;

use structopt::StructOpt;

use chunkwm::prelude::{ChunkWMError, Event, HandleEvent, LogLevel,
                       Payload, Subscription, API};

mod window;

macro_rules! c_logger {
    ($slf:expr, $n:expr, $x:expr) => (
        // want to take multiple args to fmt tho
        $slf.api.log(LogLevel::Warn, format!("{}: {}", $n, $x))
    );
}

// arguments
#[derive(Debug, StructOpt)]
#[structopt(name = "float", about = "plugin for floating window management.")]
enum Float {
    #[structopt(name = "window")]
    Window {
        /// Center window
        #[structopt(short = "c", long = "center")]
        center: bool,
        /// Set absolute size of window [fxf:fxf]
        #[structopt(short = "a", long = "absolute")]
        absolute: Option<String>,
        /// Decrement window in direction by step_size_dilate
        #[structopt(short = "d", long = "dec")]
        dec_dir: Option<String>,
        /// Increment window in direction size by step_size_dilate
        #[structopt(short = "i", long = "inc")]
        inc_dir: Option<String>,
        /// Move window in direction by step_size_move
        #[structopt(short = "m", long = "move")]
        move_dir: Option<String>,
        /// Temporarily set step size (for move or dilate)
        #[structopt(short = "s", long = "size")]
        size: Option<f32>,
    },
    #[structopt(name = "query")]
    Query {
        /// Get pixel measurement of window [left|right|top|bottom|width|height]
        #[structopt(short = "p", long = "position")]
        position: Option<String>,
        /// Get screen dimensions
        #[structopt(short = "s", long = "screen")]
        all: bool,
    },
}

// Create an event handler. Your handler should be `pub`.
pub struct Plugin {
    api: API,
    //step_size_move: CVar<f32>,
    //step_size_dilate: CVar<f32>,
}

// Create the bridge between the C/C++ plugin and the event handler.
// The string values should be bytes (i.e. b""), and should be null-terminated (i.e. end in '\0').
chunkwm_plugin!{
    Plugin,
    file: b"chunkwm-float/src/lib.rs\0",
    name: b"float\0",
    version: b"0.1.0\0"
}

// Implement `HandleEvent` on the event handler.
impl HandleEvent for Plugin {
    fn new(api: API) -> Plugin {
        api.log(LogLevel::Warn, "chunkwm-float: initialized.");

        Plugin {
            api,
            //step_size_move: CVar::new("step_size_move", api).unwrap(),
            //step_size_dilate: CVar::new("step_size_dilate", api).unwrap(),
        }
    }

    // Subscribe to events.
    subscribe!(
        Subscription::WindowCreated,
        Subscription::WindowMinimized,
        Subscription::ApplicationLaunched
    );

    // Handle events.
    fn handle(&mut self, event: Event) -> Result<(), ChunkWMError> {
        match event {
            Event::WindowCreated(window) => {
                self.api.log(
                    LogLevel::Warn,
                    format!(
                        "win create: {}", window.id()?
                    ),
                );
            }
            Event::DaemonCommand(data) => {
                //let cmd = self.parse_args("--size 0.0 --move west".to_string()).unwrap();
                let res = self.command_handler(data);
                c_logger!(self, "result", res);
            }
            _ => (),
        };

        Ok(())
    }

    // Shutdown the handler.
    fn shutdown(&self) {
        self.api
            .log(LogLevel::Debug, "chunkwm-float: shutting down.")
    }
}

// ---- ---- custom trait+methods for Plugin

pub trait DaemonHandler {
    fn command_handler(&self, payload: Payload) -> String;
    fn get_move_size(&self, size: Option<f32>) -> f32;
    fn get_dilate_size(&self, size: Option<f32>) -> f32;
}

impl DaemonHandler for Plugin {
    fn command_handler(&self, payload: Payload) -> String {
        // need a nonempty command and message
        let cmd = match payload.command() {
            Ok(v) => v,
            Err(e) => {
                return String::from(format!("chunkwm-float: error: {}", e));
            }
        };

        let msg = match payload.message() {
            Ok(v) => v,
            Err(e) => {
                return String::from(format!("chunk-float: error: {}", e));
            }
        };

        let args = format!("chunkc {} {}", cmd, msg);
        let options = Float::from_iter_safe(args.split(" "));
        c_logger!(self, "opt", options);

        match options {
            Ok(Float::Window { center, absolute, dec_dir, inc_dir, move_dir, size }) => {
                match move_dir {
                    Some(val) => { window::window_move(val, self.get_move_size(size)) }
                    None => {},
                }
            }
            Ok(Float::Query { position, all }) => {
                // how to do network comm
            }
            _ => { return String::from("unknown operation") }
        }

        return String::from("ok");
    }

    fn get_move_size(&self, size: Option<f32>) -> f32 {
        if let Some(size) = size {
            size
        } else {
            //self.step_size_move.value().unwrap()
            0.0
        }
    }

    fn get_dilate_size(&self, size: Option<f32>) -> f32 {
        if let Some(size) = size {
            size
        } else {
            //self.step_size_dilate.value().unwrap()
            0.0
        }
    }
}
