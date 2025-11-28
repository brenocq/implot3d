# Contributing to ImPlot3D

First off, thank you for considering contributing to ImPlot3D! This project is growing quickly, and we welcome your help. Whether you have ideas to share, bugs to report, or features to implement, your contributions are highly valued.

## How to Contribute

There are several ways you can contribute to the project, and we appreciate all of them.

### 1. Propose Ideas and Discuss Features

Your feedback is crucial in guiding the development of ImPlot3D.

  - **Explore the Roadmap**: Check out the [next features roadmap](https://github.com/brenocq/implot3d/issues?q=is%3Aissue%20state%3Aopen%20sort%3Aupdated-desc) to see what's planned.
  - **Discuss**: Leave comments on existing issues to help refine ideas.
  - **Request a Feature**: If you have a new idea, please open a [feature request issue](https://github.com/brenocq/implot3d/issues/new?template=feature_request.md).

### 2. Report Bugs

If you encounter a bug, please help us by reporting it.

  - **Open a Bug Report**: Use the [bug report template](https://github.com/brenocq/implot3d/issues/new?template=bug_report.md) to create an issue. Please include as much detail as possible, such as reproduction steps, screenshots, videos.

### 3. Implement Features or Fix Bugs

If you're ready to write some code, we'd love your help.

1.  **Indicate Your Intent**: To avoid duplicated effort, please comment on the relevant issue or create a new one to let the community know you plan to work on it.
2.  **Fork**: Fork the repository.
3.  **Implement**: Write your code, following the project's standards outlined below.
4.  **Open a Pull Request**: Submit a [pull request](https://github.com/brenocq/implot3d/compare) targeting the `main` branch.
    - Note: Make sure to include a demo in `implot3d_demo.cpp` if you are adding a new feature.

-----

## Development Guidelines

To ensure the codebase remains consistent, high-quality, and easy to maintain, please follow these guidelines.

### Code Style and Formatting

  - **C++ Best Practices**: Adhere to idiomatic C++ and maintain high code quality. Do not use the C++ standard library. Only use C++ features available in C++11. 
  - **Maintain ImGui/ImPlot3D Style**: Ensure new code integrates seamlessly with the existing codebase's style and API patterns.
  - **Formatting**: Use **`clang-format`** to format your code before committing. This ensures a consistent style across the entire project.

### Commit Messages

Follow the **[Conventional Commits](https://www.conventionalcommits.org/)** specification to maintain a clear and informative git history. Each commit message should be prefixed with a type, such as `feat:`, `fix:`, or `docs:`.

**Commit Types:**

  - **feat**: A new feature.
  - **fix**: A bug fix.
  - **refactor**: Code changes that neither fix a bug nor add a feature.
  - **docs**: Documentation only changes.
  - **style**: Changes that do not affect the meaning of the code (e.g., `clang-format`).
  - **test**: Adding missing tests or correcting existing tests.
  - **chore**: Changes to the build process or auxiliary tools.

**Examples:**

```
feat: add support for plotting 3D scatter plots
fix: correct rendering issue with large datasets
docs: update installation instructions
style: apply clang-format to codebase
refactor: restructure internal data handling for performance
test: add unit tests for plot data validation
chore: update ImGui dependency to latest version
```

### Documentation and Demos

  - **Public APIs**: All new public APIs should be clearly documented with comments in the header files.
  - **Breaking Changes**: We should avoid modifying the public/internal APIs as much as possible. If a breaking change in unnavoidable, update the `API BREAKING CHANGES` log in `implot3d.cpp` documenting the breaking change and instructions on how to migrate the code. If possible, mark the function as obsolete instead of deleting it.
  - **Complex Logic**: Add comments to explain any complex or non-obvious implementation details.
  - **Demos**: When you add a new feature, please include a demonstration in `implot3d_demo.cpp` to showcase its usage.
