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

# Generate source files list for CMake.
sources:
    #!/bin/bash
    set -e
    echo "set(headers \${headers}" > test.txt
    headers=$(find . -name \*\.h | sort)
    echo "${headers}" >> test.txt
    echo ")" >> test.txt
    echo "set(sources \${sources}" >> test.txt
    echo "    \${headers}" >> test.txt
    cpps=$(find . -name \*\.cpp | sort)
    echo "${cpps}" >> test.txt
    echo ")" >> test.txt
    sed -e 's/^\.\//    /' test.txt > cmake/sourcelist.cmake
    rm test.txt
