# STL_App
Practicing Standard Library based on "The C++ Standard Library, 2nd"

## Contents

|Dir|Releated Libs|
|:---:|:---:|
|ThreadLibrary| `<atomic>, <condition_variable>, <coroutine>,  <mutex>, <thread>, <future>` |

## Using CMake

From current directory:

```bash
[CC=clang] [CXX=clang++] cmake --build build --target install -j 16
# or: "cmake --install build", require CMake > 3.15
# or from ./build: "make install" / "cmake --build . --target install" /
#                   "cmake --install ." (require > 3.15)
```

To debug CMake: `cmake ... --trace` or `cmake ... --trace-source="filename"`.