# Smart Privacy Shield

## About
The Smart Privacy Shield (SPS) is a proof of concept. It is a tool that allows users to capture their screen or certain application windows of it and apply processing steps to it. The goal of the application is to increase the privacy and prevent data leaks when sharing a screen in an online conference.

The SPS tool can be seen as an intermediary application that is in charge of protecting a captured screen feed before it gets shared in an online meeting. The tool itself handles all of the capturing and eventually generates a new window with the processed video feed. This output window is then intended to be shared in the online meeting software.

## Architecture
The SPS tool is built upon a video processing pipeline that is powered by the GStreamer library. The GUI is created using the GTK library.

## Plugins
The SPS tool is based on a modular plugin architecture. A SPS plugin can provide one or several (custom) GStreamer plugins. The GStreamer plugins are the parts that are incorporated into the processing pipeline and perform the actual work on the video. In addition SPS plugins also provide GUI widgets to configure the (custom) GStreamer plugin it provides.

An example on how to implement a SPS plugin can be found looking at the SPS base plugin in the `/sps-plugin/base` directory. Examples on how to implement GStreamer plugins can be found in the GStreamer documentation.

## Building/Installation
The tool can be built from source using the cmake tool.

### MacOS

1. Install the dependencies
   1. `brew install cmake`
   2. `brew install gstreamer gst-plugins-base gst-plugins-good`
   3. `brew install gtk+3`
   4. `brew install glib`
   5. `brew install opencv`
   6. `brew install onnxruntime`
2. From the root project folder, create build folder `mkdir build && cd build`
3. Create the cmake project `cmake ..` (NOTE: Use `-DCMAKE_BUILD_TYPE=Release` for release builds)
4. Build the project `make`
5. In order to build a bundled application, run `cpack`
6. The resulting application bundle can be moved to the application folder and launched from there

### Windows

Due to the lack of some required binaries, the installation on windows is somewhat more involved

1. Install the cerbero build tool for GStreamer
   1. Install python
   2. Install MinGW with MSYS (NOTE: Not MSYS2, c.f. https://gstreamer.freedesktop.org/documentation/installing/building-from-source-using-cerbero.html?gi-language=c#install-msysmingw)
   3. Install MSYS2 as well (c.f. https://www.msys2.org)
   4. Install Visual Studio 2015 or newer
   5. Install CMake
2. Build custom GStreamer installer
   1. Open MinGW shell (typically by executing `C:\MinGW\msys\1.0\msys.bat`)
   2. Change to C directory `cd /c/`
   3. Clone Cerbero repository `git clone https://gitlab.freedesktop.org/gstreamer/cerbero && cd cerbero`
   4. Checkout Merge Request enabling gtk sink
      1. `git fetch "https://gitlab.freedesktop.org/nirbheek/cerbero.git" 'gtksink'`
      2. `git checkout -b 'nirbheek/cerbero-gtksink' FETCH_HEAD`
      3. `git rebase main`
      4. Resolve conflicts `git checkout main -- recipes/cairo.recipe && git add recipes/cairo.recipe`
      5. In case there are any other conflicts, resolve them manually
   5. Bootstrap cerbero `./cerbero-uninstalled bootstrap`
   6. Build the installer `./cerbero-uninstalled -c config/win64.cbc -v visualstudio package gstreamer-1.0`
   7. Once finished, the process will print the path of the created installer. Run this installer. (NOTE: Make sure to choose the "complete" installation option)
3. Build GTK3 from source
   1. Open a `x64 Native Tools Command Prompt for VS 20XX` shell
   2. Change to C directory `cd C:/`
   3. Create a build directory `mkdir gtk-build && cd gtk-build && mkdir github && cd github`
   4. Clone repo `git clone https://github.com/wingtk/gvsbuild.git && cd gvsbuild`
   5. Build GTK3 `python .\build.py build -p=x64 --vs-ver=16 --msys-dir=C:\msys64 --gtk3-ver=3.24 gtk3`
   6. Copy your built binaries to the root folder `move C:/gtk-build/gtk/x64/release C:/gtk`
4. Install [CppWinRT](https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/)
   1. Download the raw nuget package using the URL `https://www.nuget.org/api/v2/package/Microsoft.Windows.CppWinRT/`
   2. Change the file ending to `.zip` and extract the archive
   3. Navigate to the `/bin` folder and copy the `cppwinrt.exe` file somewhere convenient
5. Install ONNXRuntime
   1. Download the raw nuget package using the URL `https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime/`
   2. Change the file ending to `.zip` and extract the archive
   3. Move the extracted folder somewhere convenient
6. Install OpenCV
   1. Go to the OpenCV [website](https://opencv.org/releases/)
   2. Download the prebuild binaries for Windows
   3. Run the .exe file and place the resulting `opencv` folder inside your `C:/Program Files (x86)` directory.
7. Build the actual SPS application
   1. Open a `x64 Native Tools Command Prompt for VS 20XX` shell
   2. Inside the root SPS project folder, create build folder `mkdir build && cd build`
   3. Create the cmake project (NOTE: **Use forward slashes!**, use `-DCMAKE_BUILD_TYPE=Release` for release builds)
   ```
   cmake ^
   -DGLIB_COMPILE_RESOURCES=<GTK_INSTALL_DIR>/bin/glib-compile-resources.exe ^
   -DGLIB_COMPILE_SCHEMAS=<GTK_INSTALL_DIR>/bin/glib-compile-schemas.exe ^
   -DCPPWINRT_PATH=<CPP_WINRT_EXE_LOCATION> ^
   -DONNXRUNTIME_PATH=<ONNXRUNTIME_FOLDER_PATH> ^
   ..
   ```
   4. Build the project `cmake --build .` (NOTE: Use `cmake --build . --config Release` for release builds)
   5. Run `cpack` to create an executable installer
   6. Run the installer to install the SPS tool

### Linux

1. Install the dependencies
   1. `apt-get install cmake gcc git wget tar`
   2. `apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-qt5 gstreamer1.0-pulseaudio`
   3. `apt-get install gtk+3`
   4. `apt-get install libopencv-dev`
   5. `apt-get install libonnx-dev`
2. Build OpenCV dependencies
   1. `mkdir -p /tmp/opencv && cd /tmp/opencv`
   2. `git clone --recursive https://github.com/opencv/opencv.git .`
   3. `mkdir build && cd build`
   4. `cmake -DBUILD_LIST=imgcodecs,highgui -DBUILD_opencv_world=ON -DCMAKE_BUILD_TYPE=Release ..`
   5. `make install`
3. Build ONNXRuntime dependecies
   1. `wget https://github.com/microsoft/onnxruntime/releases/download/v1.9.0/onnxruntime-linux-x64-1.9.0.tgz`
   2. `tar -zxvf onnxruntime-linux-x64-1.9.0.tgz`
   3. `mkdir -p /usr/local/include/onnxruntime/core/session`
   4. `cp onnxruntime-linux-x64-1.9.0/include/* /usr/local/include/onnxruntime/core/session`
   5. `cp onnxruntime-linux-x64-1.9.0/lib/* /usr/local/lib`
1. From the root project folder, create build folder `mkdir build && cd build`
2. Create the cmake project `cmake ..` (NOTE: Use `-DCMAKE_BUILD_TYPE=Release` for release builds)
3. Build the project `make`
4. In order to build a bundled application, run `cpack`
5. To install the packaged application run `dpkg -i <path_to_bundle>.deb`

## Usage

### Sources
When the application is started, you are presented with a basic user interface. In the center, all your selected capturing sources will be displayed. At the beginning, there is no capture source. To add a source, press the button the bottom left-hand corner.

A new dialog will show up. Select either a whole screen or a specific application from the dropdown and continue. The selected source will now be shown in the source list in the center of the window.

Once at least one source is added, you can start the processing by clicking the button in the bottom right-hand corner.

#### Source Configuration
To configure a source, click on the "edit" button in its row. To remove a source, click the "delete" button instead.

If you click the "edit" button, a new dialog will open. This dialog allows you to configure which processing should be applied to the selected source.

You can add preprocessor, detector, and/or postprocessor elements. To do this, press the "+" button next to the respective category. To remove an element again, select it from the list and click the "-" button. To configure an element, select it from the list. This will show the element's settings on the right side of the dialog.

### Preprocessors
Preprocessors either alter frames before they are further processed or attach additional metadata to them. Preprocessors could for example downscale the captured video to a lower resolution or convert the stream to black and white. 

#### Change Detector
Simple preprocessor that analyzes wether a frame has a certain amount of changed pixels compared to its preceding frame. If a threshold is exceeded, it will add metadata to the frame that states that the frame has changed. It also add information on the regions in the frame that have changed. This metadata is can be used by detector elements to prevent processing of unchanged fra

The threshold (in percent) can be set by the user.

### Detectors
Detector elements are responsible for generating detections of privacy-critical areas in a frame. How they come up with those detections depends entirely on the element itself and is not restricted in any way. Detectors could for example use manual definition of regions, exploit computer vision techniques or use system APIs to come up with the detections. Other methods could be possible too as long as they return the detections in the standardized format. Detectors usually display a list of detection labels they produce. This can be used as a reference when configuring postprocessor elements. The detection labels are always prefixed by the prefix specified in the element. For example, if a detector with prefix `example` produces the label `file`, the resulting label will be `example:file`. The prefix can be changed by the user.

#### Regions
This is a basic detector that allows the user to configure privacy-critical regions manually. The region can be specified by hand using the x and y coordinates as well as the width and height. One can also use the selection button at the bottom to bring up a dialog where one can select the region on the screen using drag and drop. Note, that this feature is only available once the pipeline was started.

#### ObjDetection
This detector uses an object detection model to identify regions in the video frames. To prime this detector, the detection model has to be selected. The model is typically packaged with the application.

On macOS, open the app bundle by right-clicking on it. Navigate to `/Resources/plugins/spsbase/share`. You will find the model with a `.onnx` file ending in there.
On Windows, navigate to the installation folder of the application (typically `C:/Program Files/smart-privacy-shield XXX`). Then, navigate to `plugins/spsbase/share`. You will find the model with a `.onnx` file ending in there.

Note that if no model is selected, this element will prevent the processing of the pipeline!

#### WinAnalyzer
The window analyzer uses the system's window manager to get information about the location of application windows and report them as detections. This detector should only be used when the source captures a full screen opposed to a single application window.

The user should select the screen corresponding to the source's captured screen from the drop-down menu.

### Postprocessors
Postprocessors are in charge of applying the actual video processing to hide privacy-critical regions from the final output. For example, they could blur selected detections so that they are not readable in the final output

#### Obstruct
This postprocessor allows the user to obfuscate selected detections in the final video output.

The user can select an obfuscation type (i.e. masking mode) from the dropdown menu. This determines how the detections are handled.

To select which detections should be processed, the user can enter a list of labels. Labels use prefix matching. This means that if the label `example` is entered, it will match all detections of type `example:XXXXXX`. This can be useful to select all detections of a specific detector. In the case of the window analyzer, it can also be used to select all windows of a certain application.

## Object detection
One of the core novelties of the SPS tool is the use object detection to detect privacy-critical areas. The object detection element uses the ONNXRuntime to perform inference on object detection models. The detection models have to be created using the YOLO v5 architecture.

### Training
In order to train a custom object detection model, one can follow the explanation on the YOLO v5 github page. It does not matter which type of network (i.e. YOLOv5s, YOLOv5m, etc.) is used. It also does not matter which image dimensions are used, as the detector adjust automatically.

To create a dataset, the Data Generator, created with this tool, can be used. It can also be found on github.

### Model export
Once the model is trained, it has to be converted into the ONNX format. This can also be done using the YOLO v5 tool suite. For a better user experience, one can add special metadata to the ONNX model containing the actual labels of the dataset. This can be done by adding a list of strings as metadata using the key "labels". The detection element will try to extract these labels. Instead of reporting the detected classes as numbers, it will then return the label at the corresponding index.
