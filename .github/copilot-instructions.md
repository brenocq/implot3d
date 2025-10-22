# ImPlot3D Copilot Instructions

ImPlot3D is a C++ library for 3D plotting, extending Dear ImGui. It is designed to be efficient, flexible, and easy to use for real-time 3D data visualization.

## General Guidelines:

1.  **Prioritize C++ Best Practices:** Adhere to idiomatic C++ and maintain high code quality.
2.  **Maintain ImGui/ImPlot3D Style:** Ensure new code integrates seamlessly with the existing codebase's style and API patterns.
3.  **ImPlot Reference** Some of the feature requests may already be implemented in ImPlot. Before implementing a new feature, check the ImPlot documentation and codebase to see if it can be adapted for ImPlot3D.
4.  **Focus on 3D Plotting:** Solutions should be specific to 3D visualization within the ImPlot3D framework.
5.  **Performance & Real-time:** Solutions should consider performance for real-time 3D rendering.

## Development Flow & Standards:

* **Code Formatting:** Use `clang-format`. Ensure code is formatted correctly before proposing changes.
* **Documentation:** Document public APIs and complex logic.
* **Demos:** When a new feature is added, include a demo in `implot3d_demo.cpp` to showcase its usage.

## Contribution Specifics:

* **Bug Fixes:** Provide concise and effective fixes.
* **Feature Implementation:** Implement features aligned with the existing structure.
* **Commit Messages:** Follow Conventional Commits (`feat:`, `fix:`, `refactor:`, `docs:`, `style:`, `test:`, `chore:`, `merge:`).

## Build Example Code:

```
cd examples/
cmake -B build && cmake --build build
```

## File Structure Reference:
Below is a brief overview of the key files in the ImPlot3D repository:
* `implot3d.h`, `implot3d.cpp`: Core ImPlot3D API.
* `implot3d_internal.h`, `implot3d_items.cpp`: Internal implementation details.
* `implot3d_demo.cpp`: Comprehensive examples and demo.
* `implot3d_meshes.cpp`: Pre-loaded meshes support.

If you need to consult the ImGui/ImPlot codebase for reference, you need to first build the example code to have their files available in the `build/` directory.
You can find them in `example/build/_deps/implot-src/` and `example/build/_deps/imgui-src/`
