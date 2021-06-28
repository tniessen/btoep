**Table of Contents**
- [Usage](#usage)
- [Example](#example)
- [Building](#building)
- [Testing](#testing)
- [Supported Platforms](#supported-platforms)

## Usage

Each tool provides detailed information if `--help` is added to the command
line.

- **btoep-add** adds data to a new or existing dataset.
- **btoep-create** creates a new dataset.
- **btoep-find-offset** locates existing or missing data within a dataset.
- **btoep-get-index** allows compacting the index file for transmission.
- **btoep-list-ranges** lists existing or missing sections within a dataset.
- **btoep-read** reads existing data from a dataset.
- **btoep-set-size** changes the size of a new or existing dataset.

## Example

This assumes a Linux environment with a bash shell, but can be adapted for any
other supported platform. The directory containing the compiled tools should be
in the `PATH` environment variable.

<p align="center">
  <a href="https://asciinema.org/a/357120">
    <img src="./.github/media/basic-usage.svg">
  </a>
</p>

1. Create a new dataset from the string `Hello world!`:

   ```
   echo Hello world! | btoep-add --dataset=hello --offset=0
   ```

2. Display the dataset:

   ```
   btoep-read --dataset=hello
   ```

3. It is helpful to know the total size of a dataset. Assume our dataset has a
   size of 900 bytes:

   ```
   btoep-set-size --dataset=hello --size=900
   ```

4. Let's find out how much data is still missing:

   ```
   btoep-list-ranges --dataset=hello --missing
   ```

5. Add more data somewhere in the middle of the dataset:

   ```
   echo SPACE | btoep-add --dataset=hello --offset=450
   ```

6. btoep automatically tracks what parts are missing. List missing ranges again:

   ```
   btoep-list-ranges --dataset=hello --missing
   ```

7. Try reading the file again. btoep automatically stops after the first data
   section, since it doesn't know what data belongs there.

   ```
   btoep-read --dataset=hello
   ```

8. Let's find out where the second section is.

   ```
   end_of_first=$(btoep-find-offset --dataset=hello --start-at=0 --stop-at=no-data)
   start_of_second=$(btoep-find-offset --dataset=hello --start-at=$end_of_first --stop-at=data)
   ```

9. To display the second section, type in:

   ```
   btoep-read --dataset=hello --offset=$start_of_second
   ```

## Building

CMake and a C11 compiler are required. The easiest way to build everything
without any configuration is this:

1. Create a `build` directory within the repository, and switch to the newly
   created folder.

   ```
   mkdir build
   cd build
   ```

2. Generate build scripts for your platform and compiler:

   ```
   cmake ..
   ```

   Add options to the above command to customize the build process. For example,
   adding `-DCMAKE_BUILD_TYPE=Release` produces faster binaries at the cost of
   being harder to debug.

3. Build everything:

   ```
   cmake --build .
   ```

## Testing

Ensure that the project was built successfully as described above. From within
the `build` directory, run `ctest` to execute all unit and system tests.

## Supported Platforms

This project uses [GitHub Actions](.github/workflows) for automated testing on
supported platforms. These tests cover the following platforms:

| Operating System   | Compiler   |
| ------------------ | ---------- |
| Ubuntu (latest)    | GNU        |
| macOS (latest)     | AppleClang |
| Windows (latest)   | MSVC       |
| WSL (Ubuntu 20.04) | GNU        |
