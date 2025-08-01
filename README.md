# CPPX - C++ Project Manager

CPPX is a modern CLI application for managing C++ projects, simplifying building, testing, and documenting your C++ codebase.

## Features
- Create, build, and run C++ projects
- Manage dependencies and libraries (Conan)
- Configure project and build system settings
- Documentation support (Doxygen)
- Clean project artifacts
- Run tests, examples, and benchmarks
- Export configuration to CMake
- GitHub integration (fetch repository info)

## Installation
1. Requirements:
   - C++17+
   - Conan
   - Doxygen
   - Libraries: fmt, CLI11, toml++, nlohmann/json, cURL
2. Build the project:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```
3. Run:
   ```bash
   ./cppx --help
   ```

## Quick Start
```bash
cppx project new --name MyProject
cppx project set --name MyProject --path ./MyProject
cppx build
cppx run
cppx test
cppx doc
```

## Documentation
- [Doxygen](https://www.doxygen.nl/)
- [Conan](https://conan.io/)

## Contributing
See `CONTRIBUTING_en.md` for details.

## License
This project is licensed under the Apache 2.0 License. See `LICENSE` for details.



## Contact
- GitHub: [repository](https://github.com/dotsowiet/cppx)

---
