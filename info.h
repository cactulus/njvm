struct UTF8 {
	u16 length;
	u8 *bytes;
};

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
		UTF8 utf8;
	};
};

struct Attribute_Info {
	u16 attribute_name_index;
	u32 attribute_length;
	u8 *info;
};

struct Field_Info {
	u16 access_flags;
	u16 name_index;
	u16 descriptor_index;
	u16 attributes_count;
	Attribute_Info *attributes;
};

struct Method_Info {
	u16 access_flags;
	u16 name_index;
	u16 descriptor_index;
	u16 attributes_count;
	Attribute_Info *attributes;
};

struct Code_Info {
	u16 max_stack;
	u16 max_locals;
	u32 code_length;
	u8 *code;
	/* TODO: complete */
};

struct Class_File {
	u32 magic;
	u16 minor_version;
	u16 major_version;
	u16 constant_pool_count;
	CP_Info *constant_pool;
	u16 access_flags;
	u16 this_class;
	u16 super_class;
	u16 interfaces_count;
	u16 *interfaces;
	u16 fields_count;
	Field_Info *fields;
	u16 methods_count;
	Method_Info *methods;
	u16 attributes_count;
	Attribute_Info *attributes;
};
