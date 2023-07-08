set windows-shell := ["powershell"]
set shell := ["bash", "-uc"]

# List available recipes
help:
    just -l

# Build for release
build:
    cmake --build --preset vs2022-windows --config Release

# Set up the repo for the first time.
initialize:
    git submodule update --init --recursive
    cmake --preset vs2022-windows
