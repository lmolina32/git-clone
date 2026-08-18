# Git Clone

A from-scratch reimplementation of core Git functionality in C, built as an educational exercise. `git_clone` reads and writes real Git repositories: objects, refs, the index, and commits are all binary-compatible with actual Git. Note, the command set is intentionally simplified. The code loosely follows the structure of ["Write Yourself a Git"](https://wyag.thb.lt/).

## Building

```bash
make            # Build the program
make clean      # Remove build artifacts
make test       # Build and run the full unit test suite
```

The executable will be created at `bin/git_clone`.

## Usage

The program supports multiple subcommands, mirroring a small subset of real Git's plumbing and porcelain:

```bash
# Initialize a repository
./bin/git_clone init [directory]

# Hash a file as a Git object, optionally storing it
./bin/git_clone hash-object [-w] [-t TYPE] <filename>

# Print the raw contents of a repository object
./bin/git_clone cat-file <type> <object>

# Print commit history as Graphviz dot output
./bin/git_clone log [commit]

# List the contents of a tree object
./bin/git_clone ls-tree [-r] <tree-ish>

# Check out a commit into an empty directory
./bin/git_clone checkout <commit> <path>

# List all references (branches and tags)
./bin/git_clone show-ref

# List tags, or create a lightweight/annotated tag
./bin/git_clone tag [-a] [name] [object]

# Resolve a name (HEAD, branch, tag, short hash) to a full object hash
./bin/git_clone rev-parse [--wyag-type TYPE] <name>

# List files currently in the staging area
./bin/git_clone ls-files [--verbose]

# Check paths against .gitignore rules
./bin/git_clone check-ignore <path>...

# Show branch, staged, unstaged, and untracked changes
./bin/git_clone status

# Stage file contents
./bin/git_clone add <path>...

# Remove files from the working tree and the index
./bin/git_clone rm <path>...

# Record staged changes as a new commit
./bin/git_clone commit -m <message>
```

Run `./bin/git_clone -h` or `./bin/git_clone --help` for a summary at any time.

### Exit Codes

- `0` - Success
- `1` - Command error or invalid input

## Testing

Run the test suite to verify program functionality:

```bash
make test              # Build and run every unit test binary
scripts/run_unit.sh    # Run a single test binary's suite directly
```

Each module has a corresponding unit test binary under `test/`, built into `bin/unit_<module>` and driven by `scripts/run_unit.sh`, which runs every numbered test case in each binary under Valgrind (Linux) or `leaks` (macOS) and reports pass/fail with memory-safety diagnostics on failure.

## Project Structure

```
git_clone/
├── include/            # Public headers, one per module
├── src/                # Implementation files, mirroring include/
├── test/               # Unit tests, one per module (unit_<module>.c)
├── scripts/            # Test runner
├── build/              # Compiled object files (generated)
├── bin/                # Final executable and unit_* test binaries (generated)
└── Makefile
```

## Requirements

- GCC or compatible C compiler supporting `gnu23`
- zlib
- GNU Make
- [Valgrind](https://valgrind.org/) (Linux) or `leaks` (macOS), for running the test suite

## Known Limitations

This is an educational implementation, not a production Git client:

- **No packfile support**, only loose objects are read and written.
- **`checkout` requires an empty target directory**, no worktree safety checks or partial checkouts.
- **`.gitignore` matching** uses POSIX `fnmatch` and does not support `**` globs or the "ignore whole directory by name" shorthand.
- **No rename detection** in `status`, renamed files show as a deletion plus an addition.
- **`status` does not rewrite the index** with refreshed file metadata the way real Git does after a clean comparison.
- **Merging, rebasing, and remotes are not implemented.**
