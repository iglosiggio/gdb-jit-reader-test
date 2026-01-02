#include <stdint.h>
#include <stdio.h>

// https://sourceware.org/gdb/current/onlinedocs/gdb.html/Declarations.html#Declarations
typedef enum
{
  JIT_NOACTION = 0,
  JIT_REGISTER_FN,
  JIT_UNREGISTER_FN
} jit_actions_t;

struct jit_code_entry
{
  struct jit_code_entry *next_entry;
  struct jit_code_entry *prev_entry;
  const char *symfile_addr;
  uint64_t symfile_size;
};

struct jit_descriptor
{
  uint32_t version;
  /* This type should be jit_actions_t, but we use uint32_t
     to be explicit about the bitwidth.  */
  uint32_t action_flag;
  struct jit_code_entry *relevant_entry;
  struct jit_code_entry *first_entry;
};

/* GDB puts a breakpoint in this function.  */
void __attribute__((noinline)) __jit_debug_register_code() { };

/* Make sure to specify the version statically, because the
   debugger may check the version before we can set it.  */
struct jit_descriptor __jit_debug_descriptor = { 1, 0, 0, 0 };

struct jit_reader_entry {
  void* self_addr;
  uint32_t i;
};

int main(int argc, const char* argv[]) {
  uint32_t crash_loop;
  printf("I want to crash at loop: ");
  scanf("%d", &crash_loop);
  while (!feof(stdin) && getchar() != '\n');

  struct jit_reader_entry reader_entry = {
    .self_addr = &reader_entry,
    .i = 0,
  };
  struct jit_code_entry entry = {
    .next_entry = NULL,
    .prev_entry = NULL,
    .symfile_addr = (void*) &reader_entry,
    .symfile_size = sizeof(reader_entry),
  };
  __jit_debug_descriptor.relevant_entry = &entry;
  __jit_debug_descriptor.first_entry = &entry;

  for (;; reader_entry.i++) {
    printf("Loop[%d]\n", reader_entry.i);
    __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
    __jit_debug_register_code();

    if (reader_entry.i == crash_loop) {
      ((void(*)(void))&reader_entry)();
    }

    while (!feof(stdin) && getchar() != '\n');
    __jit_debug_descriptor.action_flag = JIT_UNREGISTER_FN;
    __jit_debug_register_code();
  }
}
