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

struct Type {
	enum BaseType {
		VOID,
		INT,
		FUNCTION,
		ARRAY,
		CLASS,
	};

	BaseType type;
	
	String clazz_name;
	Type *element_type;

	Array<Type *> parameters;
	Type *return_type;

	Type() {}
};

struct Method {
	u16 access_flags;
	String name;
	Type *type;
	u16 attributes_count;
	Attribute *attributes;
};

struct Field {
	u16 access_flags;
	String name;
	Type *type;
	u16 attributes_count;
	Attribute *attributes;
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
extern Type *type_void;
extern Type *type_int;