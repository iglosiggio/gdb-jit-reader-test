PHARO_VM_BUILD=$$(pwd)/../pharo-vm-build
PHARO_IMAGE=$$(pwd)/../pharo12-image/Pharo12.0-SNAPSHOT-64bit-2047df47d7.image
PHARO_ARGS=eval '[ OrderedCollection new add: 1; add: 2; add: 3. ] repeat.'
#PHARO_ARGS=--interactive

jit-reader.so: jit-reader.c
	$(CC) -shared -fPIC $< -o $@

gdb: jit-reader.so
	LD_LIBRARY_PATH="/lib:/usr/lib:$(PHARO_VM_BUILD)/build/dist/lib:" gdb -ex "jit-reader-load $$(pwd)/jit-reader.so" -args $(PHARO_VM_BUILD)/build/dist/lib/pharo $(PHARO_IMAGE) $(PHARO_ARGS)

clean:
	rm -f jit-reader.so

.PHONY: gdb clean
