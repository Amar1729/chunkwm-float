// Plugin for better floating window management for chunkwm.
// created using chunkwm-rs bridge.

// #[allow(unused_variables)]

#[macro_use]
extern crate structopt;

#[macro_use]
extern crate chunkwm;

use structopt::StructOpt;

use chunkwm::prelude::{ChunkWMError, Event, HandleEvent, LogLevel,
                       Payload, Subscription, API};

macro_rules! c_logger {
    ($slf:expr, $n:expr, $x:expr) => (
        // want to take multiple args to fmt tho
        $slf.api.log(LogLevel::Warn, format!("{}: {}", $n, $x))
    );
}

//mod parse;

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

pub struct Command {
    size: f32,
    command: String,
    value: String,
}

pub trait Handler {
    fn parse_args(&self, message: String) -> Result<Command, &'static str>;
    fn command_handler(&self, payload: Payload) -> String;
}

impl Handler for Plugin {
    fn parse_args(&self, message: String) -> Result<Command, &'static str> {
        let mut size: f32 = 0.0;
        let mut cmd: String = String::from("");
        let mut value: String = String::from("");

        let mut get_size: bool = false;
        let mut get_cmd: bool = false;

        c_logger!(self, "msg", message);

        for i in message.split(" ") {
            c_logger!(self, "i", i);

            if get_size && get_cmd {
                self.api.log(
                    LogLevel::Warn,
                    "incorrect args"
                );
                return Err("--size needs param");
            } else if get_size {
                get_size = false;
                size = i.parse().unwrap(); // can do error here
            } else if get_cmd {
                get_cmd = false;
                if value != "" {
                    // passed more than one cmd
                    self.api.log(
                        LogLevel::Warn,
                        "incorrect args"
                    );
                    return Err("too many commands");
                }
                value = String::from(i);
            } else if i == "--size" || i == "-s" {
                get_size = true;
                get_cmd = false;
                continue;
            } else if i == "--move" || i == "-m" {
                get_size = false;
                get_cmd = true;
                cmd = String::from("move");
            }
        }

        Ok(
            Command {
                size : size,
                command : cmd,
                value : value,
            }
        )
    }

    fn command_handler(&self, payload: Payload) -> String {
        // need a nonempty command and message
        let cmd = match payload.command() {
            Ok(v) => v,
            Err(e) => {
                return String::from(format!("chunkwm-float: error: {}", e));
            }
        };

        let _args = match payload.message() {
            Ok(v) => v,
            Err(e) => {
                return String::from(format!("chunk-float: error: {}", e));
            }
        };

        //api.log(LogLevel::Warn, format!("recv {}", cmd));
        match cmd.as_ref() {
            "window" => {
                return String::from("window");
            }
            "query" => {
                return String::from("query");
            }
            e => {
                return String::from(format!("chunkwm-float: unknown command {}", e));
            }
        }
    }
}

/*
fn window_handler(m: String, plugin: &mut Plugin) -> String {
    /*
    let step_size = plugin.step_size_move.value();
    let action = "";
    for arg in m.split(" ") {
        match arg.as_ref() {
            "--size" => {
                step_size = arg;
            }
    }
    */
    let args: Vec<String> = env::args().collect();
    plugin.api.log(
        LogLevel::Warn,
        format!("env: {:?}", args)
    );
    return String::from("wut");
}

fn query_handler(m: String, plugin: &mut Plugin) -> String {
    return String::from("query");
}
*/

