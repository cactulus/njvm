#define STR_REF(x) llvm::StringRef((const char * ) x.data, x.length)

struct JIT_Call_Frame {
    Class *clazz;
    Method *method;
    llvm::AllocaInst **stack;
    llvm::AllocaInst **locals;
    u8 *ip;
    u8 sp;
};

struct NJIT {
    llvm::LLVMContext context;
    llvm::Module *module;
    llvm::IRBuilder<> *irb;
    llvm::AllocaInst **stack;
    llvm::AllocaInst **locals;
    Class *clazz;
    Method *method;
    u8 *ip;
    u8 sp;

    llvm::Type *type_i32;

    NJIT(Class *main_clazz) {
        this->clazz = main_clazz;

        module = new llvm::Module("njit", context);
        irb = new llvm::IRBuilder<>(context);

        type_i32 = llvm::Type::getInt32Ty(context);
    }

    void run() {
        Method *main_method = find_method("main");
        convert_main_method(main_method);

        module->print(llvm::outs(), 0);
    }

    llvm::Function *convert_main_method(Method *m) {
        Array<llvm::Type *> params;
        auto fty = llvm::FunctionType::get(type_i32, llvm::ArrayRef(params.data, params.length), false);
        auto fn = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, "main", module);

        llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "", fn);
        irb->SetInsertPoint(bb);

        Code ci = find_code(m);
        method = m;

        stack = (llvm::AllocaInst **) malloc(ci.max_stack * sizeof(llvm::AllocaInst *));
        locals = (llvm::AllocaInst **) malloc(ci.max_locals * sizeof(llvm::AllocaInst *));
        ip = ci.code;
        sp = 0;

        bool sbreak = false;
        while (!sbreak) {
            sbreak = execute();
        }

        irb->CreateRet(llvm::ConstantInt::get(type_i32, 0));

        return fn;
    }

    llvm::Function *convert_method(Method *m) {
        llvm::Function *fn = convert_function_header(m);

        Code ci = find_code(m);
        method = m;

        JIT_Call_Frame frame = save_frame();

        stack = (llvm::AllocaInst **) malloc(ci.max_stack * sizeof(llvm::AllocaInst *));
        locals = (llvm::AllocaInst **) malloc(ci.max_locals * sizeof(llvm::AllocaInst *));
        ip = ci.code;
        sp = 0;

        for (u16 i = 0; i < ci.max_stack; ++i) {
            llvm::AllocaInst *a = irb->CreateAlloca(type_i32, 0, "stack_" + std::to_string(i));
            stack[i] = a;
        }

        for (u16 i = 0; i < ci.max_locals; ++i) {
            llvm::AllocaInst *a = irb->CreateAlloca(type_i32, 0, "local_" + std::to_string(i));
            locals[i] = a;
        }

        u8 par_count = method->type->parameters.length;

        for (int k = 0; k < par_count; ++k) {

        }

        bool sbreak = false;
        while (!sbreak) {
            sbreak = execute();
        }

        // return value
        if (m->type->return_type->type != Type::VOID) {

        }

        restore_frame(frame);

        return fn;
    }

    bool execute() {
        return true;
    }

    llvm::Function *convert_function_header(Method *m) {
        llvm::Type *ret_type = convert_type(m->type->return_type);

        Array<llvm::Type *> params;
        for (auto pty : m->type->parameters) {
            params.add(convert_type(pty));
        }

        auto fty = llvm::FunctionType::get(ret_type, llvm::ArrayRef(params.data, params.length), false);
        auto fn = llvm::Function::Create(fty, llvm::Function::PrivateLinkage, STR_REF(m->name), module);

        llvm::BasicBlock *bb = llvm::BasicBlock::Create(context, "",fn);
        irb->SetInsertPoint(bb);
        return fn;
    }

    llvm::Type *convert_type(Type *type) {
        switch (type->type) {
            case Type::INT:
                return type_i32;
        }

        assert(0 && "Type conversion not implemented");
        return 0;
    }

    JIT_Call_Frame save_frame() {
        JIT_Call_Frame frame;

        frame.clazz = clazz;
        frame.method = method;
        frame.stack = stack;
        frame.locals = locals;
        frame.ip = ip;
        frame.sp = sp;

        return frame;
    }

    void restore_frame(JIT_Call_Frame frame) {
        delete[] stack;
        delete[] locals;

        stack = frame.stack;
        locals = frame.locals;
        ip = frame.ip;
        sp = frame.sp;
        method = frame.method;
        clazz = frame.clazz;
    }

    /* all of the below functions are duplicates from njvm, maybe remove later */

    Method *find_method(String class_name, String name) {
        if (class_name != clazz->name) {
            return 0;
        }

        for (u16 i = 0; i < clazz->methods_count; ++i) {
            Method *m = &clazz->methods[i];
            if (m->name == name) {
                return m;
            }
        }

        return 0;
    }

    Method *find_method(String name) {
        for (u16 i = 0; i < clazz->methods_count; ++i) {
            Method *m = &clazz->methods[i];
            if (m->name == name) {
                return m;
            }
        }

        printf("Can't find function %.*s\n", name.length, name.data);
        return 0;
    }

    /* Finds class in current class only */
    Method *find_method(const char *mname) {
        for (u16 i = 0; i < clazz->methods_count; ++i) {
            Method *m = &clazz->methods[i];

            if (m->name == mname) {
                return m;
            }
        }

        printf("Can't find method '%s'\n", mname);
        return 0;
    }

    Code find_code(Method *m) {
        Code info;

        for (u16 i = 0; i < m->attributes_count; ++i) {
            Attribute *a = &m->attributes[i];
            if (a->name == "Code") {
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
        return clazz->constant_pool[index - 1];
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