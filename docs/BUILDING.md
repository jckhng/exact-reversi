# Building Kindle Iagno

## Requirements

- Docker.
- ARM binfmt support if your Docker setup does not already run ARM containers.

Install ARM binfmt support on Linux with:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm
```

## Build And Package

```bash
./docker_rebuild.sh
```

The persistent builder is:

```text
image:     kindle-iagno-armhf-build:bullseye
container: kindle-iagno-armhf-builder
```

Build outputs:

```text
kindle-iagno
smoke-test
dist/kindle-iagno-extension.zip
```

## Build Without Packaging

```bash
KINDLE_IAGNO_PACKAGE=0 ./docker_rebuild.sh
```

## Builder Shell

```bash
./docker_shell.sh
```

Inside the container:

```bash
make clean
make kindle-iagno smoke-test
./smoke-test
```

If you move the repository, recreate the persistent container:

```bash
docker rm -f kindle-iagno-armhf-builder
./docker_rebuild.sh
```
