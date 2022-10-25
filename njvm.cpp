bool utf8_match(CP_Info cp_info, const char *str);
void utf8_print(CP_Info cp_info);

struct Value {
	enum Value_Type {
		INT,
		STRING
	};

	Value_Type type;
};

struct Stack {
	Value *stack = 0;
	u16 size;
	u16 pos;

	void setup(u16 max_size) {
		if (stack) {
			free(stack);
		}

		stack = (Value *) malloc(sizeof(Value) * max_size);
		size = max_size;
		pos = 0;
	}

	void push(Value val) {
		assert(pos < size && "Stackoverflow");
		stack[pos++] = val;
	}

	Value *pop() {
		assert(pos > 0);
		return &stack[--pos];
	}
};

struct NJVM {
	Class_File *cf;
	Stack stack;
	u8 *ip;

	NJVM(Class_File *cf) {
		this->cf = cf;
	}

	void run() {
		Method_Info *main_method = find_method("main");
		execute_method(main_method);
	}

	void execute_method(Method_Info *m) {
		Code_Info ci = find_code(m);	
		stack.setup(ci.max_stack);
		ip = ci.code;

		while (ip < ci.code + ci.code_length) {
			u8 opcode = fetch_u8();
			switch (opcode) {
				case OP_LDC: {
					u8 const_index = fetch_u8();
					CP_Info cnst = get_cp_info(const_index + 1);

					utf8_print(cnst);
				} break;
				case OP_GETSTATIC: {
					u16 field_index = fetch_u16();

					CP_Info field_ref = get_cp_info(field_index);
					CP_Info class_name = get_class_name(field_ref.class_index);
					CP_Info member_name = get_member_name(field_ref.name_and_type_index);

					utf8_print(class_name);
					utf8_print(member_name);
				} break;
				case OP_INVOKEVIRTUAL: {
					u16 method_index = fetch_u16();

					CP_Info method_ref = get_cp_info(method_index);
					CP_Info class_name = get_class_name(method_ref.class_index);
					CP_Info member_name = get_member_name(method_ref.name_and_type_index);

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

	u8 fetch_u8() {
		return *ip++;
	}

	u16 fetch_u16() {
		u16 f = fetch_u8();
		u16 s = fetch_u8();
		return (f << 8) | s;
	}

	Method_Info *find_method(const char *mname) {
		for (u16 i = 0; i < cf->methods_count; ++i) {
			Method_Info *m = &cf->methods[i];
			
			CP_Info name = get_cp_info(m->name_index);
			if (utf8_match(name, mname)) {
				return m;
			}
		}

		printf("Can't find method '%s'\n", mname);
		return 0;
	}

	Code_Info find_code(Method_Info *m) {
		Code_Info info;

		for (u16 i = 0; i < m->attributes_count; ++i) {
			Attribute_Info *a = &m->attributes[i];
			CP_Info name = get_cp_info(a->attribute_name_index);

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

	CP_Info get_cp_info(u16 index) {
		return cf->constant_pool[index - 1];
	}

	CP_Info get_class_name(u16 class_index) {
		CP_Info ci = get_cp_info(class_index);
		return get_cp_info(ci.name_index);
	}

	CP_Info get_member_name(u16 name_and_type_index) {
		CP_Info ci = get_cp_info(name_and_type_index);
		return get_cp_info(ci.name_index);
	}
	
};

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

