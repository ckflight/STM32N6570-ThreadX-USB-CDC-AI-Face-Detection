# STM32N6 AI – Model Zoo Deployment

## Environment

Required tools:

* STM32CubeIDE
* STM32CubeProgrammer
* X-CUBE-AI / STEdgeAI Core
* Python 3.12
* TensorFlow 2.18
* STM32AI Model Zoo
* STM32AI Model Zoo Services
* Git LFS

Repositories:

```text
~/stm32ai-modelzoo
~/stm32ai-modelzoo-services
```

## 1. Download Model Zoo Models

Model files are managed with Git LFS.

```bash
cd ~/stm32ai-modelzoo
git lfs pull
```

Selected model:

```text
Face Detection
└── BlazeFace 128 INT8
    └── blazeface_front_128_int8.tflite
```

## 2. Initialize STM32N6 Application

The STM32N6 application is a Git submodule and must be downloaded separately.

```bash
cd ~/stm32ai-modelzoo-services
git submodule update --init application_code/face_detection/STM32N6
```

Application project:

```text
application_code/face_detection/STM32N6/
```

Model Zoo Services does not generate the complete firmware from scratch. It updates this reference STM32N6 project with model-specific generated files.

## 3. Create Deployment Configuration

Copy the provided example configuration:

```bash
cd ~/stm32ai-modelzoo-services/face_detection
cp config_file_examples/deployment_n6_blazeface_config.yaml user_config.yaml
```

`stm32ai_main.py` uses `user_config.yaml` as its active configuration.

Set the local tool paths:

```yaml
tools:
   stedgeai:
      optimization: balanced
      on_cloud: False
      path_to_stedgeai: /home/ck/STEdgeAI/4.0/Utilities/linux/stedgeai

   path_to_cubeIDE: /opt/st/stm32cubeide_2.2.0/stm32cubeide
```

Deployment target:

```yaml
deployment:
  c_project_path: ../application_code/face_detection/STM32N6/
  IDE: GCC
  hardware_setup:
    serie: STM32N6
    board: STM32N6570-DK
```

## 4. Generate / Deploy

Run:

```bash
cd ~/stm32ai-modelzoo-services/face_detection
python3.12 stm32ai_main.py
```

Flow:

```text
Pretrained Model
      ↓
Model Zoo Services
      ↓
STEdgeAI Core
      ↓
Generate Neural-ART / NPU files
      ↓
Update STM32N6 C Project
      ↓
CubeIDE Build
      ↓
STM32N6570-DK
```

The board is required for the complete deployment process. Model generation and project update occur before the board programming stage.

## 5. Generated STM32 Project

Final generated/reference project:

```text
~/stm32ai-modelzoo-services/application_code/face_detection/STM32N6/
```

The generated project contains the model-specific NN integration required by STM32N6:

```text
STAI runtime / generated network
Post-processing
Neural-ART / NPU configuration
Camera pipeline
External PSRAM / NOR support
Model-specific headers and binaries
```

The application initializes the NPU and external memories, obtains the generated network input/output buffers and connects the camera NN pipeline directly to the network input.

Typical runtime flow:

```text
Camera
  ↓
DCMIPP / Camera Pipeline
  ↓
nn_in
  ↓
stai_network_run()
  ↓
nn_out[]
  ↓
Face Detection Post-Process
  ↓
Application
```

## 6. Integrate into Our Project

The generated Model Zoo project is used as the **reference AI implementation**.

Instead of developing the final application directly inside the Model Zoo repository, copy the required model and AI integration files from:

```text
~/stm32ai-modelzoo-services/application_code/face_detection/STM32N6/
```

into our own STM32N6570-DK project.

Main pieces to integrate:

```text
Generated STAI / network files
Neural-ART / NPU support
Model-specific headers and binaries
Post-processing
Camera pipeline
Required BSP / memory configuration
```

Then integrate the NN flow into the existing application:

```text
Existing Application
       +
Camera Pipeline
       +
Generated AI Model
       +
NPU Initialization
       +
Post-Processing
       ↓
Final Project
```

This keeps the Model Zoo project as a reproducible reference while the actual application remains independent and under our own project structure.
