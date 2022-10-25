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
			UTF8 utf8;

			utf8.length = r->read_u16();
			utf8.bytes = (u8 *) malloc(utf8.length);
			for (u16 i = 0; i < utf8.length; ++i) {
				utf8.bytes[i] = r->read_u8();
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
