# Building Exact Reversi

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
image:     exact-reversi-armhf-build:bullseye
container: exact-reversi-armhf-builder
```

Build outputs:

```text
exact-reversi
smoke-test
dist/exact-reversi-extension.zip
```

## Build Without Packaging

```bash
EXACT_REVERSI_PACKAGE=0 ./docker_rebuild.sh
```

## Builder Shell

```bash
./docker_shell.sh
```

Inside the container:

```bash
make clean
make exact-reversi smoke-test
./smoke-test
```

If you move the repository, recreate the persistent container:

```bash
docker rm -f exact-reversi-armhf-builder
./docker_rebuild.sh
```
