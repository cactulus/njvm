extern "C" {
    void print_int(s64 a) {
        printf("%lld\n", a);
    }

    void *create_array(s64 size, s64 type_size) {
        return malloc(type_size * size);
    }
}

namespace jit {
    using namespace llvm;
    using namespace llvm::orc;

#define STR_REF(x) StringRef((const char * ) x.data, x.length)

    struct ControlFlow {
        Array<u16> offsets;
        Array<BasicBlock *> blocks;

        void add(u16 offset, BasicBlock *block) {
            offsets.add(offset);
            blocks.add(block);
        }

        BasicBlock *find(u16 offset) {
            for (s64 i = 0; i < offsets.length; ++i)
                if (offsets[i] == offset)
                    return blocks[i];

            return 0;
        }
    };

    struct JavaArray {
        Value *llvm_ref = 0;
        Value *length = 0;
        u8 type = 0;
    };

    struct JavaObject {
        enum JavaObjectType {
            ARRAY,
            CLASS
        };

        JavaObjectType type;
        JavaArray array;
        String name;
    };

    struct Jit : Backend {
        LLVMContext context;
        std::unique_ptr<Module> module;
        IRBuilder<> *irb;
        Value **stack_int;
        Value **locals_int;

        JavaObject *stack_object;
        JavaObject *locals_object;

        ControlFlow control_flow;

        Type *llty_i1;
        Type *llty_i8;
        Type *llty_i16;
        Type *llty_i32;
        Type *llty_i64;
        Type *llty_void;
        Type *llty_i8_ptr;

        // TODO: move somewhere else later
        Function *print_int_fn = 0;
        Function *create_array_fn = 0;

        Jit(Class *main_clazz) : Backend(main_clazz) {
            InitializeAllTargetInfos();
            InitializeAllTargets();
            InitializeAllTargetMCs();
            InitializeAllAsmParsers();
            InitializeAllAsmPrinters();

            module = std::make_unique<Module>("njit", context);
            irb = new IRBuilder<>(context);

            llty_i1 = Type::getInt1Ty(context);
            llty_i8 = Type::getInt8Ty(context);
            llty_i16 = Type::getInt16Ty(context);
            llty_i32 = Type::getInt32Ty(context);
            llty_i64 = Type::getInt64Ty(context);
            llty_void = Type::getVoidTy(context);
            llty_i8_ptr = llty_i8->getPointerTo();

            // TODO: move somewhere else later
            auto print_int_fn_ty = FunctionType::get(llty_void, {llty_i64}, false);
            print_int_fn = Function::Create(print_int_fn_ty, Function::ExternalLinkage, "print_int", *module);

            auto create_array_fn_ty = FunctionType::get(llty_i8_ptr, {llty_i64, llty_i64}, false);
            create_array_fn = Function::Create(create_array_fn_ty, Function::ExternalLinkage, "create_array", *module);
        }

        void run() override {
            for (u16 i = 0; i < clazz->fields_count; ++i) {
                convert_field(&clazz->fields[i]);
            }

            for (u16 i = 0; i < clazz->methods_count; ++i) {
                convert_method(&clazz->methods[i]);
            }

            finalize();
        }

        void finalize() {
            // optimize();
            module->print(outs(), 0);
            if (verifyModule(*module, &outs())) {
                return;
            }

            EngineBuilder eb = EngineBuilder(std::move(module));

            ExecutionEngine *ee = eb.create();

            void (*print_int_ptr)(s64) = print_int;
            ee->addGlobalMapping(print_int_fn, (void *)print_int_ptr);

            void *(*create_array_ptr)(s64, s64) = create_array;
            ee->addGlobalMapping(create_array_fn, (void *) create_array);

            void (*main)() = (void (*)())(intptr_t) ee->getFunctionAddress("main");
            main();
        }

        void SetInsertBlock(BasicBlock *bb) {
            if (!irb->GetInsertBlock()->getTerminator()) {
                irb->CreateBr(bb);
            }

            irb->SetInsertPoint(bb);
        }

        void convert_opcode() {
            u8 opcode = fetch_u8();

            BasicBlock *bb = control_flow.find(base_offset());
            if (bb) {
                SetInsertBlock(bb);
            }

            switch (opcode) {
                case OP_ICONST_0:
                case OP_ICONST_1:
                case OP_ICONST_2:
                case OP_ICONST_3:
                case OP_ICONST_4:
                case OP_ICONST_5: {
                    push_int(make_int(opcode - 3));
                } break;
                case OP_BIPUSH: {
                    push_int(make_int((s8) fetch_u8()));
                } break;
                case OP_SIPUSH: {
                    push_int(make_int((s16) fetch_u16()));
                } break;
                case OP_LDC: {
                    u8 index = fetch_u8();
                    CP_Info info = get_cp_info(index);
                    switch (info.tag) {
                        case CONSTANT_Integer:
                        case CONSTANT_Long:
                            push_int(make_int((s64) info.long_int));
                            break;
                        default:
                            assert(0 && "No implementation for LDC for type used");
                    }
                } break;
                case OP_ILOAD: {
                    load_int(fetch_u8());
                } break;
                case OP_ALOAD: {
                    load_array(fetch_u8());
                } break;
                case OP_ALOAD_0:
                case OP_ALOAD_1:
                case OP_ALOAD_2:
                case OP_ALOAD_3: {
                    load_array(opcode - 0x2a);
                } break;
                case OP_IALOAD:
                case OP_LALOAD:
                case OP_BALOAD:
                case OP_SALOAD: {
                    Value *index = pop_int();
                    JavaArray *arr = pop_array();

                    Value *arr_ref = load(arr->llvm_ref);
                    arr_ref = irb->CreateBitOrPointerCast(arr_ref, java_to_llvm_type(arr->type)->getPointerTo());
                    Value *pos = gep(arr_ref, index);

                    push_int(load(pos));
                } break;
                case OP_ASTORE: {
                    store_array(fetch_u8());
                } break;
                case OP_ASTORE_0:
                case OP_ASTORE_1:
                case OP_ASTORE_2:
                case OP_ASTORE_3: {
                    store_array(opcode - 0x4b);
                } break;
                case OP_ILOAD_0:
                case OP_ILOAD_1:
                case OP_ILOAD_2:
                case OP_ILOAD_3: {
                    load_int(opcode - 0x1a);
                } break;
                case OP_ISTORE: {
                    store_int(fetch_u8());
                } break;
                case OP_ISTORE_0:
                case OP_ISTORE_1:
                case OP_ISTORE_2:
                case OP_ISTORE_3: {
                    store_int(opcode - 0x3b);
                } break;
                case OP_IASTORE:
                case OP_LASTORE:
                case OP_BASTORE:
                case OP_SASTORE: {
                    Value *val = pop_int();
                    Value *index = pop_int();
                    JavaArray *arr = pop_array();

                    Value *arr_ref = load(arr->llvm_ref);
                    arr_ref = irb->CreateBitOrPointerCast(arr_ref, java_to_llvm_type(arr->type)->getPointerTo());
                    Value *pos = gep(arr_ref, index);
                    llvm_store_int(val, pos);
                } break;
                case OP_POP: {
                    sp--;
                } break;
                case OP_DUP: {
                    Value *val = stack_int[sp - 1];
                    llvm_store_int(load(val), stack_int[sp]);

                    JavaObject *obj = &stack_object[sp - 1];
                    JavaObject *odup = &stack_object[sp];

                    JavaArray *arr = &obj->array;
                    JavaArray *adup = &odup->array;

                    odup->type = obj->type;
                    odup->name = obj->name;
                    adup->length = arr->length;
                    adup->type = arr->type;
                    irb->CreateStore(load(arr->llvm_ref), adup->llvm_ref);

                    sp++;
                } break;
                case OP_IADD:
                case OP_ISUB:
                case OP_IMUL:
                case OP_IDIV:
                case OP_IREM:
                case OP_ISHL:
                case OP_ISHR:
                case OP_IAND:
                case OP_IOR: {
                    Value *l = pop_int();
                    Value *r = pop_int();
                    Instruction::BinaryOps op;

                    switch (opcode) {
                        case OP_IADD: op = Instruction::BinaryOps::Add; break;
                        case OP_ISUB: op = Instruction::BinaryOps::Sub; break;
                        case OP_IMUL: op = Instruction::BinaryOps::Mul; break;
                        case OP_IDIV: op = Instruction::BinaryOps::SDiv; break;
                        case OP_IREM: op = Instruction::BinaryOps::SRem; break;
                        case OP_ISHL: op = Instruction::BinaryOps::AShr; break;
                        case OP_ISHR: op = Instruction::BinaryOps::AShr; break;
                        case OP_IAND: op = Instruction::BinaryOps::And; break;
                        case OP_IOR: op = Instruction::BinaryOps::Or; break;
                    }

                    push_int(irb->CreateBinOp(op, l, r));
                } break;
                case OP_INEG: {
                    Value *v = pop_int();
                    v = irb->CreateNeg(v);
                    push_int(v);
                } break;
                case OP_IINC: {
                    u8 index = fetch_u8();
                    s8 value = (s8) fetch_u8();

                    Value *local = load_local_int(index);
                    Value *added = irb->CreateAdd(local, make_int(value));
                    store_int(index, added);
                } break;
                case OP_IFEQ:
                case OP_IFNE:
                case OP_IFLT:
                case OP_IFGE:
                case OP_IFGT:
                case OP_IFLE: {
                    Value *zero = make_int(0);
                    Value *val = pop_int();
                    u16 off = fetch_offset();
                    CmpInst::Predicate op;

                    switch (opcode) {
                        case OP_IFEQ: op = ICmpInst::ICMP_EQ; break;
                        case OP_IFNE: op = ICmpInst::ICMP_NE; break;
                        case OP_IFLT: op = ICmpInst::ICMP_SLT; break;
                        case OP_IFGE: op = ICmpInst::ICMP_SGE; break;
                        case OP_IFGT: op = ICmpInst::ICMP_SGT; break;
                        case OP_IFLE: op = ICmpInst::ICMP_SLE; break;
                    }

                    create_cond_jump(op, val, zero, off);
                } break;
                case OP_IF_ICMPEQ:
                case OP_IF_ICMPNE:
                case OP_IF_ICMPLT:
                case OP_IF_ICMPGE:
                case OP_IF_ICMPGT:
                case OP_IF_ICMPLE: {
                    Value *l = pop_int();
                    Value *r = pop_int();
                    u16 off = fetch_offset();

                    CmpInst::Predicate op;

                    switch (opcode) {
                        case OP_IF_ICMPEQ: op = ICmpInst::ICMP_EQ; break;
                        case OP_IF_ICMPNE: op = ICmpInst::ICMP_NE; break;
                        case OP_IF_ICMPLT: op = ICmpInst::ICMP_SLT; break;
                        case OP_IF_ICMPGE: op = ICmpInst::ICMP_SGE; break;
                        case OP_IF_ICMPGT: op = ICmpInst::ICMP_SGT; break;
                        case OP_IF_ICMPLE: op = ICmpInst::ICMP_SLE; break;
                    }

                    create_cond_jump(op, r, l, off);
                } break;
                case OP_GOTO: {
                    u16 off = fetch_offset();

                    BasicBlock *target = get_or_create_block(off);
                    irb->CreateBr(target);
                } break;
                case OP_RETURN:
                    irb->CreateRetVoid();
                    break;
                case OP_IRETURN:
                    irb->CreateRet(pop_int());
                    break;
                case OP_GETSTATIC: {
                    u16 field_index = fetch_u16();

                    CP_Info field_ref = get_cp_info(field_index);
                    CP_Info class_name = get_class_name(field_ref.class_index);
                    CP_Info member_name = get_member_name(field_ref.name_and_type_index);

                    /* TODO:  */
                    sp++;
                } break;
                case OP_INVOKEVIRTUAL: {
                    u16 method_index = fetch_u16();

                    CP_Info method_ref = get_cp_info(method_index);
                    CP_Info class_name = get_class_name(method_ref.class_index);
                    CP_Info member_name = get_member_name(method_ref.name_and_type_index);

                    if (class_name.utf8 == "java/io/PrintStream" && member_name.utf8 == "println") {
                        call(print_int_fn, 1, true);
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
                    if (m) {
                        call(m, false);
                    }
                } break;
                case OP_NEW: {
                    u16 index = fetch_u16();

                    CP_Info constant_clazz = get_cp_info(index);
                    CP_Info class_name = get_cp_info(constant_clazz.name_index);

                    // push(make_object(class_name.utf8));
                } break;
                case OP_NEWARRAY: {
                    u8 type = fetch_u8();
                    Value *size = pop_int();
                    Value *type_size = 0;
                    switch (type) {
                        case TYPE_BOOLEAN: type_size = make_int(1); break;
                        case TYPE_BYTE: type_size = make_int(1); break;
                        case TYPE_SHORT: type_size = make_int(2); break;
                        case TYPE_INT: type_size = make_int(4); break;
                        case TYPE_LONG: type_size = make_int(8); break;
                        default:
                            assert(0 && "Array type not implemented");
                            break;
                    }

                    Value *ptr = irb->CreateCall(create_array_fn, {size, type_size});
                    push_array(ptr, type, size);
                } break;
                case OP_ARRAYLENGTH: {
                    JavaArray *arr = pop_array();
                    push_int(arr->length);
                } break;
            }
        }

        void call(Method *m, bool on_object) {
            if (!m->llvm_ref) {
                convert_method(m);
            }

            Value *ret_val = call(m->llvm_ref, m->type->parameters.length, on_object);

            if (m->type->return_type->type == NType::INT) {
                push_int(ret_val);
            }
        }

        Value *call(Function *f, s64 arg_count, bool on_object) {
            Array<Value *> args;
            for (u16 i = 0; i < arg_count; ++i) {
                args.add(pop_int());
            }

            if (on_object) {
                sp--;
            }

            return irb->CreateCall(f, ArrayRef(args.data, args.length));;
        }

        void convert_field(Field *f) {
            Type *ty = convert_type(f->type);

            String name = clazz->name + to_string(".") + f->name;
            auto var_name = STR_REF(name);
            module->getOrInsertGlobal(var_name, ty);
            auto var = module->getGlobalVariable(var_name);
            var->setInitializer(ConstantInt::get(ty, 0));
        }

        void convert_method(Method *m) {
            Function *fn = convert_function_header(m);
            m->llvm_ref = fn;
            Code ci = find_code(m);
            method = m;

            function_setup(ci);

            u16 i = 0;
            for (auto &arg : fn->args()) {
                if (m->type->parameters[i]->type == NType::INT) {
                    store_int(i, &arg);
                }
                ++i;
            }

            control_flow.offsets.clear();
            control_flow.blocks.clear();

            ip = ci.code;
            while (ip < ci.code + ci.code_length) {
                u8 opcode = fetch_u8();

                u8 inst_type = inst_types[opcode];
                if (inst_type == INST_LABEL || inst_type == INST_LABELW) {
                    get_or_create_block(base_offset());
                }
                switch (opcode) {
                    case OP_BIPUSH:
                    case OP_ILOAD:
                    case OP_ISTORE:
                    case OP_ALOAD:
                    case OP_LDC:
                    case OP_ASTORE:
                    case OP_NEWARRAY: ip++; break;
                    case OP_IF_ICMPEQ: case OP_IF_ICMPNE: case OP_IF_ICMPLT: case OP_IF_ICMPGE:
                    case OP_IF_ICMPGT: case OP_IF_ICMPLE: case OP_IFEQ:
                    case OP_IFNE: case OP_IFLT: case OP_IFGE:
                    case OP_IFGT: case OP_IFLE: case OP_GOTO: {
                        u16 off = fetch_offset();
                        get_or_create_block(off);
                    } break;
                    case OP_GETSTATIC:
                    case OP_INVOKEVIRTUAL:
                    case OP_INVOKESPECIAL:
                    case OP_INVOKESTATIC:
                    case OP_SIPUSH:
                    case OP_IINC:
                    case OP_NEW: ip += 2; break;
                }
            }

            ip = ci.code;
            while (ip < ci.code + ci.code_length) {
                convert_opcode();
            }
        }

        Function *convert_function_header(Method *m) {
            Type *ret_type = convert_type(m->type->return_type);

            Array<Type *> params;
            for (auto pty: m->type->parameters) {
                if (pty->type == NType::INT) {
                    params.add(convert_type(pty));
                }
            }

            String fn_name = m->name;
            if (fn_name != "main") {
                fn_name = clazz->name + to_string(".") + fn_name;
            }

            auto fty = FunctionType::get(ret_type, ArrayRef(params.data, params.length), false);
            auto fn = Function::Create(fty, Function::ExternalLinkage, STR_REF(fn_name), *module);

            BasicBlock *bb = BasicBlock::Create(context, "", fn);
            irb->SetInsertPoint(bb);

            return fn;
        }

        void function_setup(Code ci) {
            stack_int = (Value **) malloc(ci.max_stack * sizeof(Value *));
            locals_int = (Value **) malloc(ci.max_locals * sizeof(Value *));

            stack_object = (JavaObject *) malloc(ci.max_stack * sizeof(JavaArray));
            locals_object = (JavaObject *) malloc(ci.max_locals * sizeof(JavaArray));

            ip = ci.code;
            sp = 0;

            for (u16 i = 0; i < ci.max_stack; ++i) {
                AllocaInst *ia = irb->CreateAlloca(llty_i64, 0, "s_i_" + std::to_string(i));
                stack_int[i] = ia;

                AllocaInst *aa = irb->CreateAlloca(llty_i8_ptr, 0, "s_a_" + std::to_string(i));
                stack_object[i].array.llvm_ref = aa;
            }

            for (u16 i = 0; i < ci.max_locals; ++i) {
                AllocaInst *ia = irb->CreateAlloca(llty_i64, 0, "l_i_" + std::to_string(i));
                locals_int[i] = ia;

                AllocaInst *aa = irb->CreateAlloca(llty_i8_ptr, 0, "l_a_" + std::to_string(i));
                locals_object[i].array.llvm_ref = aa;
            }
        }

        Type *convert_type(NType *type) {
            switch (type->type) {
                case NType::ARRAY:
                    return convert_type(type->element_type)->getPointerTo();
                case NType::CLASS: {
                    if (type->clazz_name == "java/lang/String") {
                        return llty_i8->getPointerTo();
                    }
                    assert(0 && "Class not implemented");
                    return 0;
                }
                case NType::BOOL: return llty_i1;
                case NType::BYTE: return llty_i8;
                case NType::SHORT: return llty_i16;
                case NType::INT: return llty_i32;
                case NType::LONG: return llty_i64;
                case NType::VOID: return llty_void;
            }

            assert(0 && "Type conversion not implemented");
            return 0;
        }

        Type *java_to_llvm_type(u8 type) {
            switch (type) {
                case TYPE_BOOLEAN: return llty_i1;
                case TYPE_CHAR: return llty_i8;
                case TYPE_FLOAT: break;
                case TYPE_DOUBLE: break;
                case TYPE_BYTE: return llty_i8;
                case TYPE_SHORT: return llty_i16;
                case TYPE_INT: return llty_i32;
                case TYPE_LONG: return llty_i64;
            }

            assert(0 && "Type not implemented");
            return 0;
        }

        void create_cond_jump(CmpInst::Predicate op, Value *l, Value * r, u16 off) {
            BasicBlock *after = BasicBlock::Create(context, "", method->llvm_ref);

            BasicBlock *target = get_or_create_block(off);

            Value *cmp = irb->CreateICmp(op, l, r);
            irb->CreateCondBr(cmp, target, after);

            SetInsertBlock(after);
        }

        BasicBlock *get_or_create_block(u16 off) {
            BasicBlock * bb = control_flow.find(off);

            if (!bb) {
                bb = BasicBlock::Create(context, "", method->llvm_ref);
                control_flow.add(off, bb);
            }

            return bb;
        }

        void push_int(Value *val) {
            llvm_store_int(val, stack_int[sp++]);
        }

        Value *pop_int() {
            return load(stack_int[--sp]);
        }

        void store_int(u8 index, Value *value) {
            llvm_store_int(value, locals_int[index]);
        }

        void store_int(u8 index) {
            store_int(index, pop_int());
        }

        void load_int(u8 index) {
            push_int(load_local_int(index));
        }

        Value *load_local_int(u8 index) {
            return load(locals_int[index]);
        }
        
        /* converts between different int types automatically */
        void llvm_store_int(Value *val, Value *ptr) {
            Type *ptr_el_ty = ptr->getType()->getPointerElementType();

            irb->CreateStore(irb->CreateIntCast(val, ptr_el_ty, true), ptr);
        }

        void push_array(Value *ptr, u8 type, Value *length) {
            JavaObject *obj = &stack_object[sp++];
            JavaArray *arr = &obj->array;

            obj->type = JavaObject::ARRAY;
            arr->type = type;
            arr->length = length;

            irb->CreateStore(ptr, arr->llvm_ref);
        }

        JavaArray *pop_array() {
            return &stack_object[--sp].array;
        }

        void store_array(u8 index, Value *ptr, u8 type, Value *length) {
            JavaObject *obj = &locals_object[index];
            JavaArray *arr = &obj->array;

            obj->type = JavaObject::ARRAY;
            arr->type = type;
            arr->length = length;

            irb->CreateStore(ptr, arr->llvm_ref);
        }

        void store_array(u8 index) {
            JavaArray *arr = pop_array();
            store_array(index, load(arr->llvm_ref), arr->type, arr->length);
        }

        void load_array(u8 index) {
            JavaArray *arr = &locals_object[index].array;
            push_array(load(arr->llvm_ref), arr->type, arr->length);
        }

        void optimize() {
            legacy::PassManager *pm = new legacy::PassManager();
            PassManagerBuilder pmb;
            pmb.OptLevel = 2;
            pmb.SizeLevel = 0;
            pmb.DisableUnrollLoops = false;
            pmb.LoopVectorize = true;
            pmb.SLPVectorize = true;
            pmb.populateModulePassManager(*pm);
            pm->run(*module);
        }

        Value *make_int(s64 v) {
            return ConstantInt::get(llty_i64, v);
        }

        Value *gep(llvm::Value *ptr, ArrayRef<Value *> idx_list) {
            Value *inst = irb->CreateInBoundsGEP(ptr->getType()->getPointerElementType(), ptr, idx_list);
            return inst;
        }

        Value *load(Value *value) {
            Type *ty = value->getType()->getPointerElementType();
            LoadInst *load = irb->CreateLoad(ty, value);
            return load;
        }
    };
}