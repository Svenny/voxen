// EXPERIMENT: umbrella module of the toolkit library, `import tk;` gets all of it.
// Each part is also a separate module (`import tk.toml;` etc.) to import only what's needed.

export module tk;

export import tk.cli;
export import tk.demangle;
export import tk.fs;
export import tk.json;
export import tk.parse_error;
export import tk.png;
export import tk.rng;
export import tk.toml;
