INSTALL_PATH=~cs537-1/tests/p4/p4-test

runtests:
	echo "#!/bin/bash" > $@
	echo "base=$(INSTALL_PATH)" >> $@
	echo "python \$$base/project5a.py --test-path \$$base \$$@ |"\
		"tee -i runtests.log" >> $@
	echo "exit \$$?" >> $@
	chmod a+x runtests

install: runtests
	mkdir -p $(INSTALL_PATH)
	cp -r * $(INSTALL_PATH)
	afs_rseta $(INSTALL_PATH) system:anyuser read

check:
	pychecker --only *.py

clean:
	rm -f runtests
	rm -f *.pyc

libpdrop.so: pdrop.c
	gcc -shared -ldl -fPIC $< -o $@
