struct CP_Info {
	u8 tag;

	union {
    /* String */
    u16 string_index;

    /* Class & NameAndType */
    struct {
      u16 name_index;
      u16 descriptor_index;
    };

    /* Methodref, Fieldref, InterfaceMethodref */
    struct {
      u16 class_index;
      u16 name_and_type_index;
    };

    /* Utf-8 */
    String utf8;
    u64 long_int;
	};

	CP_Info() {}
};

struct Attribute {
	String name;
	u32 attribute_length;
	u8 *info;
};

struct Code {
	u16 max_stack;
	u16 max_locals;
	u32 code_length;
	u8 *code;
	/* TODO: complete */
};

struct NType {
	enum BaseType {
		VOID,
    BOOL,
    BYTE,
    SHORT,
		INT,
    LONG,
		FUNCTION,
		ARRAY,
		CLASS,
	};

	BaseType type;
	
  String clazz_name;
  NType *element_type;

	Array<NType *> parameters;
  NType *return_type;

  NType() {}
};

struct Class;
struct Method {
  Class *clazz;
	u16 access_flags;
	String name;
  NType *type;
	u16 attributes_count;
	Attribute *attributes;

  Code code = {0};

  /* remove later? */
  llvm::Function *llvm_ref;
};

struct Field {
	u16 access_flags;
	String name;
  NType *type;
	u16 attributes_count;
	Attribute *attributes;

  /* remove later? */
  llvm::GlobalVariable *llvm_ref;
};

struct Class {
	u16 access_flags;
	String name;
	String super_name;
	u16 constant_pool_count;
	CP_Info *constant_pool;
	u16 methods_count;
	Method *methods;
	u16 fields_count;
	Field *fields;
};

/* TODO: cleanup. Don't really want them to stay globally for ever */
extern NType *type_void;
extern NType *type_bool;
extern NType *type_byte;
extern NType *type_short;
extern NType *type_int;
extern NType *type_long;