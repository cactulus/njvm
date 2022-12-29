namespace interp {
  struct Value {
    enum Value_Type {
      INT,
      STRING,
      TYPE,
      OBJECT,
    };

    Value_Type type;

    union {
        String utf8;
        long int int_value;

        struct {
            String clazz;
            String member;
        };
    };

    Value() {}
  };

  void value_print(Value value);

  Value make_int(long int val);

  Value make_string(String str);

  Value make_type(String clazz, String member);

  Value make_object(String utf8);

  struct Call_Frame {
    Class *clazz;
    Method *method;
    Value *stack;
    Value *locals;
    u8 *ip;
    u8 sp;
  };

  struct Interpreter : Backend {
    Value *stack;
    Value *locals;

    Interpreter(Class *main_clazz) : Backend(main_clazz) {
    }

    void run() override {
        Method *main_method = find_method("main");
        call_main(main_method);
    }

    bool execute() {
      u8 opcode = fetch_u8();

      switch (opcode) {
        case OP_ICONST_0:
        case OP_ICONST_1:
        case OP_ICONST_2:
        case OP_ICONST_3:
        case OP_ICONST_4:
        case OP_ICONST_5: {
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
        case OP_ALOAD_0:
        case OP_ALOAD_1:
        case OP_ALOAD_2:
        case OP_ALOAD_3: {
          load(opcode - 0x2a);
        } break;
        case OP_ASTORE: {
          store(fetch_u8());
        } break;
        case OP_ASTORE_0:
        case OP_ASTORE_1:
        case OP_ASTORE_2:
        case OP_ASTORE_3: {
          store(opcode - 0x4b);
        } break;
        case OP_ILOAD_0:
        case OP_ILOAD_1:
        case OP_ILOAD_2:
        case OP_ILOAD_3: {
          load(opcode - 0x1a);
        } break;
        case OP_ISTORE: {
          store(fetch_u8());
        } break;
        case OP_ISTORE_0:
        case OP_ISTORE_1:
        case OP_ISTORE_2:
        case OP_ISTORE_3: {
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
        case OP_IADD:
        case OP_ISUB:
        case OP_IMUL:
        case OP_IDIV:
        case OP_IREM:
        case OP_ISHL:
        case OP_ISHR: {
          long int l = pop().int_value;
          long int r = pop().int_value;

          switch (opcode) {
            case OP_IADD:
              push(make_int(l + r));
              break;
            case OP_ISUB:
              push(make_int(l - r));
              break;
            case OP_IMUL:
              push(make_int(l * r));
              break;
            case OP_IDIV:
              push(make_int(l / r));
              break;
            case OP_IREM:
              push(make_int(l % r));
              break;
            case OP_ISHL:
              push(make_int(l << r));
              break;
            case OP_ISHR:
              push(make_int(l >> r));
              break;
          }
        } break;
        case OP_INEG: {
          Value v = pop();
          v.int_value = -v.int_value;
          push(v);
        } break;
        case OP_IINC: {
          u8 index = fetch_u8();
          locals[index].int_value++;
        } break;
        case OP_GOTO: {
          u16 offset_u = fetch_u16();
          s16 offset = (s16) offset_u;

          ip += offset;
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

          if (class_name.utf8 == "java/io/PrintStream" && member_name.utf8 == "println") {
            Value val = pop();
            Value field = pop();

            if (field.clazz == "java/lang/System" && field.member == "out") {
              value_print(val);
            }
          } else {
            Method *m = find_method(class_name.utf8, member_name.utf8);
            if (m) {
              call(m, true);
            }
          }
        } break;
        case OP_INVOKESPECIAL: {
          u16 method_index = fetch_u16();
          CP_Info method_ref = get_cp_info(method_index);
          CP_Info class_name = get_class_name(method_ref.class_index);
          CP_Info member_name = get_member_name(method_ref.name_and_type_index);

          Method *m = find_method(class_name.utf8, member_name.utf8);
          if (m) {
              call(m, true);
          }
        } break;
        case OP_INVOKESTATIC: {
          u16 method_index = fetch_u16();

          CP_Info method_ref = get_cp_info(method_index);
          CP_Info member_name = get_member_name(method_ref.name_and_type_index);

          Method *m = find_method(member_name.utf8);
          call(m, false);
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

    void call_main(Method *m) {
      Code ci = find_code(m);
      method = m;

      stack = (Value *) malloc(ci.max_stack * sizeof(Value));
      locals = (Value *) malloc(ci.max_locals * sizeof(Value));
      ip = ci.code;
      sp = 0;

      bool ret_type = false;
      while (!ret_type) {
        ret_type = execute();
      }
    }

    void call(Method *m, bool on_object) {
      Code ci = find_code(m);
      method = m;

      Call_Frame frame = save_frame();

      stack = (Value *) malloc(ci.max_stack * sizeof(Value));
      locals = (Value *) malloc(ci.max_locals * sizeof(Value));
      ip = ci.code;
      sp = 0;

      u8 par_count = m->type->parameters.length;

      for (int k = 0; k < par_count; ++k) {
        store(k, frame.stack[--frame.sp]);
      }

      if (on_object) {
        /* TOOD: figure out what I must do with this? */
        frame.sp--;
      }

      bool ret_type = false;
      while (!ret_type) {
        ret_type = execute();
      }

      // return value
      if (m->type->return_type->type != NType::VOID) {
        frame.stack[frame.sp++] = pop();
      }

      restore_frame(frame);
    }

    void push(Value val) {
      stack[sp++] = val;
    }

    Value pop() {
      return stack[--sp];
    }

    void store(u8 index, Value value) {
      locals[index] = value;
    }

    void store(u8 index) {
      store(index, pop());
    }

    void load(u8 index) {
      push(locals[index]);
    }

    Call_Frame save_frame() {
      Call_Frame frame;

      frame.clazz = clazz;
      frame.method = method;
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
      method = frame.method;
      clazz = frame.clazz;
    }

    void debug_info() {
      printf("------------- DEBUG INFO -------------\n");
      printf("Current class: ");
      string_println(clazz->name);
      printf("Current method: ");
      string_println(method->name);
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
          string_print(value.utf8);
          putc('\'', stdout);
          putc('\n', stdout);
        } break;
        case Value::INT:
          printf("int %ld\n", value.int_value);
          break;
        case Value::TYPE: {
          printf("type ");
          string_print(value.clazz);
          printf(".");
          string_println(value.member);
        } break;
        case Value::OBJECT: {
          printf("object ");
          string_println(value.utf8);
        }
      }
    }
  };

  void value_print(Value value) {
    switch (value.type) {
      case Value::STRING:
        string_println(value.utf8);
        break;
      case Value::INT:
        printf("%ld\n", value.int_value);
        break;
      default:
        printf("Havent implemented print for type %d\n", value.type);
        break;
    }
  }

  Value make_int(long int val) {
    Value v;
    v.type = Value::INT;
    v.int_value = val;
    return v;
  }

  Value make_string(String utf8) {
    Value v;
    v.type = Value::STRING;
    v.utf8 = utf8;
    return v;
  }

  Value make_type(String clazz, String member) {
    Value v;
    v.type = Value::TYPE;
    v.clazz = clazz;
    v.member = member;
    return v;
  }

  Value make_object(String utf8) {
    Value v;
    v.type = Value::OBJECT;
    v.utf8 = utf8;
    return v;
  }
}