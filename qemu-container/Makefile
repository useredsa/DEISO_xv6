all: Dockerfile
	docker images 2>&1 | grep -qe debian/deiso || docker build -t debian/deiso:r1 .

run-nox:
	docker run -it --rm \
		-h deiso \
		-v /tmp:/tmp/tmp:rw \
		-v `pwd`/..:/root/deiso --name deiso debian/deiso:r1

run:
	xhost +local:root
	docker run -it --rm \
		-h deiso \
		-e DISPLAY=${DISPLAY} \
		--security-opt seccomp=unconfined \
		--cap-add=SYS_PTRACE \
		-v /tmp/.X11-unix:/tmp/.X11-unix:rw \
		-v /tmp:/tmp/tmp:rw \
		-v `pwd`/../..:/root/deiso --name deiso debian/deiso:r1

run-gdb:
	docker exec -it deiso bash

.PHONY: all run run-nox run-gdb
