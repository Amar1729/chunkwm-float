// Functions for managing windows

#![allow(unused_imports)]

use std::fs::File;
use std::io::prelude::*;

use std::collections::HashMap;
use std::iter::Iterator;

//use chunkwm::prelude::{ChunkWMError, Event, HandleEvent, LogLevel,
//                       Payload, Subscription, API};

use core_graphics::geometry::{CGPoint, CGSize};
use chunkwm::raw::{AXUIElementRef};
use chunkwm::common::accessibility::{element};

//use chunkwm::{application, display, window};

fn logger(msg: &str) {
    let mut file = File::create("/tmp/float_logger").unwrap();
	file.write_all(msg.as_bytes()).unwrap();
}

// ---- utility functions

// ---- window management/query functions

pub fn window_center() {}
pub fn window_absolute(coord: String) {}
pub fn window_dec(dir: String, size: f32) {}
pub fn window_inc(dir: String, size: f32) {}

pub fn window_move(dir: String, size: f32) {
    unsafe {
        let app_ref: AXUIElementRef = element::get_focused_application();
        let window_ref: AXUIElementRef = element::get_focused_window(app_ref);

        let mut res: f32 = 0.0;
        match dir.as_ref() {
            "north" => res = 1200.0,
            "south" => res = 1200.0,
            "west" => res = 1920.0,
            "east" => res = 1920.0,
            _ => {} // implicitly don't move window on wrong opt
        }

        let step: f64 = (size * res) as f64;

        // naive (no constraint check)
        let point: CGPoint = element::get_window_position(window_ref);
        match dir.as_ref() {
            "north" => {
                let y = point.y - step;
                element::set_window_position(window_ref, point.x as f32, y as f32);
            }
            "south" => {
                let y = point.y + step;
                element::set_window_position(window_ref, point.x as f32, y as f32);
            }
            _ => {}
        }
    }
}
