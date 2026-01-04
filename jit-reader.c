#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdb/jit-reader.h>

const char* specialCases[] = {
  "%nullptr%",
  "%selector-wrong-format%",
  "%method-wrong-format%",
  "%non-oop-selector%",
  "%non-oop-method%",
  "%non-oop-association%",
  "%method-too-big%",
  "%selector-too-big%",
  "%method-unknown-last-literal",
  "%gdb-read-failed%"
};
#define SPECIAL_CASE_CASE_NULLPTR                0
#define SPECIAL_CASE_SELECTOR_WRONG_FORMAT       1
#define SPECIAL_CASE_METHOD_WRONG_FORMAT         2
#define SPECIAL_CASE_NON_OOP_SELECTOR            3
#define SPECIAL_CASE_NON_OOP_METHOD              4
#define SPECIAL_CASE_NON_OOP_ASSOCIATION         5
#define SPECIAL_CASE_METHOD_TOO_BIG              6
#define SPECIAL_CASE_SELECTOR_TOO_BIG            7
#define SPECIAL_CASE_METHOD_UNKNOWN_LAST_LITERAL 8
#define SPECIAL_CASE_GDB_READ_FAILED             9

typedef struct {
  unsigned class     : 22;
  unsigned dontcare0 :  2;
  unsigned format    :  5;
  unsigned dontcare1 :  3;
  unsigned hash      : 22;
  unsigned dontcare2 :  2;
  unsigned size      :  8;
} oop_header_t;

uintptr_t get_class_name_selector_for_method_oop(
  struct gdb_symbol_callbacks *cb,
  uintptr_t methodObject,
  uintptr_t methodHeader
) {
  enum gdb_status read_status;
	oop_header_t header;
  size_t num_literals = (methodHeader >> 3) & 0x7FFF;
  uintptr_t classAssociation;
  uintptr_t selectorObject;

  if (methodObject == 0) {
    return SPECIAL_CASE_CASE_NULLPTR;
  }

  if ((methodObject & 0b111) != 0) {
    return SPECIAL_CASE_NON_OOP_METHOD;
  }

  read_status = cb->target_read(methodObject, &header, sizeof(header));
  if (read_status == GDB_FAIL) {
    return SPECIAL_CASE_GDB_READ_FAILED;
  }

  if (header.size == 255) {
    return SPECIAL_CASE_METHOD_TOO_BIG;
  }

  if (header.format < 24 || 31 < header.format) {
    return SPECIAL_CASE_METHOD_WRONG_FORMAT;
  }

  //printf("Object(%#.8llx) = (class=%.6x, format=%.2x, hash=%.6x, size=%.2x)\n", methodObject, header.class, header.format, header.hash, header.size);

  read_status = cb->target_read(methodObject + (num_literals + 1) * sizeof(uintptr_t), &classAssociation, sizeof(classAssociation));
  if (read_status == GDB_FAIL) {
    return SPECIAL_CASE_GDB_READ_FAILED;
  }

  if (classAssociation == 0) {
    return SPECIAL_CASE_CASE_NULLPTR;
  }

  if ((classAssociation & 0b111) != 0) {
    return SPECIAL_CASE_NON_OOP_ASSOCIATION;
  }

  read_status = cb->target_read(classAssociation, &header, sizeof(header));
  if (read_status == GDB_FAIL) {
    return SPECIAL_CASE_GDB_READ_FAILED;
  }

  // FIXME: Add support to CompiledBlocks
  if (header.format == 1 || header.size == 2) {
    read_status = cb->target_read(classAssociation + sizeof(uintptr_t), &selectorObject, sizeof(uintptr_t));
    if (read_status == GDB_FAIL) {
      return SPECIAL_CASE_GDB_READ_FAILED;
    }
    return selectorObject;
  }

  //printf("  Assoc(%#.8llx) = (class=%.6x, format=%.2x, hash=%.6x, size=%.2x)\n", classAssociation, header.class, header.format, header.hash, header.size);

  return SPECIAL_CASE_METHOD_UNKNOWN_LAST_LITERAL;
}

size_t copy_selector_text_to(
  struct gdb_symbol_callbacks *cb,
  uintptr_t selectorObject,
  char* buf, size_t bufsiz
) {
  enum gdb_status read_status;
	oop_header_t header;
  size_t text_size;

  if (selectorObject < sizeof(specialCases)) {
    size_t i = 0;
    const char* s = specialCases[i];
    while (s[i]) {
      buf[i] = s[i];
      i++;
    }
    return i;
  }

  if ((selectorObject & 0b111) != 0) {
    return copy_selector_text_to(cb, SPECIAL_CASE_NON_OOP_SELECTOR, buf, bufsiz);
  }

  read_status = cb->target_read(selectorObject, &header, sizeof(header));
  if (read_status == GDB_FAIL) {
    return copy_selector_text_to(cb, SPECIAL_CASE_GDB_READ_FAILED, buf, bufsiz);
  }

  if (header.size == 255) {
    return copy_selector_text_to(cb, SPECIAL_CASE_SELECTOR_TOO_BIG, buf, bufsiz);
  }

  if (header.format < 16 || 23 < header.format) {
    return copy_selector_text_to(cb, SPECIAL_CASE_SELECTOR_WRONG_FORMAT, buf, bufsiz);
  }

  text_size = header.size * sizeof(uintptr_t) - (header.format - 16);
  if (bufsiz < text_size) text_size = bufsiz;

  read_status = cb->target_read(selectorObject + sizeof(uintptr_t), buf, text_size);
  if (read_status == GDB_FAIL) {
    return copy_selector_text_to(cb, SPECIAL_CASE_GDB_READ_FAILED, buf, bufsiz);
  }

  return text_size;
}

#define CMFree 1
#define CMMethod 2
#define CMPolymorphicIC 3
#define CMMegamorphicIC 4
typedef struct {
	uintptr_t objectHeader;
	unsigned cmNumArgs : 8;
	unsigned cmType : 3;
	unsigned cmRefersToYoung : 1;
	unsigned cpicHasMNUCaseOrCMIsFullBlock : 1;
	unsigned cmUsageCount : 3;
	unsigned cmUsesPenultimateLit : 1;
	unsigned cbUsesInstVars : 1;
	unsigned cmUnusedFlags : 2;
	unsigned stackCheckOffset : 12;
	unsigned short blockSize;
	unsigned short picUsage;
	uintptr_t methodObject;
	uintptr_t methodHeader;
	uintptr_t selector;
 } CogMethod;

struct pharo_jit_entry {
  char magic[8];
  uintptr_t code_zone_start;
  uintptr_t code_zone_end;
  uintptr_t trampoline_return_to_interpreter;
  uintptr_t trampoline_base_frame_return;
  uintptr_t trampoline_cannot_return;
};

size_t register_method(
  struct gdb_symbol_callbacks *cb,
  struct gdb_symtab* symtab,
  struct gdb_block* cog_method_zone_block,
  uintptr_t method_addr
) {
  char symbol[128] = "%unknown-class>>%unknown-selector";
  enum gdb_status read_status;

  CogMethod header;
  read_status = cb->target_read(method_addr, &header, sizeof(header));
  if (read_status == GDB_FAIL) {
    return 0;
  }
  size_t size = (header.blockSize + 0b111) & ~0b111;

  if (header.cmType == CMFree) {
    return size;
  } else if (header.cmType == CMPolymorphicIC) {
    return size;
  } else if (header.cmType == CMMegamorphicIC) {
    return size;
  } else if (header.cmType == CMMethod) {
    size_t off = 0;
    uintptr_t class_name_selector = get_class_name_selector_for_method_oop(cb, header.methodObject, header.methodHeader);
    off += copy_selector_text_to(cb, class_name_selector, symbol + off, sizeof(symbol) - off - 1);
    if (off < sizeof(symbol) - 1) symbol[off++] = '>';
    if (off < sizeof(symbol) - 1) symbol[off++] = '>';
    if (off < sizeof(symbol) - 1) symbol[off++] = '#';
    off += copy_selector_text_to(cb, header.selector, symbol + off, sizeof(symbol) - off - 1);
    symbol[off++] = 0;
  } else {
    printf("Unrecognized CMType(%d) at %#.8x\n", header.cmType, method_addr);
    return 0;
  }

  struct gdb_block* block = cb->block_open(
    cb, symtab, cog_method_zone_block,
    method_addr, method_addr + size,
    symbol
  );

  return size;
}

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
  printf(".");
  const char* file_name = "PharoJIT";
  struct pharo_jit_entry* entry = memory;
  if (memory_sz != sizeof(struct pharo_jit_entry)) {
    printf("Unexpected size!\n");
    return GDB_FAIL;
  }
  if (strncmp("PHAROOOP", entry->magic, 8) != 0) {
    printf("Unexpected magic value!\n");
    return GDB_FAIL;
  }

  struct pharo_jit_entry* saved_entry = self->priv_data;
  *saved_entry = *entry;

  struct gdb_object* object = cb->object_open(cb);
  struct gdb_symtab* symtab = cb->symtab_open(cb, object, file_name);
  struct gdb_block* block = cb->block_open(
    cb, symtab, NULL,
    entry->code_zone_start, entry->code_zone_end,
    "CogMethodZone"
  );

  uintptr_t method = entry->code_zone_start;
  while (method < entry->code_zone_end) {
    uintptr_t method_size = register_method(cb, symtab, block, method);
    if (method_size == 0) break;
    method += method_size;
  }

  cb->symtab_close(cb, symtab);
  cb->object_close(cb, object);
  return GDB_SUCCESS;
}

constexpr size_t AMD64_Rbp = 6;
constexpr size_t AMD64_Rsp = 7;
constexpr size_t AMD64_Rip = 16;

/* Unwind the current frame, CB is the set of unwind callbacks that
   are to be used to do this.

   Return GDB_FAIL on failure and GDB_SUCCESS on success.  */
enum gdb_status jit_reader_unwind(
  struct gdb_reader_funcs *self,
  struct gdb_unwind_callbacks *cb
) {
  enum gdb_status read_status;
  struct pharo_jit_entry* entry = self->priv_data;

  struct gdb_reg_value* ip_reg = cb->reg_get(cb, AMD64_Rip);
  uintptr_t ip;
  char old_ip_buf[sizeof(uintptr_t)];

  struct gdb_reg_value* bp_reg = cb->reg_get(cb, AMD64_Rbp);
  uintptr_t bp;
  char old_bp_buf[sizeof(uintptr_t)];


  ip_reg = cb->reg_get(cb, AMD64_Rip);
  bp_reg = cb->reg_get(cb, AMD64_Rbp);
  if (bp_reg->size != sizeof(uintptr_t)) goto fail;
  if (ip_reg->size != sizeof(uintptr_t)) goto fail;

  bp = 0;
  for (size_t i = 0; i < sizeof(uintptr_t); i++) {
    bp |= ((uintptr_t) bp_reg->value[i]) << (i * 8 /* Assuming 8bit bytes, duh */);
  }

  ip = 0;
  for (size_t i = 0; i < sizeof(uintptr_t); i++) {
    ip |= ((uintptr_t) ip_reg->value[i]) << (i * 8 /* Assuming 8bit bytes, duh */);
  }
  if (entry->code_zone_start <= ip && ip < entry->code_zone_end) goto jitted_method_unwind;
  if (ip == entry->trampoline_return_to_interpreter) goto interpreted_method_unwind;
  if (ip == entry->trampoline_base_frame_return) goto stack_switch_unwind;
  if (ip == entry->trampoline_cannot_return) goto backtrace_end_unwind;
fail:
stack_switch_unwind:
  ip_reg->free(ip_reg);
  bp_reg->free(bp_reg);
  return GDB_FAIL;

jitted_method_unwind:
  read_status = cb->target_read(bp, old_bp_buf, sizeof(old_bp_buf));
  if (read_status != GDB_SUCCESS) goto fail;
  read_status = cb->target_read(bp + sizeof(uintptr_t), old_ip_buf, sizeof(old_ip_buf));
  if (read_status != GDB_SUCCESS) goto fail;

  for (size_t i = 0; i < sizeof(uintptr_t); i++) {
    bp_reg->value[i] = old_bp_buf[i];
    ip_reg->value[i] = old_ip_buf[i];
  }

  cb->reg_set(cb, AMD64_Rbp, bp_reg);
  cb->reg_set(cb, AMD64_Rip, ip_reg);
  return GDB_SUCCESS;

interpreted_method_unwind:
  read_status = cb->target_read(bp, old_bp_buf, sizeof(old_bp_buf));
  if (read_status != GDB_SUCCESS) goto fail;

  for (size_t i = 0; i < sizeof(uintptr_t); i++) {
    bp_reg->value[i] = old_bp_buf[i];
  }

  cb->reg_set(cb, AMD64_Rbp, bp_reg);
  cb->reg_set(cb, AMD64_Rip, ip_reg);
  return GDB_SUCCESS;

backtrace_end_unwind:
  for (size_t i = 0; i < sizeof(uintptr_t); i++) {
    bp_reg->value[i] = ip_reg->value[i] = 0;
  }

  cb->reg_set(cb, AMD64_Rbp, bp_reg);
  cb->reg_set(cb, AMD64_Rip, ip_reg);
  return GDB_SUCCESS;
}

/* Return the frame ID corresponding to the current frame, using C to
   read the current register values.  See the comment on struct
   gdb_frame_id.  */
struct gdb_frame_id jit_reader_get_frame_id(
  struct gdb_reader_funcs *self,
  struct gdb_unwind_callbacks *cb
) {
  struct gdb_reg_value* bp_reg = cb->reg_get(cb, AMD64_Rbp);
  struct gdb_reg_value* ip_reg = cb->reg_get(cb, AMD64_Rip);
  uintptr_t ip;
  uintptr_t bp;
  bp = 0;
  ip = 0;
  for (size_t i = 0; i < sizeof(uintptr_t); i++) {
    bp |= ((uintptr_t) bp_reg->value[i]) << (i * 8 /* Assuming 8bit bytes, duh */);
    ip |= ((uintptr_t) ip_reg->value[i]) << (i * 8 /* Assuming 8bit bytes, duh */);
  }
  bp_reg->free(bp_reg);
  ip_reg->free(ip_reg);

  return (struct gdb_frame_id) {
    .code_address = ip,
    .stack_address = bp,
  };
}

/* Called when a reader is being unloaded.  This function should also
   free SELF, if required.  */
void jit_reader_destroy(struct gdb_reader_funcs *self) {
  free(self->priv_data);
  free(self);
}

GDB_DECLARE_GPL_COMPATIBLE_READER;
struct gdb_reader_funcs* gdb_init_reader(void) {
  printf("JIT Reader Starting!\n");

  struct gdb_reader_funcs* funcs = malloc(sizeof(*funcs));
  struct pharo_jit_entry* jit_entry = malloc(sizeof(*jit_entry));
  *jit_entry = (struct pharo_jit_entry) {
    .code_zone_start = 0,
    .code_zone_end = 0
  };

  *funcs = (struct gdb_reader_funcs) {
    .reader_version = GDB_READER_INTERFACE_VERSION,

    .priv_data = jit_entry,

    .read = jit_reader_read,
    .unwind = jit_reader_unwind,
    .get_frame_id = jit_reader_get_frame_id,
    .destroy = jit_reader_destroy,
  };
}
