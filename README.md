# fetaRecorder
* Screen recording app that records last few seconds that happened.
* Records last 10 sec with 60 fps after button press and saves it in \\Pictures\\Videos and as Testt.mp4. Can be changed inside the code.
* Keep in mind code is currently under production so it is not in a state that it should be but works at least partly the way its intended.

## Features

* Feature 1: Records 10 seconds what happened on screen recently

## Requirements & Prerequisites

* **Qt Version:** Qt 6.8
* **Build System:** CMake 3.16+ (or qmake)
* **Compiler:** C++17 compatible compiler (MSVC 2019+ (Recomended as code uses atlbase which is included), GCC 10+, or Clang)

## Planned features
* Fix saturation problem with video
* Configs to change fps, recording length, key binds, file name and saving location.
* Make window functional and not just barebones with wrong text
* Clean up code and possible documentation as currently code is held together with duct tape.

## License & Attributions

This project is licensed under the [MIT License](LICENSE).

### Third-Party Software
This project uses the **Qt Framework** under the terms of the **GNU Lesser General Public License (LGPL v3)**. 

* Qt is a registered trademark of The Qt Company Ltd.
* You can find the full source code for Qt at [https://www.qt.io](https://www.qt.io).
* Qt's LGPLv3 license details can be viewed at [https://www.gnu.org/licenses/lgpl-3.0.html](https://www.gnu.org/licenses/lgpl-3.0.html).


## Building and Running

### Using Qt Creator (Recommended)
1. Open Qt Creator.
2. Select **File > Open File or Project...** and choose `CMakeLists.txt` (or `.pro`).
3. Select your configured Desktop Kit.
4. Click **Build & Run** (`Ctrl+R` / `Cmd+R`).

### Using Command Line (CMake)
```bash
# Clone the repository
git clone [https://github.com/your-username/your-repo.name.git](https://github.com/your-username/your-repo.name.git)
cd your-repo-name

# Configure and build
mkdir build && cd build
cmake ..
cmake --build .
