#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

enum {
	CONSTANT_Utf8 = 1,
	CONSTANT_Integer = 3,
	CONSTANT_Float = 4,
	CONSTANT_Long = 5,
	CONSTANT_Double = 6,
	CONSTANT_Class = 7,
	CONSTANT_String = 8,
	CONSTANT_Fieldref = 9,
	CONSTANT_Methodref = 10,
	CONSTANT_InterfaceMethodref = 11,
	CONSTANT_NameAndType = 12,
	CONSTANT_MethodHandle = 15,
	CONSTANT_MethodType = 16,
	CONSTANT_Dynamic = 17
	CONSTANT_InvokeDynamic = 18
	CONSTANT_Module = 19
	CONSTANT_Package = 20
};

enum {
	ACC_PUBLIC = 0x0001,
	ACC_PRIVATE = 0x0002,
	ACC_PROTECTED = 0x0004,
	ACC_STATIC = 0x0008,
	ACC_FINAL = 0x0010,
	ACC_SYNCHRONIZED = 0x0020,
	ACC_SUPER = 0x0020,
	ACC_VOLATILE = 0x0040,
	ACC_BRIDGE = 0x0040,
	ACC_TRANSIENT = 0x0080,
	ACC_VARARGS = 0x0080,
	ACC_NATIVE = 0x1000,
	ACC_SYNTHETIC = 0x1000
	ACC_ANNOTATION = 0x2000,
	ACC_ENUM = 0x4000,
	ACC_ABSTRACT = 0x4000,
	ACC_STRICT = 0x8000,
	ACC_MODULE = 0x8000,
};

enum {
	OP_LDC = 0x12,
	OP_RETURN = 0xb1,
	OP_GETSTATIC = 0xb2,
	OP_INVOKEVIRTUAL = 0xb6,
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
		struct {
			u16 length;
			u8 *bytes;
		};
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

struct Reader {
	u8 *bytes;
	u32 length;
	u32 pos;

	Reader(u8 *bytes, u32 length) {
		this->bytes = bytes;
		this->length = length;
		this->pos = 0;
	}

	Reader(const char *file_name) {
		FILE *f = fopen(file_name, "rb");

		fseek(f, 0, SEEK_END);
		length = ftell(f);
		fseek(f, 0, SEEK_SET);

		bytes = (u8 *) malloc(length);
		fread(bytes, sizeof(u8), length, f);

		fclose(f);

		pos = 0;
	}

	u8 read_u8() {
		assert(pos < length);
		return bytes[pos++];
	}

	u16 read_u16() {
		assert(pos < length - 1);

		u16 *p = (u16 *) &bytes[pos];
		pos += 2;
		u16 v = *p;
		u16 c = 0;
		c |= ((0xff & v) << 8);
		c |= (((0xff << 8) & v) >> 8);
		return c;
	}

	u32 read_u32() {
		assert(pos < length - 3);
		u32 *p = (u32 *) &bytes[pos];
		pos += 4;
		u32 v = *p;
		u32 c = 0;
		c |= ((0xff & v) << 24);
		c |= (((0xff << 8) & v) <<8);
		c |= (((0xff << 16) & v) >> 8);
		c |= (((0xff << 24) & v) >> 24);

		return c;
	}
};

CP_Info read_cp_info(Reader *r) {
	CP_Info info;

	info.tag = r->read_u8();
	
	switch (info.tag) {
		case CONSTANT_Methodref:
		case CONSTANT_Fieldref:
		case CONSTANT_InterfaceMethodref: {
			info.class_index = r->read_u16();
			info.name_and_type_index = r->read_u16();
		} break;
		case CONSTANT_Class: {
			info.name_index = r->read_u16();
		} break;
		case CONSTANT_NameAndType: {
			info.name_index = r->read_u16();
			info.descriptor_index = r->read_u16();
		} break;
		case CONSTANT_Utf8: {
			info.length = r->read_u16();
			info.bytes = (u8 *) malloc(info.length);
			for (u16 i = 0; i < info.length; ++i) {
				info.bytes[i] = r->read_u8();
			}
		} break;
		case CONSTANT_String: {
			info.string_index = r->read_u16();
		} break;
		default: {
			printf("Unhandled tag: %d\n", info.tag);
		}
	}

	return info;
}

Attribute_Info read_attribute_info(Reader *r) {
	Attribute_Info info;

	info.attribute_name_index = r->read_u16();
	info.attribute_length = r->read_u32();

	info.info = (u8 *) malloc(info.attribute_length);
	for (u16 i = 0; i < info.attribute_length; ++i) {
		info.info[i] = r->read_u8();
	}

	return info;
}

Field_Info read_field_info(Reader *r) {
	Field_Info info;

	info.access_flags = r->read_u16();
	info.name_index = r->read_u16();
	info.descriptor_index = r->read_u16();
	info.attributes_count = r->read_u16();

	info.attributes = (Attribute_Info *) malloc(info.attributes_count * sizeof(Attribute_Info));
	for (u16 i = 0; i < info.attributes_count; ++i) {
		info.attributes[i] = read_attribute_info(r);
	}

	return info;
}

Method_Info read_method_info(Reader *r) {
	Method_Info info;

	info.access_flags = r->read_u16();
	info.name_index = r->read_u16();
	info.descriptor_index = r->read_u16();
	info.attributes_count = r->read_u16();

	info.attributes = (Attribute_Info *) malloc(info.attributes_count * sizeof(Attribute_Info));

	for (u16 i = 0; i < info.attributes_count; ++i) {
		info.attributes[i] = read_attribute_info(r);
	}

	return info;
}

void read_class_file(Reader *r, Class_File *cf) {
	cf->magic = r->read_u32();
	cf->minor_version = r->read_u16();
	cf->major_version = r->read_u16();
	
	cf->constant_pool_count = r->read_u16();
	cf->constant_pool = (CP_Info *) malloc(sizeof(CP_Info) * (cf->constant_pool_count-1));
	for (u16 i = 0; i < cf->constant_pool_count - 1; ++i) {
		cf->constant_pool[i] = read_cp_info(r);	
	}

	cf->access_flags = r->read_u16();
	cf->this_class = r->read_u16();
	cf->super_class = r->read_u16();

	cf->interfaces_count = r->read_u16();
	cf->interfaces = (u16 *) malloc(sizeof(u16) * cf->interfaces_count);
	for (u16 i = 0; i < cf->interfaces_count; ++i) {
		cf->interfaces[i] = r->read_u16();
	}

	cf->fields_count = r->read_u16();
	cf->fields = (Field_Info *) malloc(sizeof(Field_Info) * cf->fields_count);
	for (u16 i = 0; i < cf->fields_count; ++i) {
		cf->fields[i] = read_field_info(r);
	}

	cf->methods_count = r->read_u16();
	cf->methods = (Method_Info *) malloc(sizeof(Method_Info) * cf->methods_count);
	for (u16 i = 0; i < cf->methods_count; ++i) {
		cf->methods[i] = read_method_info(r);
	}

	cf->attributes_count = r->read_u16();
	cf->attributes = (Attribute_Info *) malloc(sizeof(Attribute_Info) * cf->attributes_count);
	for (u16 i = 0; i < cf->attributes_count; ++i) {
		cf->attributes[i] = read_attribute_info(r);
	}
}

bool utf8_match(CP_Info cp_info, const char *str) {
	if (strlen(str) != cp_info.length) return false;

	for (u16 k = 0; k < cp_info.length; ++k) {
		if (str[k] != cp_info.bytes[k]) {
			return false;
		}
	}

	return true;
}

void utf8_print(CP_Info cp_info) {
	printf("%.*s\n", cp_info.length, cp_info.bytes);
}

Method_Info *find_method(Class_File *cf, const char *mname) {
	for (u16 i = 0; i < cf->methods_count; ++i) {
		Method_Info *m = &cf->methods[i];
		
		CP_Info name = cf->constant_pool[m->name_index-1];
		if (utf8_match(name, mname)) {
			return m;
		}
	}

	printf("Can't find method '%s'\n", mname);
	return 0;
}

Code_Info find_code(Class_File *cf, Method_Info *m) {
	Code_Info info;

	for (u16 i = 0; i < m->attributes_count; ++i) {
		Attribute_Info *a = &m->attributes[i];
		CP_Info name = cf->constant_pool[a->attribute_name_index-1];

		if (utf8_match(name, "Code")) {
			Reader r(a->info, a->attribute_length);	
			info.max_stack = r.read_u16();
			info.max_locals = r.read_u16();
			info.code_length = r.read_u32();

			info.code = (u8 *) malloc(info.code_length);
			for (u32 i = 0; i < info.code_length; ++i) {
				info.code[i] = r.read_u8();
			}
		}
	}
	
	return info;
}

CP_Info get_cp_info(Class_File *cf, u16 index) {
	return cf->constant_pool[index - 1];
}

CP_Info get_class_name(Class_File *cf, u16 class_index) {
	CP_Info ci = get_cp_info(cf, class_index);
	return get_cp_info(cf, ci.name_index);
}

CP_Info get_member_name(Class_File *cf, u16 name_and_type_index) {
	CP_Info ci = get_cp_info(cf, name_and_type_index);
	return get_cp_info(cf, ci.name_index);
}

void execute_method(Class_File *cf, Method_Info *m) {
	Code_Info ci = find_code(cf, m);	
	Reader r(ci.code, ci.code_length);

	while (r.pos < r.length) {
		u8 opcode = r.read_u8();
		switch (opcode) {
			case OP_LDC: {
				u8 const_index = r.read_u8();
				CP_Info cnst = get_cp_info(cf, const_index);

				utf8_print(cnst);
			} break;
			case OP_GETSTATIC: {
				u16 field_index = r.read_u16();

				CP_Info field_ref = get_cp_info(cf, field_index);
				CP_Info class_name = get_class_name(cf, field_ref.class_index);
				CP_Info member_name = get_member_name(cf, field_ref.name_and_type_index);

				utf8_print(class_name);
				utf8_print(member_name);
			} break;
			case OP_INVOKEVIRTUAL: {
				u16 method_index = r.read_u16();

				CP_Info method_ref = get_cp_info(cf, method_index);
				CP_Info class_name = get_class_name(cf, method_ref.class_index);
				CP_Info member_name = get_member_name(cf, method_ref.name_and_type_index);

				utf8_print(class_name);
				utf8_print(member_name);
			} break;
			case OP_RETURN: {
			} break;
			default: {
				printf("Unhandled opcode: %02x\n", opcode);
			};
		}
	}
}

int main() {
	Reader r("HelloWorld.class");
	Class_File cf;

	read_class_file(&r, &cf);
	Method_Info *main_method = find_method(&cf, "main");
	execute_method(&cf, main_method);

	return 0;
}

