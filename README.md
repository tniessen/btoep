## Building

CMake is required. As usual, create a new directory, configure CMake, and then
invoke the build tool:

```
mkdir build
cd build
cmake ..
cmake --build .
```

## Testing

Ensure that the project was built successfully as described above. From within
the `build` directory, run `ctest` to execute all unit and system tests.

## Supported Platforms

This project uses [GitHub Actions](.github/workflows) for automated testing on
supported platforms. These tests cover the following platforms:

| Operating System   | Compiler   | Notes                                    |
| ------------------ | ---------- | ---------------------------------------- |
| Ubuntu (latest)    | GNU        |                                          |
| macOS (latest)     | AppleClang |                                          |
| Windows (latest)   | MSVC       |                                          |
| WSL (Ubuntu 18.04) | GNU        | Automated testing covers unit tests only |
