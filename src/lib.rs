// Plugin for better floating window management for chunkwm.
// created using chunkwm-rs bridge.

use std::env;

#[macro_use]
extern crate chunkwm;

use chunkwm::prelude::{ChunkWMError, Event, HandleEvent, LogLevel,
                       Payload, Subscription, API};

// Create an event handler. Your handler should be `pub`.
pub struct Plugin {
    api: API,
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
                let res = command_handler(data);
                self.api.log(
                    LogLevel::Warn,
                    format!("result: {:?}", res),
                );

                let args: Vec<String> = env::args().collect();
                self.api.log(
                    LogLevel::Warn,
                    format!("args: {:?}", args),
                );
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

fn command_handler(p: Payload) -> String {
    // need a nonempty command and message
    let cmd = match p.command() {
        Ok(v) => v,
        Err(e) => {
            return String::from(format!("chunkwm-float: error: {}", e));
        }
    };

    let args = match p.message() {
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
