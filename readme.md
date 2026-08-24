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

Model Zoo Services updates this project with model-specific generated files.

## 3. Create Deployment Configuration

Copy the provided example configuration:

```bash
cd ~/stm32ai-modelzoo-services/face_detection
cp config_file_examples/deployment_n6_blazeface_config.yaml user_config.yaml
```

`stm32ai_main.py` uses `user_config.yaml` as its active configuration.

Set the local tool paths in `user_config.yaml`:

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

## 4. Deploy

Connect the STM32N6570-DK and run:

```bash
cd ~/stm32ai-modelzoo-services/face_detection
python3.12 stm32ai_main.py
```

Deployment flow:

```text
Pretrained Model
      ↓
Model Zoo Services
      ↓
STEdgeAI Core
      ↓
Generate N6 / Neural-ART files
      ↓
Update STM32N6 C Project
      ↓
CubeIDE Build
      ↓
STM32N6570-DK
```

Generated content includes the network C files, model-specific headers and Neural-ART/NPU binary data.

Final STM32 firmware project:

```text
~/stm32ai-modelzoo-services/application_code/face_detection/STM32N6/
```
