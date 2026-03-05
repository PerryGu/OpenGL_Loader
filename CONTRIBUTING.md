# Contributing to OpenGL_Loader

Thank you for your interest in OpenGL_Loader! This document provides guidelines for contributing to the project.

## Code Style

- **C++ Standard**: C++17
- **Naming Conventions**:
  - Classes: `PascalCase` (e.g., `ModelManager`)
  - Functions: `camelCase` (e.g., `loadModel`)
  - Member variables: `m_` prefix (e.g., `m_showSkeleton`)
  - Constants: `UPPER_CASE` (e.g., `MAX_RECENT_FILES`)
- **Comments**: Use Doxygen-style comments for public APIs
- **Logging**: Use `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR` from `logger.h` (no `std::cout`)

## Development Setup

1. Clone the repository
2. Follow the installation instructions in `README.md`
3. Ensure all tests pass (if applicable)
4. Build in Debug mode for development, Release mode for testing

## Pull Request Process

1. Create a feature branch from `main`
2. Make your changes with clear, descriptive commit messages
3. Ensure code compiles without warnings
4. Test your changes thoroughly
5. Update documentation if needed
6. Submit a pull request with a clear description

## Areas for Contribution

- Performance optimizations
- Bug fixes
- Documentation improvements
- UI/UX enhancements
- Cross-platform compatibility
- Additional file format support

## Questions?

Feel free to open an issue for questions or discussions about the project.
