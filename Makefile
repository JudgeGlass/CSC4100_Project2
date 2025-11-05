CLEAN_SUBDIRS = src doc tests
PINTOS_BASE=~/pintos_build/src/
DOCKER_EXEC=docker exec -it pintos bash -c 
PINTOS=/home/PKUOS/toolchain/x86_64/bin/pintos

all::
	@echo "This makefile has only 'clean' and 'check' targets."

clean::
	for d in $(CLEAN_SUBDIRS); do $(MAKE) -C $$d $@; done

docker:
	docker run -it -d --rm --name pintos -p 1234:1234 -v $(shell pwd)/:/home/PKUOS/pintos_build pkuflyingpig/pintos bash
	$(DOCKER_EXEC) 'rm ~/toolchain/x86_64/bin/pintos'
	$(DOCKER_EXEC) 'ln -s ~/pintos_build/src/utils/pintos ~/toolchain/x86_64/bin/pintos'

docker-stop:
	docker container stop pintos

threads:
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)threads; make -j4'

userprog:
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)userprog; make -j4'       

vm:
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)vm; make -j4'       

filesys:
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)filesys; make -j4'

build-all: threads userprog vm filesys

run-threads: threads
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)threads/build; $(PINTOS) --'

run-userprog: userprog
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)userprog/build; $(PINTOS) --'

run-vm: vm
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)vm/build; $(PINTOS) --'

run-filesys: filesys
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)filesys/build; $(PINTOS) --'

run-test: threads
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)/threads/build; $(PINTOS) run alarm-multiple'

docker-clean:
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE); make clean'

distclean:: clean
	find . -name '*~' -exec rm '{}' \;

docker-debug-threads: threads
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)/threads/build; $(PINTOS) --gdb -- run $(TEST)'

docker-check:
	$(DOCKER_EXEC) 'cd $(PINTOS_BASE)/..; make check'

check::
	$(MAKE) -C tests $@
