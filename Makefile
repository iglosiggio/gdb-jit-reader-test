test-program: test-program.c

jit-reader.so: jit-reader.c
	$(CC) -shared -fPIC $< -o $@

gdb: test-program jit-reader.so
	gdb -ex "jit-reader-load $$(pwd)/jit-reader.so" test-program

clean:
	rm -f test-program jit-reader.so

.PHONY: gdb clean
