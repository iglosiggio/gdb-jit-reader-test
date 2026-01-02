#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <gdb/jit-reader.h>

struct jit_reader_entry {
  void* self_addr;
  uint32_t i;
};

/* Parse the debug info off a block of memory, pointed to by MEMORY
   (already copied to GDB's address space) and MEMORY_SZ bytes long.
   The implementation has to use the functions in CB to actually emit
   the parsed data into GDB.  SELF is the same structure returned by
   gdb_init_reader.

   Return GDB_FAIL on failure and GDB_SUCCESS on success.  */
enum gdb_status jit_reader_read(
  struct gdb_reader_funcs *self,
  struct gdb_symbol_callbacks *cb,
  void *memory, long memory_sz
) {
  printf(
    "Called read(self=%p, symbol_callbacks=%p, memory=%p, size=%ld)\n",
    self, cb, memory, memory_sz
  );

  if (memory_sz != sizeof(struct jit_reader_entry)) {
    printf("Unexpected size!");
    return GDB_FAIL;
  }
  struct jit_reader_entry* entry = memory;
  printf("Read Loop[%d]\n", entry->i);

  const char* file_name = "file.test";
  char symbol[32];
  snprintf(symbol, 32, "test_symbol_%d", entry->i);

  struct gdb_object* object = cb->object_open(cb);
  struct gdb_symtab* symtab = cb->symtab_open(cb, object, file_name);
  struct gdb_block* block = cb->block_open(
    cb, symtab, NULL,
    (uintptr_t) entry->self_addr, (uintptr_t) entry->self_addr + 4,
    symbol
  );

  printf("object=%p, symtab=%p, block=%p\n", object, symtab, block);
  printf("Registered %s:%s\n", file_name, symbol);

  cb->symtab_close(cb, symtab);
  cb->object_close(cb, object);
  return GDB_SUCCESS;
}

/* Unwind the current frame, CB is the set of unwind callbacks that
   are to be used to do this.

   Return GDB_FAIL on failure and GDB_SUCCESS on success.  */
enum gdb_status jit_reader_unwind(
  struct gdb_reader_funcs *self,
  struct gdb_unwind_callbacks *cb
) {
  printf(
    "Called unwind(self=%p, unwind_callbacks=%p)\n",
    self, cb
  );
  return GDB_FAIL;
}

/* Return the frame ID corresponding to the current frame, using C to
   read the current register values.  See the comment on struct
   gdb_frame_id.  */
struct gdb_frame_id jit_reader_get_frame_id(
  struct gdb_reader_funcs *self,
  struct gdb_unwind_callbacks *cb
) {
  printf(
    "Called get_frame_id(self=%p, unwind_callbacks=%p)\n",
    self, cb
  );
  return (struct gdb_frame_id) {
    .code_address = 0,
    .stack_address = 0,
  };
}

/* Called when a reader is being unloaded.  This function should also
   free SELF, if required.  */
void jit_reader_destroy(struct gdb_reader_funcs *self) {
  printf("Called destroy(self=%p)\n", self);
  free(self);
}

GDB_DECLARE_GPL_COMPATIBLE_READER;
struct gdb_reader_funcs* gdb_init_reader(void) {
  printf("JIT Reader Starting!\n");

  struct gdb_reader_funcs* funcs = malloc(sizeof(*funcs));
  *funcs = (struct gdb_reader_funcs) {
    .reader_version = GDB_READER_INTERFACE_VERSION,

    .priv_data = funcs,

    .read = jit_reader_read,
    .unwind = jit_reader_unwind,
    .get_frame_id = jit_reader_get_frame_id,
    .destroy = jit_reader_destroy,
  };
}
