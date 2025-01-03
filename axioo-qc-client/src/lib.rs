// This module serves as the main entry point for the library, exposing bindings and constants.

pub mod bindings; // Import the bindings module for system information.

pub use bindings::systeminfo::*; // Re-export systeminfo functions for easier access.

pub const DEBUG_MODE: bool = false; // Constant to enable or disable debug mode.
pub const BACKGROUND_MODE: bool = false; // Constant to control console window visibility
pub const JSON_MODE: bool = false; // Constant to control JSON display on startup
