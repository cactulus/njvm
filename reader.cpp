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
        if (!f) {
            printf("Failed to open file '%s'\n", file_name);
            exit(1);
        }

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

struct ClassReader {
	CP_Info *cp;
	Reader *r;

	ClassReader(const char *file_name) {
		r = new Reader(file_name);
	}

	String read_name() {
		u16 index = r->read_u16();
		CP_Info cpi = cp[index - 1];
		return cpi.utf8;
	}

	String read_class_name() {
		u16 index = r->read_u16();
		CP_Info cpi = cp[index - 1];
		CP_Info ccpi = cp[cpi.name_index - 1];
		return ccpi.utf8;
	}

	CP_Info read_cp_info() {
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
				String utf8;

				utf8.length = r->read_u16();
				utf8.data = (u8 *) malloc(utf8.length);
				for (u16 i = 0; i < utf8.length; ++i) {
					utf8.data[i] = r->read_u8();
				}
				info.utf8 = utf8;
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

	Attribute read_attribute() {
		Attribute info;

		info.name = read_name();
		info.attribute_length = r->read_u32();

		info.info = (u8 *) malloc(info.attribute_length);
		for (u16 i = 0; i < info.attribute_length; ++i) {
			info.info[i] = r->read_u8();
		}

		return info;
	}

	Field read_field() {
		Field info;

		info.access_flags = r->read_u16();
		info.name = read_name();
		info.type = read_type();
		info.attributes_count = r->read_u16();

		info.attributes = (Attribute *) malloc(info.attributes_count * sizeof(Attribute));
		for (u16 i = 0; i < info.attributes_count; ++i) {
			info.attributes[i] = read_attribute();
		}

		return info;
	}

	Method read_method() {
		Method info;

		info.access_flags = r->read_u16();
		info.name = read_name();
		info.type = read_type();
		
		info.attributes_count = r->read_u16();
		info.attributes = (Attribute *) malloc(info.attributes_count * sizeof(Attribute));
		for (u16 i = 0; i < info.attributes_count; ++i) {
			info.attributes[i] = read_attribute();
		}

		return info;
	}

	Class *read() {
		u32 magic = r->read_u32();
		if (magic != 0xcafebabe) {
			printf("Magic number incorrect\n");
			exit(1);
		}

		Class *clazz = new Class();

		u16 minor_version = r->read_u16();
		u16 major_version = r->read_u16();
		
		clazz->constant_pool_count = r->read_u16();
		clazz->constant_pool = (CP_Info *) malloc(sizeof(CP_Info) * (clazz->constant_pool_count-1));
		for (u16 i = 0; i < clazz->constant_pool_count - 1; ++i) {
			clazz->constant_pool[i] = read_cp_info();	
		}
		cp = clazz->constant_pool;

		clazz->access_flags = r->read_u16();
		clazz->name = read_class_name();
		clazz->super_name = read_class_name();

		u16 interfaces_count = r->read_u16();
		for (u16 i = 0; i < interfaces_count; ++i) {
			r->pos += 2;
		}

		clazz->fields_count = r->read_u16();
		clazz->fields = (Field *) malloc(sizeof(Field) * clazz->fields_count);
		for (u16 i = 0; i < clazz->fields_count; ++i) {
			clazz->fields[i] = read_field();
		}

		clazz->methods_count = r->read_u16();
		clazz->methods = (Method *) malloc(sizeof(Method) * clazz->methods_count);
		for (u16 i = 0; i < clazz->methods_count; ++i) {
			clazz->methods[i] = read_method();
		}

		u16 attributes_count = r->read_u16();
		for (u16 i = 0; i < attributes_count; ++i) {
			read_attribute();
		}

		return clazz;
	}

	~ClassReader() {
		delete r;
	}

	Type *read_type() {
		String str = read_name();
		return parse_type(str);
	}

	Type *parse_type(String str) {
		u8 first = str.data[0];

		if (first == '(') {
			Type *ty = new Type();
			ty->type = Type::FUNCTION;
			
			u16 i = 1;
			while (str.data[i] != ')') {
				ty->parameters.add(parse_type(str, &i));
			}
			i++;
			ty->return_type = parse_type(str, &i);

			return ty;
		} else {
			return parse_type(str, 0);
		}
	}
		
	Type *parse_type(String str, u16 *i) {
		u16 start = i ? *i : 0;
		u8 c = str.data[start];

		if (c == 'L') {
			u16 len = 0;
			u16 p = start + 1;
			String s;

			c = str.data[p];
			while (c != ';') {
				c = str.data[++p];
				len++;
			}

			(*i) += len + 2; /* 2 -> L and ;*/

			Type *ty = new Type();
			ty->clazz_name = str.substring(start + 1, len);
			return ty;
		} else if (c == '[') {
			(*i)++;
			Type *ty = new Type();
			ty->type = Type::ARRAY;
			ty->element_type = parse_type(str, i);
			return ty;
		} else {
			if (i) {
				(*i)++;
			}
			switch (c) {
				case 'V': return type_int; 
				case 'I': return type_int; 
				default: {
					printf("Unknown type '%c'\n", c);
				};
			}
		}
		
		return type_void;
	}
};