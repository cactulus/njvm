bool utf8_match(UTF8 utf8, const char *str);
bool utf8_match(UTF8 l, UTF8 r);
void utf8_print(UTF8 utf8, bool new_line=true);

enum Type {
	VOID,
	INT,
};

struct FunctionType {
	u32 parameter_count = 0;
	Type return_type;
};

Type parse_type(u8 c);
FunctionType parse_function_type(UTF8 utf8);

struct Value {
	enum Value_Type {
		INT,
		STRING,
		TYPE,
		OBJECT,
	};

	Value_Type type;

	union {
		UTF8 utf8;
		long int int_value;

		struct {
			UTF8 clazz;
			UTF8 member;
		};
	};
};

bool utf8_match(Value value, const char *str);
void value_print(Value value);

Value make_int(long int val);
Value make_string(UTF8 utf8);
Value make_type(UTF8 clazz, UTF8 member);
Value make_object(UTF8 utf8);

struct Call_Frame {
	Method_Info *current_method;
	Value *stack;
	Value *locals;
	u8 *ip;
	u8 sp;
};

struct NJVM {
	Method_Info *current_method;
	Class_File *cf;
	Value *stack;
	Value *locals;
	u8 *ip;
	u8 sp;

	NJVM(Class_File *cf) {
		this->cf = cf;
	}

	void run() {
		Method_Info *main_method = find_method("main");
		call_main(main_method);
	}

	void call_main(Method_Info *m) {
		Code_Info ci = find_code(m);	
		current_method = m;

		stack = (Value *) malloc(ci.max_stack * sizeof(Value));
		locals = (Value *) malloc(ci.max_locals * sizeof(Value));
		ip = ci.code;
		sp = 0;

		bool sbreak = false;
		while (!sbreak) {
			sbreak = execute();
		}
	}

	void call(Method_Info *m, bool on_object) {
		Code_Info ci = find_code(m);
		current_method = m;

		Call_Frame frame = save_frame();

		stack = (Value *) malloc(ci.max_stack * sizeof(Value));
		locals = (Value *) malloc(ci.max_locals * sizeof(Value));
		ip = ci.code;
		sp = 0;

		FunctionType ft = parse_function_type(get_cp_info(m->descriptor_index).utf8);
		u8 par_count = ft.parameter_count;

		for (int k = 1; k <= par_count; ++k) {
			store(k, frame.stack[--frame.sp]);
		}

		if (on_object) {
			/* TOOD: figure out what I must do with this? */
			frame.sp--;
		}

		bool sbreak = false;
		while (!sbreak) {
			sbreak = execute();
		}

		// return value
		if (ft.return_type != Type::VOID) {
			frame.stack[frame.sp++] = pop();
		}

		restore_frame(frame);
	}

	bool execute() {
		u8 opcode = fetch_u8();
		switch (opcode) {
			case OP_ICONST_0: case OP_ICONST_1:
			case OP_ICONST_2: case OP_ICONST_3:
			case OP_ICONST_4: case OP_ICONST_5: {
				push(make_int(opcode - 3));
			} break;
			case OP_BIPUSH: {
				push(make_int(fetch_u8()));
			} break;
			case OP_SIPUSH: {
				push(make_int(fetch_u16()));
			} break;
			case OP_LDC: {
				u8 const_index = fetch_u8();
				CP_Info cnst = get_cp_info(const_index + 1);

				push(make_string(cnst.utf8));
			} break;
			case OP_ILOAD: {
				load(fetch_u8());
			} break;
			case OP_ALOAD: {
				load(fetch_u8());
			} break;
			case OP_ALOAD_0: case OP_ALOAD_1:
			case OP_ALOAD_2: case OP_ALOAD_3: {
				load(opcode - 0x2a);
			} break;
			case OP_ASTORE: {
				store(fetch_u8());
			} break;
			case OP_ASTORE_0: case OP_ASTORE_1:
			case OP_ASTORE_2: case OP_ASTORE_3: {
				store(opcode - 0x4b);
			} break;
			case OP_ILOAD_0: case OP_ILOAD_1:
			case OP_ILOAD_2: case OP_ILOAD_3: {
				load(opcode - 0x1a);
			} break;
			case OP_ISTORE: {
				store(fetch_u8());
			} break;
			case OP_ISTORE_0: case OP_ISTORE_1:
			case OP_ISTORE_2: case OP_ISTORE_3: {
				store(opcode - 0x3b);
			} break;
			case OP_POP: {
				pop();
			} break;
			case OP_DUP: {
				Value val = pop();
				push(val);
				push(val);
			} break;
			case OP_IADD: {
				long int l = pop().int_value;
				long int r = pop().int_value;

				push(make_int(l + r));
			} break;
			case OP_IRETURN:
			case OP_RETURN: {
				return true;
			} break;
			case OP_GETSTATIC: {
				u16 field_index = fetch_u16();

				CP_Info field_ref = get_cp_info(field_index);
				CP_Info class_name = get_class_name(field_ref.class_index);
				CP_Info member_name = get_member_name(field_ref.name_and_type_index);

				Value type = make_type(class_name.utf8, member_name.utf8);
				push(type);
			} break;
			case OP_INVOKEVIRTUAL: {
				u16 method_index = fetch_u16();

				CP_Info method_ref = get_cp_info(method_index);
				CP_Info class_name = get_class_name(method_ref.class_index);
				CP_Info member_name = get_member_name(method_ref.name_and_type_index);

				if	(utf8_match(class_name.utf8, "java/io/PrintStream") && utf8_match(member_name.utf8, "println")) {
					Value val = pop();
					Value field = pop();

					if (utf8_match(field.clazz, "java/lang/System") && utf8_match(field.member, "out")) {
					 	value_print(val);
					}
				} else {
					for (u16 i = 0; i < cf->methods_count; ++i) {
						Method_Info *mi = &cf->methods[i];
						CP_Info name = get_cp_info(mi->name_index);

						/* check class later */
						if (utf8_match(member_name.utf8, name.utf8)) {
							call(mi, true);
							break;
						}
					}
				}
			} break;
			case OP_INVOKESPECIAL: {
				u16 method_index = fetch_u16();
				CP_Info method_ref = get_cp_info(method_index);
				CP_Info class_name = get_class_name(method_ref.class_index);
				CP_Info member_name = get_member_name(method_ref.name_and_type_index);

				Value field = pop();
				
				for (u16 i = 0; i < cf->methods_count; ++i) {
					Method_Info *mi = &cf->methods[i];
					CP_Info name = get_cp_info(mi->name_index);

					/* check class later */
					/*
					if (utf8_match(member_name.utf8, name.utf8)) {
						call(mi, true);
						break;
					}*/
				}
			} break;
			case OP_INVOKESTATIC: {
				u16 method_index = fetch_u16();

				CP_Info method_ref = get_cp_info(method_index);
				CP_Info member_name = get_member_name(method_ref.name_and_type_index);
				
				for (u16 i = 0; i < cf->methods_count; ++i) {
					Method_Info *mi = &cf->methods[i];
					CP_Info name = get_cp_info(mi->name_index);

					/* check class later */
					if (utf8_match(member_name.utf8, name.utf8)) {
						call(mi, false);
						break;
					}
				}
			} break;
			case OP_NEW: {
				u16 index = fetch_u16();

				CP_Info constant_clazz = get_cp_info(index);
				CP_Info class_name = get_cp_info(constant_clazz.name_index);

				push(make_object(class_name.utf8));
			} break;
			default: {
				printf("Unhandled opcode: %02x\n", opcode);
			};
		}

		return false;
	}

	void push(Value val) {
		stack[sp++] = val;
	}

	Value pop() {
		return stack[--sp];
	}

	void store(u8 index, Value value) {
		assert(index > 0);
		locals[index - 1] = value;
	}

	void store(u8 index) {
		store(index, pop());
	}

	void load(u8 index) {
		assert(index > 0);
		push(locals[index - 1]);
	}

	u8 fetch_u8() {
		return *ip++;
	}

	u16 fetch_u16() {
		u16 f = fetch_u8();
		u16 s = fetch_u8();
		return (f << 8) | s;
	}

	Call_Frame save_frame() {
		Call_Frame frame;
		
		frame.current_method = current_method;
		frame.stack = stack;
		frame.locals = locals;
		frame.ip = ip;
		frame.sp = sp;

		return frame;
	}

	void restore_frame(Call_Frame frame) {
		delete[] stack;
		delete[] locals;

		stack = frame.stack;
		locals = frame.locals;
		ip = frame.ip;
		sp = frame.sp;
		current_method = frame.current_method;
	}

	Method_Info *find_method(const char *mname) {
		for (u16 i = 0; i < cf->methods_count; ++i) {
			Method_Info *m = &cf->methods[i];
			
			CP_Info name = get_cp_info(m->name_index);
			if (utf8_match(name.utf8, mname)) {
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

			if (utf8_match(name.utf8, "Code")) {
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
	
	void debug_info() {
		printf("------------- DEBUG INFO -------------\n");
		printf("Current function: ");
		utf8_print(get_cp_info(current_method->name_index).utf8);
		printf("Stack pointer: 0x%02x\n", sp);
		printf("Inst. pointer: 0x%p\n", ip);
		printf("\nStack\n");
		
		for (u8 i = 0; i < sp; ++i) {
			printf("%02x: ", i);
			debug_value(stack[i]);
		} 
		
		printf("--------------------------------------\n");
	}

	void debug_value(Value value) {
		switch (value.type) {
			case Value::STRING: {
				printf("string '");
				utf8_print(value.utf8, false);
				putc('\'', stdout);
				putc('\n', stdout);
			} break;
			case Value::INT: printf("int %ld\n", value.int_value); break;
			case Value::TYPE: {
				printf("type ");
				utf8_print(value.clazz, false);
				printf(".");
				utf8_print(value.member);
			} break;
			case Value::OBJECT: {
				printf("object ");
				utf8_print(value.utf8);
			}
		}
	}
};

Type parse_type(u8 c) {
	switch (c) {
		case 'V': return Type::VOID;
		case 'I': return Type::INT;
		default: {
			printf("Unknown type '%c'\n", c);
		};
	}
	
	return Type::VOID;
}

FunctionType parse_function_type(UTF8 utf8) {
	FunctionType type;
	
	bool pars = false;
	for (u16 i = 0; i < utf8.length; ++i) {
		u8 c = utf8.bytes[i];

		if (c == '(') {
			pars = true;
		} else if (c == ')') {
			pars = false;
		} else {
			if (pars) {
				type.parameter_count++;
			} else {
				type.return_type = parse_type(c);
			}
		}
	}

	return type;
}

bool utf8_match(UTF8 utf8, const char *str) {
	if (strlen(str) != utf8.length) return false;

	for (u16 k = 0; k < utf8.length; ++k) {
		if (str[k] != utf8.bytes[k]) {
			return false;
		}
	}

	return true;
}

bool utf8_match(UTF8 l, UTF8 r) {
	if (l.length != r.length) return false;

	for (u16 k = 0; k < l.length; ++k) {
		if (l.bytes[k] != r.bytes[k]) {
			return false;
		}
	}

	return true;
}

void utf8_print(UTF8 utf8, bool new_line) {
	printf("%.*s", utf8.length, utf8.bytes);
	if (new_line)
		putc('\n', stdout);
}

void value_print(Value value) {
	switch (value.type) {
		case Value::STRING: utf8_print(value.utf8); break;
		case Value::INT: printf("%ld\n", value.int_value); break;
		default: printf("Havent implemented print for type %d\n", value.type); break;
	}
}

Value make_int(long int val) {
	Value v;
	v.type = Value::INT;
	v.int_value = val;	
	return v;
}

Value make_string(UTF8 utf8) {
	Value v;
	v.type = Value::STRING;
	v.utf8 = utf8;
	return v;
}

Value make_type(UTF8 clazz, UTF8 member) {
	Value v;
	v.type = Value::TYPE;
	v.clazz = clazz;
	v.member = member;
	return v;
}

Value make_object(UTF8 utf8) {
	Value v;
	v.type = Value::OBJECT;
	v.utf8 = utf8;
	return v;
}