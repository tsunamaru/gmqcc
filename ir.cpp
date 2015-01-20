#include <stdlib.h>
#include <string.h>

#include "gmqcc.h"
#include "ir.h"

/***********************************************************************
 * Type sizes used at multiple points in the IR codegen
 */

const char *type_name[TYPE_COUNT] = {
    "void",
    "string",
    "float",
    "vector",
    "entity",
    "field",
    "function",
    "pointer",
    "integer",
    "variant",
    "struct",
    "union",
    "array",

    "nil",
    "<no-expression>"
};

static size_t type_sizeof_[TYPE_COUNT] = {
    1, /* TYPE_VOID     */
    1, /* TYPE_STRING   */
    1, /* TYPE_FLOAT    */
    3, /* TYPE_VECTOR   */
    1, /* TYPE_ENTITY   */
    1, /* TYPE_FIELD    */
    1, /* TYPE_FUNCTION */
    1, /* TYPE_POINTER  */
    1, /* TYPE_INTEGER  */
    3, /* TYPE_VARIANT  */
    0, /* TYPE_STRUCT   */
    0, /* TYPE_UNION    */
    0, /* TYPE_ARRAY    */
    0, /* TYPE_NIL      */
    0, /* TYPE_NOESPR   */
};

const uint16_t type_store_instr[TYPE_COUNT] = {
    INSTR_STORE_F, /* should use I when having integer support */
    INSTR_STORE_S,
    INSTR_STORE_F,
    INSTR_STORE_V,
    INSTR_STORE_ENT,
    INSTR_STORE_FLD,
    INSTR_STORE_FNC,
    INSTR_STORE_ENT, /* should use I */
#if 0
    INSTR_STORE_I, /* integer type */
#else
    INSTR_STORE_F,
#endif

    INSTR_STORE_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t field_store_instr[TYPE_COUNT] = {
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
    INSTR_STORE_V,
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
#if 0
    INSTR_STORE_FLD, /* integer type */
#else
    INSTR_STORE_FLD,
#endif

    INSTR_STORE_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t type_storep_instr[TYPE_COUNT] = {
    INSTR_STOREP_F, /* should use I when having integer support */
    INSTR_STOREP_S,
    INSTR_STOREP_F,
    INSTR_STOREP_V,
    INSTR_STOREP_ENT,
    INSTR_STOREP_FLD,
    INSTR_STOREP_FNC,
    INSTR_STOREP_ENT, /* should use I */
#if 0
    INSTR_STOREP_ENT, /* integer type */
#else
    INSTR_STOREP_F,
#endif

    INSTR_STOREP_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t type_eq_instr[TYPE_COUNT] = {
    INSTR_EQ_F, /* should use I when having integer support */
    INSTR_EQ_S,
    INSTR_EQ_F,
    INSTR_EQ_V,
    INSTR_EQ_E,
    INSTR_EQ_E, /* FLD has no comparison */
    INSTR_EQ_FNC,
    INSTR_EQ_E, /* should use I */
#if 0
    INSTR_EQ_I,
#else
    INSTR_EQ_F,
#endif

    INSTR_EQ_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t type_ne_instr[TYPE_COUNT] = {
    INSTR_NE_F, /* should use I when having integer support */
    INSTR_NE_S,
    INSTR_NE_F,
    INSTR_NE_V,
    INSTR_NE_E,
    INSTR_NE_E, /* FLD has no comparison */
    INSTR_NE_FNC,
    INSTR_NE_E, /* should use I */
#if 0
    INSTR_NE_I,
#else
    INSTR_NE_F,
#endif

    INSTR_NE_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t type_not_instr[TYPE_COUNT] = {
    INSTR_NOT_F, /* should use I when having integer support */
    VINSTR_END,  /* not to be used, depends on string related -f flags */
    INSTR_NOT_F,
    INSTR_NOT_V,
    INSTR_NOT_ENT,
    INSTR_NOT_ENT,
    INSTR_NOT_FNC,
    INSTR_NOT_ENT, /* should use I */
#if 0
    INSTR_NOT_I, /* integer type */
#else
    INSTR_NOT_F,
#endif

    INSTR_NOT_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

/* protos */
static void            ir_value_dump(ir_value*, int (*oprintf)(const char*,...));

static ir_value*       ir_gen_extparam_proto(ir_builder *ir);
static void            ir_gen_extparam      (ir_builder *ir);

static void            ir_function_dump(ir_function*, char *ind, int (*oprintf)(const char*,...));

static ir_value*       ir_block_create_general_instr(ir_block *self, lex_ctx_t, const char *label,
                                                     int op, ir_value *a, ir_value *b, qc_type outype);
static bool GMQCC_WARN ir_block_create_store(ir_block*, lex_ctx_t, ir_value *target, ir_value *what);
static void            ir_block_dump(ir_block*, char *ind, int (*oprintf)(const char*,...));

static bool            ir_instr_op(ir_instr*, int op, ir_value *value, bool writing);
static void            ir_instr_delete(ir_instr*);
static void            ir_instr_dump(ir_instr* in, char *ind, int (*oprintf)(const char*,...));
/* error functions */

static void irerror(lex_ctx_t ctx, const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    con_cvprintmsg(ctx, LVL_ERROR, "internal error", msg, ap);
    va_end(ap);
}

static bool GMQCC_WARN irwarning(lex_ctx_t ctx, int warntype, const char *fmt, ...)
{
    bool    r;
    va_list ap;
    va_start(ap, fmt);
    r = vcompile_warning(ctx, warntype, fmt, ap);
    va_end(ap);
    return r;
}

/***********************************************************************
 * Vector utility functions
 */

static bool GMQCC_WARN vec_ir_value_find(std::vector<ir_value *> &vec, const ir_value *what, size_t *idx)
{
    for (auto &it : vec) {
        if (it != what)
            continue;
        if (idx)
            *idx = &it - &vec[0];
        return true;
    }
    return false;
}

static bool GMQCC_WARN vec_ir_block_find(ir_block **vec, ir_block *what, size_t *idx)
{
    size_t i;
    size_t len = vec_size(vec);
    for (i = 0; i < len; ++i) {
        if (vec[i] == what) {
            if (idx) *idx = i;
            return true;
        }
    }
    return false;
}

static bool GMQCC_WARN vec_ir_instr_find(std::vector<ir_instr *> &vec, ir_instr *what, size_t *idx)
{
    for (auto &it : vec) {
        if (it != what)
            continue;
        if (idx)
            *idx = &it - &vec[0];
        return true;
    }
    return false;
}

/***********************************************************************
 * IR Builder
 */

static void ir_block_delete_quick(ir_block* self);
static void ir_instr_delete_quick(ir_instr *self);
static void ir_function_delete_quick(ir_function *self);

void* ir_builder::operator new(std::size_t bytes)
{
    return mem_a(bytes);
}

void ir_builder::operator delete(void *ptr)
{
    mem_d(ptr);
}

ir_builder::ir_builder(const std::string& modulename)
: name(modulename)
{
    htglobals   = util_htnew(IR_HT_SIZE);
    htfields    = util_htnew(IR_HT_SIZE);
    htfunctions = util_htnew(IR_HT_SIZE);

    nil = new ir_value("nil", store_value, TYPE_NIL);
    nil->cvq = CV_CONST;

    for (size_t i = 0; i != IR_MAX_VINSTR_TEMPS; ++i) {
        /* we write to them, but they're not supposed to be used outside the IR, so
         * let's not allow the generation of ir_instrs which use these.
         * So it's a constant noexpr.
         */
        vinstr_temp[i] = new ir_value("vinstr_temp", store_value, TYPE_NOEXPR);
        vinstr_temp[i]->cvq = CV_CONST;
    }

    code = code_init();
}

ir_builder::~ir_builder()
{
    util_htdel(htglobals);
    util_htdel(htfields);
    util_htdel(htfunctions);
    for (auto& f : functions)
        ir_function_delete_quick(f.release());
    functions.clear(); // delete them now before deleting the rest:

    delete nil;

    for (size_t i = 0; i != IR_MAX_VINSTR_TEMPS; ++i) {
        delete vinstr_temp[i];
    }

    code_cleanup(code);
}

static ir_function* ir_builder_get_function(ir_builder *self, const char *name)
{
    return (ir_function*)util_htget(self->htfunctions, name);
}

ir_function* ir_builder_create_function(ir_builder *self, const std::string& name, qc_type outtype)
{
    ir_function *fn = ir_builder_get_function(self, name.c_str());
    if (fn) {
        return nullptr;
    }

    fn = new ir_function(self, outtype);
    fn->name = name;
    self->functions.emplace_back(fn);
    util_htset(self->htfunctions, name.c_str(), fn);

    fn->value = ir_builder_create_global(self, fn->name, TYPE_FUNCTION);
    if (!fn->value) {
        delete fn;
        return nullptr;
    }

    fn->value->hasvalue = true;
    fn->value->outtype = outtype;
    fn->value->constval.vfunc = fn;
    fn->value->context = fn->context;

    return fn;
}

static ir_value* ir_builder_get_global(ir_builder *self, const char *name)
{
    return (ir_value*)util_htget(self->htglobals, name);
}

ir_value* ir_builder_create_global(ir_builder *self, const std::string& name, qc_type vtype)
{
    ir_value *ve;

    if (name[0] != '#')
    {
        ve = ir_builder_get_global(self, name.c_str());
        if (ve) {
            return nullptr;
        }
    }

    ve = new ir_value(std::string(name), store_global, vtype);
    self->globals.emplace_back(ve);
    util_htset(self->htglobals, name.c_str(), ve);
    return ve;
}

ir_value* ir_builder_get_va_count(ir_builder *self)
{
    if (self->reserved_va_count)
        return self->reserved_va_count;
    return (self->reserved_va_count = ir_builder_create_global(self, "reserved:va_count", TYPE_FLOAT));
}

static ir_value* ir_builder_get_field(ir_builder *self, const char *name)
{
    return (ir_value*)util_htget(self->htfields, name);
}


ir_value* ir_builder_create_field(ir_builder *self, const std::string& name, qc_type vtype)
{
    ir_value *ve = ir_builder_get_field(self, name.c_str());
    if (ve) {
        return nullptr;
    }

    ve = new ir_value(std::string(name), store_global, TYPE_FIELD);
    ve->fieldtype = vtype;
    self->fields.emplace_back(ve);
    util_htset(self->htfields, name.c_str(), ve);
    return ve;
}

/***********************************************************************
 *IR Function
 */

static bool ir_function_naive_phi(ir_function*);
static void ir_function_enumerate(ir_function*);
static bool ir_function_calculate_liferanges(ir_function*);
static bool ir_function_allocate_locals(ir_function*);

void* ir_function::operator new(std::size_t bytes)
{
    return mem_a(bytes);
}

void ir_function::operator delete(void *ptr)
{
    mem_d(ptr);
}

ir_function::ir_function(ir_builder* owner_, qc_type outtype_)
: owner(owner_),
  name("<@unnamed>"),
  outtype(outtype_)
{
    owner = owner;
    context.file = "<@no context>";
    context.line = 0;
    outtype = outtype;
}

ir_function::~ir_function()
{
}

static void ir_function_delete_quick(ir_function *self)
{
    for (auto& b : self->blocks)
        ir_block_delete_quick(b.release());
    delete self;
}

static void ir_function_collect_value(ir_function *self, ir_value *v)
{
    self->values.emplace_back(v);
}

ir_block* ir_function_create_block(lex_ctx_t ctx, ir_function *self, const char *label)
{
    ir_block* bn = new ir_block(self, label ? std::string(label) : std::string());
    bn->context = ctx;
    self->blocks.emplace_back(bn);

    if ((self->flags & IR_FLAG_BLOCK_COVERAGE) && self->owner->coverage_func)
        (void)ir_block_create_call(bn, ctx, nullptr, self->owner->coverage_func, false);

    return bn;
}

static bool instr_is_operation(uint16_t op)
{
    return ( (op >= INSTR_MUL_F  && op <= INSTR_GT) ||
             (op >= INSTR_LOAD_F && op <= INSTR_LOAD_FNC) ||
             (op == INSTR_ADDRESS) ||
             (op >= INSTR_NOT_F  && op <= INSTR_NOT_FNC) ||
             (op >= INSTR_AND    && op <= INSTR_BITOR) ||
             (op >= INSTR_CALL0  && op <= INSTR_CALL8) ||
             (op >= VINSTR_BITAND_V && op <= VINSTR_NEG_V) );
}

static bool ir_function_pass_peephole(ir_function *self)
{
    for (auto& bp : self->blocks) {
        ir_block *block = bp.get();
        for (size_t i = 0; i < vec_size(block->instr); ++i) {
            ir_instr *inst;
            inst = block->instr[i];

            if (i >= 1 &&
                (inst->opcode >= INSTR_STORE_F &&
                 inst->opcode <= INSTR_STORE_FNC))
            {
                ir_instr *store;
                ir_instr *oper;
                ir_value *value;

                store = inst;

                oper  = block->instr[i-1];
                if (!instr_is_operation(oper->opcode))
                    continue;

                /* Don't change semantics of MUL_VF in engines where these may not alias. */
                if (OPTS_FLAG(LEGACY_VECTOR_MATHS)) {
                    if (oper->opcode == INSTR_MUL_VF && oper->_ops[2]->memberof == oper->_ops[1])
                        continue;
                    if (oper->opcode == INSTR_MUL_FV && oper->_ops[1]->memberof == oper->_ops[2])
                        continue;
                }

                value = oper->_ops[0];

                /* only do it for SSA values */
                if (value->store != store_value)
                    continue;

                /* don't optimize out the temp if it's used later again */
                if (value->reads.size() != 1)
                    continue;

                /* The very next store must use this value */
                if (value->reads[0] != store)
                    continue;

                /* And of course the store must _read_ from it, so it's in
                 * OP 1 */
                if (store->_ops[1] != value)
                    continue;

                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                (void)!ir_instr_op(oper, 0, store->_ops[0], true);

                vec_remove(block->instr, i, 1);
                ir_instr_delete(store);
            }
            else if (inst->opcode == VINSTR_COND)
            {
                /* COND on a value resulting from a NOT could
                 * remove the NOT and swap its operands
                 */
                while (true) {
                    ir_block *tmp;
                    size_t    inotid;
                    ir_instr *inot;
                    ir_value *value;
                    value = inst->_ops[0];

                    if (value->store != store_value || value->reads.size() != 1 || value->reads[0] != inst)
                        break;

                    inot = value->writes[0];
                    if (inot->_ops[0] != value ||
                        inot->opcode < INSTR_NOT_F ||
                        inot->opcode > INSTR_NOT_FNC ||
                        inot->opcode == INSTR_NOT_V || /* can't do these */
                        inot->opcode == INSTR_NOT_S)
                    {
                        break;
                    }

                    /* count */
                    ++opts_optimizationcount[OPTIM_PEEPHOLE];
                    /* change operand */
                    (void)!ir_instr_op(inst, 0, inot->_ops[1], false);
                    /* remove NOT */
                    tmp = inot->owner;
                    for (inotid = 0; inotid < vec_size(tmp->instr); ++inotid) {
                        if (tmp->instr[inotid] == inot)
                            break;
                    }
                    if (inotid >= vec_size(tmp->instr)) {
                        compile_error(inst->context, "sanity-check failed: failed to find instruction to optimize out");
                        return false;
                    }
                    vec_remove(tmp->instr, inotid, 1);
                    ir_instr_delete(inot);
                    /* swap ontrue/onfalse */
                    tmp = inst->bops[0];
                    inst->bops[0] = inst->bops[1];
                    inst->bops[1] = tmp;
                }
                continue;
            }
        }
    }

    return true;
}

static bool ir_function_pass_tailrecursion(ir_function *self)
{
    size_t p;

    for (auto& bp : self->blocks) {
        ir_block *block = bp.get();

        ir_value *funcval;
        ir_instr *ret, *call, *store = nullptr;

        if (!block->final || vec_size(block->instr) < 2)
            continue;

        ret = block->instr[vec_size(block->instr)-1];
        if (ret->opcode != INSTR_DONE && ret->opcode != INSTR_RETURN)
            continue;

        call = block->instr[vec_size(block->instr)-2];
        if (call->opcode >= INSTR_STORE_F && call->opcode <= INSTR_STORE_FNC) {
            /* account for the unoptimized
             * CALL
             * STORE %return, %tmp
             * RETURN %tmp
             * version
             */
            if (vec_size(block->instr) < 3)
                continue;

            store = call;
            call = block->instr[vec_size(block->instr)-3];
        }

        if (call->opcode < INSTR_CALL0 || call->opcode > INSTR_CALL8)
            continue;

        if (store) {
            /* optimize out the STORE */
            if (ret->_ops[0]   &&
                ret->_ops[0]   == store->_ops[0] &&
                store->_ops[1] == call->_ops[0])
            {
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                call->_ops[0] = store->_ops[0];
                vec_remove(block->instr, vec_size(block->instr) - 2, 1);
                ir_instr_delete(store);
            }
            else
                continue;
        }

        if (!call->_ops[0])
            continue;

        funcval = call->_ops[1];
        if (!funcval)
            continue;
        if (funcval->vtype != TYPE_FUNCTION || funcval->constval.vfunc != self)
            continue;

        /* now we have a CALL and a RET, check if it's a tailcall */
        if (ret->_ops[0] && call->_ops[0] != ret->_ops[0])
            continue;

        ++opts_optimizationcount[OPTIM_TAIL_RECURSION];
        vec_shrinkby(block->instr, 2);

        block->final = false; /* open it back up */

        /* emite parameter-stores */
        for (p = 0; p < call->params.size(); ++p) {
            /* assert(call->params_count <= self->locals_count); */
            if (!ir_block_create_store(block, call->context, self->locals[p].get(), call->params[p])) {
                irerror(call->context, "failed to create tailcall store instruction for parameter %i", (int)p);
                return false;
            }
        }
        if (!ir_block_create_jump(block, call->context, self->blocks[0].get())) {
            irerror(call->context, "failed to create tailcall jump");
            return false;
        }

        ir_instr_delete(call);
        ir_instr_delete(ret);
    }

    return true;
}

bool ir_function_finalize(ir_function *self)
{
    if (self->builtin)
        return true;

    if (OPTS_OPTIMIZATION(OPTIM_PEEPHOLE)) {
        if (!ir_function_pass_peephole(self)) {
            irerror(self->context, "generic optimization pass broke something in `%s`", self->name.c_str());
            return false;
        }
    }

    if (OPTS_OPTIMIZATION(OPTIM_TAIL_RECURSION)) {
        if (!ir_function_pass_tailrecursion(self)) {
            irerror(self->context, "tail-recursion optimization pass broke something in `%s`", self->name.c_str());
            return false;
        }
    }

    if (!ir_function_naive_phi(self)) {
        irerror(self->context, "internal error: ir_function_naive_phi failed");
        return false;
    }

    for (auto& lp : self->locals) {
        ir_value *v = lp.get();
        if (v->vtype == TYPE_VECTOR ||
            (v->vtype == TYPE_FIELD && v->outtype == TYPE_VECTOR))
        {
            ir_value_vector_member(v, 0);
            ir_value_vector_member(v, 1);
            ir_value_vector_member(v, 2);
        }
    }
    for (auto& vp : self->values) {
        ir_value *v = vp.get();
        if (v->vtype == TYPE_VECTOR ||
            (v->vtype == TYPE_FIELD && v->outtype == TYPE_VECTOR))
        {
            ir_value_vector_member(v, 0);
            ir_value_vector_member(v, 1);
            ir_value_vector_member(v, 2);
        }
    }

    ir_function_enumerate(self);

    if (!ir_function_calculate_liferanges(self))
        return false;
    if (!ir_function_allocate_locals(self))
        return false;
    return true;
}

ir_value* ir_function_create_local(ir_function *self, const std::string& name, qc_type vtype, bool param)
{
    ir_value *ve;

    if (param &&
        !self->locals.empty() &&
        self->locals.back()->store != store_param)
    {
        irerror(self->context, "cannot add parameters after adding locals");
        return nullptr;
    }

    ve = new ir_value(std::string(name), (param ? store_param : store_local), vtype);
    if (param)
        ve->locked = true;
    self->locals.emplace_back(ve);
    return ve;
}

/***********************************************************************
 *IR Block
 */

void* ir_block::operator new(std::size_t bytes) {
  return mem_a(bytes);
}

void ir_block::operator delete(void *data) {
    mem_d(data);
}

ir_block::ir_block(ir_function* owner, const std::string& name)
: owner(owner),
  label(name)
{
    context.file = "<@no context>";
    context.line = 0;
}

ir_block::~ir_block()
{
    for (size_t i = 0; i != vec_size(instr); ++i)
        ir_instr_delete(instr[i]);
    vec_free(instr);
    vec_free(entries);
    vec_free(exits);
}

static void ir_block_delete_quick(ir_block* self)
{
    size_t i;
    for (i = 0; i != vec_size(self->instr); ++i)
        ir_instr_delete_quick(self->instr[i]);
    vec_free(self->instr);
    delete self;
}

/***********************************************************************
 *IR Instructions
 */

static ir_instr* ir_instr_new(lex_ctx_t ctx, ir_block* owner, int op)
{
    ir_instr *self = new ir_instr;
    self->owner = owner;
    self->context = ctx;
    self->opcode = op;
    self->_ops[0] = nullptr;
    self->_ops[1] = nullptr;
    self->_ops[2] = nullptr;
    self->bops[0] = nullptr;
    self->bops[1] = nullptr;
    self->eid = 0;
    self->likely = true;
    return self;
}

static void ir_instr_delete_quick(ir_instr *self)
{
    delete self;
}

static void ir_instr_delete(ir_instr *self)
{
    /* The following calls can only delete from
     * vectors, we still want to delete this instruction
     * so ignore the return value. Since with the warn_unused_result attribute
     * gcc doesn't care about an explicit: (void)foo(); to ignore the result,
     * I have to improvise here and use if(foo());
     */
    for (auto &it : self->phi) {
        size_t idx;
        if (vec_ir_instr_find(it.value->writes, self, &idx))
            it.value->writes.erase(it.value->writes.begin() + idx);
        if (vec_ir_instr_find(it.value->reads, self, &idx))
            it.value->reads.erase(it.value->reads.begin() + idx);
    }
    for (auto &it : self->params) {
        size_t idx;
        if (vec_ir_instr_find(it->writes, self, &idx))
            it->writes.erase(it->writes.begin() + idx);
        if (vec_ir_instr_find(it->reads, self, &idx))
            it->reads.erase(it->reads.begin() + idx);
    }
    (void)!ir_instr_op(self, 0, nullptr, false);
    (void)!ir_instr_op(self, 1, nullptr, false);
    (void)!ir_instr_op(self, 2, nullptr, false);
    mem_d(self);
}

static bool ir_instr_op(ir_instr *self, int op, ir_value *v, bool writing)
{
    if (v && v->vtype == TYPE_NOEXPR) {
        irerror(self->context, "tried to use a NOEXPR value");
        return false;
    }

    if (self->_ops[op]) {
        size_t idx;
        if (writing && vec_ir_instr_find(self->_ops[op]->writes, self, &idx))
            self->_ops[op]->writes.erase(self->_ops[op]->writes.begin() + idx);
        else if (vec_ir_instr_find(self->_ops[op]->reads, self, &idx))
            self->_ops[op]->reads.erase(self->_ops[op]->reads.begin() + idx);
    }
    if (v) {
        if (writing)
            v->writes.push_back(self);
        else
            v->reads.push_back(self);
    }
    self->_ops[op] = v;
    return true;
}

/***********************************************************************
 *IR Value
 */

static void ir_value_code_setaddr(ir_value *self, int32_t gaddr)
{
    self->code.globaladdr = gaddr;
    if (self->members[0]) self->members[0]->code.globaladdr = gaddr;
    if (self->members[1]) self->members[1]->code.globaladdr = gaddr;
    if (self->members[2]) self->members[2]->code.globaladdr = gaddr;
}

static int32_t ir_value_code_addr(const ir_value *self)
{
    if (self->store == store_return)
        return OFS_RETURN + self->code.addroffset;
    return self->code.globaladdr + self->code.addroffset;
}

void* ir_value::operator new(std::size_t bytes) {
  return mem_a(bytes);
}

void ir_value::operator delete(void *data) {
    mem_d(data);
}

ir_value::ir_value(std::string&& name_, store_type store_, qc_type vtype_)
: name(move(name_)),
  vtype(vtype_),
  store(store_)
{
    fieldtype = TYPE_VOID;
    outtype = TYPE_VOID;
    flags = 0;

    cvq          = CV_NONE;
    hasvalue     = false;
    context.file = "<@no context>";
    context.line = 0;

    memset(&constval, 0, sizeof(constval));
    memset(&code,     0, sizeof(code));

    members[0] = nullptr;
    members[1] = nullptr;
    members[2] = nullptr;
    memberof = nullptr;

    unique_life = false;
    locked = false;
    callparam  = false;
}

ir_value::~ir_value()
{
    size_t i;
    if (hasvalue) {
        if (vtype == TYPE_STRING)
            mem_d((void*)constval.vstring);
    }
    if (!(flags & IR_FLAG_SPLIT_VECTOR)) {
        for (i = 0; i < 3; ++i) {
            if (members[i])
                delete members[i];
        }
    }
}


/*  helper function */
static ir_value* ir_builder_imm_float(ir_builder *self, float value, bool add_to_list) {
    ir_value *v = new ir_value("#IMMEDIATE", store_global, TYPE_FLOAT);
    v->flags |= IR_FLAG_ERASABLE;
    v->hasvalue = true;
    v->cvq = CV_CONST;
    v->constval.vfloat = value;

    self->globals.emplace_back(v);
    if (add_to_list)
        self->const_floats.emplace_back(v);
    return v;
}

ir_value* ir_value_vector_member(ir_value *self, unsigned int member)
{
    std::string name;
    ir_value *m;
    if (member >= 3)
        return nullptr;

    if (self->members[member])
        return self->members[member];

    if (!self->name.empty()) {
        char member_name[3] = { '_', char('x' + member), 0 };
        name = self->name + member_name;
    }

    if (self->vtype == TYPE_VECTOR)
    {
        m = new ir_value(move(name), self->store, TYPE_FLOAT);
        if (!m)
            return nullptr;
        m->context = self->context;

        self->members[member] = m;
        m->code.addroffset = member;
    }
    else if (self->vtype == TYPE_FIELD)
    {
        if (self->fieldtype != TYPE_VECTOR)
            return nullptr;
        m = new ir_value(move(name), self->store, TYPE_FIELD);
        if (!m)
            return nullptr;
        m->fieldtype = TYPE_FLOAT;
        m->context = self->context;

        self->members[member] = m;
        m->code.addroffset = member;
    }
    else
    {
        irerror(self->context, "invalid member access on %s", self->name.c_str());
        return nullptr;
    }

    m->memberof = self;
    return m;
}

static GMQCC_INLINE size_t ir_value_sizeof(const ir_value *self)
{
    if (self->vtype == TYPE_FIELD && self->fieldtype == TYPE_VECTOR)
        return type_sizeof_[TYPE_VECTOR];
    return type_sizeof_[self->vtype];
}

static ir_value* ir_value_out(ir_function *owner, const char *name, store_type storetype, qc_type vtype)
{
    ir_value *v = new ir_value(name ? std::string(name) : std::string(), storetype, vtype);
    if (!v)
        return nullptr;
    ir_function_collect_value(owner, v);
    return v;
}

bool ir_value_set_float(ir_value *self, float f)
{
    if (self->vtype != TYPE_FLOAT)
        return false;
    self->constval.vfloat = f;
    self->hasvalue = true;
    return true;
}

bool ir_value_set_func(ir_value *self, int f)
{
    if (self->vtype != TYPE_FUNCTION)
        return false;
    self->constval.vint = f;
    self->hasvalue = true;
    return true;
}

bool ir_value_set_vector(ir_value *self, vec3_t v)
{
    if (self->vtype != TYPE_VECTOR)
        return false;
    self->constval.vvec = v;
    self->hasvalue = true;
    return true;
}

bool ir_value_set_field(ir_value *self, ir_value *fld)
{
    if (self->vtype != TYPE_FIELD)
        return false;
    self->constval.vpointer = fld;
    self->hasvalue = true;
    return true;
}

bool ir_value_set_string(ir_value *self, const char *str)
{
    if (self->vtype != TYPE_STRING)
        return false;
    self->constval.vstring = util_strdupe(str);
    self->hasvalue = true;
    return true;
}

#if 0
bool ir_value_set_int(ir_value *self, int i)
{
    if (self->vtype != TYPE_INTEGER)
        return false;
    self->constval.vint = i;
    self->hasvalue = true;
    return true;
}
#endif

bool ir_value_lives(ir_value *self, size_t at)
{
    for (auto& l : self->life) {
        if (l.start <= at && at <= l.end)
            return true;
        if (l.start > at) /* since it's ordered */
            return false;
    }
    return false;
}

static bool ir_value_life_insert(ir_value *self, size_t idx, ir_life_entry_t e)
{
    self->life.insert(self->life.begin() + idx, e);
    return true;
}

static bool ir_value_life_merge(ir_value *self, size_t s)
{
    size_t i;
    const size_t vs = self->life.size();
    ir_life_entry_t *life_found = nullptr;
    ir_life_entry_t *before = nullptr;
    ir_life_entry_t new_entry;

    /* Find the first range >= s */
    for (i = 0; i < vs; ++i)
    {
        before = life_found;
        life_found = &self->life[i];
        if (life_found->start > s)
            break;
    }
    /* nothing found? append */
    if (i == vs) {
        ir_life_entry_t e;
        if (life_found && life_found->end+1 == s)
        {
            /* previous life range can be merged in */
            life_found->end++;
            return true;
        }
        if (life_found && life_found->end >= s)
            return false;
        e.start = e.end = s;
        self->life.emplace_back(e);
        return true;
    }
    /* found */
    if (before)
    {
        if (before->end + 1 == s &&
            life_found->start - 1 == s)
        {
            /* merge */
            before->end = life_found->end;
            self->life.erase(self->life.begin()+1);
            return true;
        }
        if (before->end + 1 == s)
        {
            /* extend before */
            before->end++;
            return true;
        }
        /* already contained */
        if (before->end >= s)
            return false;
    }
    /* extend */
    if (life_found->start - 1 == s)
    {
        life_found->start--;
        return true;
    }
    /* insert a new entry */
    new_entry.start = new_entry.end = s;
    return ir_value_life_insert(self, i, new_entry);
}

static bool ir_value_life_merge_into(ir_value *self, const ir_value *other)
{
    size_t i, myi;

    if (other->life.empty())
        return true;

    if (self->life.empty()) {
        self->life = other->life;
        return true;
    }

    myi = 0;
    for (i = 0; i < other->life.size(); ++i)
    {
        const ir_life_entry_t &otherlife = other->life[i];
        while (true)
        {
            ir_life_entry_t *entry = &self->life[myi];

            if (otherlife.end+1 < entry->start)
            {
                /* adding an interval before entry */
                if (!ir_value_life_insert(self, myi, otherlife))
                    return false;
                ++myi;
                break;
            }

            if (otherlife.start <  entry->start &&
                otherlife.end+1 >= entry->start)
            {
                /* starts earlier and overlaps */
                entry->start = otherlife.start;
            }

            if (otherlife.end   >  entry->end &&
                otherlife.start <= entry->end+1)
            {
                /* ends later and overlaps */
                entry->end = otherlife.end;
            }

            /* see if our change combines it with the next ranges */
            while (myi+1 < self->life.size() &&
                   entry->end+1 >= self->life[1+myi].start)
            {
                /* overlaps with (myi+1) */
                if (entry->end < self->life[1+myi].end)
                    entry->end = self->life[1+myi].end;
                self->life.erase(self->life.begin() + (myi + 1));
                entry = &self->life[myi];
            }

            /* see if we're after the entry */
            if (otherlife.start > entry->end)
            {
                ++myi;
                /* append if we're at the end */
                if (myi >= self->life.size()) {
                    self->life.emplace_back(otherlife);
                    break;
                }
                /* otherweise check the next range */
                continue;
            }
            break;
        }
    }
    return true;
}

static bool ir_values_overlap(const ir_value *a, const ir_value *b)
{
    /* For any life entry in A see if it overlaps with
     * any life entry in B.
     * Note that the life entries are orderes, so we can make a
     * more efficient algorithm there than naively translating the
     * statement above.
     */

    const ir_life_entry_t *la, *lb, *enda, *endb;

    /* first of all, if either has no life range, they cannot clash */
    if (a->life.empty() || b->life.empty())
        return false;

    la = &a->life.front();
    lb = &b->life.front();
    enda = &a->life.back() + 1;
    endb = &b->life.back() + 1;
    while (true)
    {
        /* check if the entries overlap, for that,
         * both must start before the other one ends.
         */
        if (la->start < lb->end &&
            lb->start < la->end)
        {
            return true;
        }

        /* entries are ordered
         * one entry is earlier than the other
         * that earlier entry will be moved forward
         */
        if (la->start < lb->start)
        {
            /* order: A B, move A forward
             * check if we hit the end with A
             */
            if (++la == enda)
                break;
        }
        else /* if (lb->start < la->start)  actually <= */
        {
            /* order: B A, move B forward
             * check if we hit the end with B
             */
            if (++lb == endb)
                break;
        }
    }
    return false;
}

/***********************************************************************
 *IR main operations
 */

static bool ir_check_unreachable(ir_block *self)
{
    /* The IR should never have to deal with unreachable code */
    if (!self->final/* || OPTS_FLAG(ALLOW_UNREACHABLE_CODE)*/)
        return true;
    irerror(self->context, "unreachable statement (%s)", self->label.c_str());
    return false;
}

bool ir_block_create_store_op(ir_block *self, lex_ctx_t ctx, int op, ir_value *target, ir_value *what)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;

    if (target->store == store_value &&
        (op < INSTR_STOREP_F || op > INSTR_STOREP_FNC))
    {
        irerror(self->context, "cannot store to an SSA value");
        irerror(self->context, "trying to store: %s <- %s", target->name.c_str(), what->name.c_str());
        irerror(self->context, "instruction: %s", util_instr_str[op]);
        return false;
    }

    in = ir_instr_new(ctx, self, op);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, target, (op < INSTR_STOREP_F || op > INSTR_STOREP_FNC)) ||
        !ir_instr_op(in, 1, what, false))
    {
        ir_instr_delete(in);
        return false;
    }
    vec_push(self->instr, in);
    return true;
}

bool ir_block_create_state_op(ir_block *self, lex_ctx_t ctx, ir_value *frame, ir_value *think)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;

    in = ir_instr_new(ctx, self, INSTR_STATE);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, frame, false) ||
        !ir_instr_op(in, 1, think, false))
    {
        ir_instr_delete(in);
        return false;
    }
    vec_push(self->instr, in);
    return true;
}

static bool ir_block_create_store(ir_block *self, lex_ctx_t ctx, ir_value *target, ir_value *what)
{
    int op = 0;
    qc_type vtype;
    if (target->vtype == TYPE_VARIANT)
        vtype = what->vtype;
    else
        vtype = target->vtype;

#if 0
    if      (vtype == TYPE_FLOAT   && what->vtype == TYPE_INTEGER)
        op = INSTR_CONV_ITOF;
    else if (vtype == TYPE_INTEGER && what->vtype == TYPE_FLOAT)
        op = INSTR_CONV_FTOI;
#endif
        op = type_store_instr[vtype];

    if (OPTS_FLAG(ADJUST_VECTOR_FIELDS)) {
        if (op == INSTR_STORE_FLD && what->fieldtype == TYPE_VECTOR)
            op = INSTR_STORE_V;
    }

    return ir_block_create_store_op(self, ctx, op, target, what);
}

bool ir_block_create_storep(ir_block *self, lex_ctx_t ctx, ir_value *target, ir_value *what)
{
    int op = 0;
    qc_type vtype;

    if (target->vtype != TYPE_POINTER)
        return false;

    /* storing using pointer - target is a pointer, type must be
     * inferred from source
     */
    vtype = what->vtype;

    op = type_storep_instr[vtype];
    if (OPTS_FLAG(ADJUST_VECTOR_FIELDS)) {
        if (op == INSTR_STOREP_FLD && what->fieldtype == TYPE_VECTOR)
            op = INSTR_STOREP_V;
    }

    return ir_block_create_store_op(self, ctx, op, target, what);
}

bool ir_block_create_return(ir_block *self, lex_ctx_t ctx, ir_value *v)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;

    self->final = true;

    self->is_return = true;
    in = ir_instr_new(ctx, self, INSTR_RETURN);
    if (!in)
        return false;

    if (v && !ir_instr_op(in, 0, v, false)) {
        ir_instr_delete(in);
        return false;
    }

    vec_push(self->instr, in);
    return true;
}

bool ir_block_create_if(ir_block *self, lex_ctx_t ctx, ir_value *v,
                        ir_block *ontrue, ir_block *onfalse)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;
    self->final = true;
    /*in = ir_instr_new(ctx, self, (v->vtype == TYPE_STRING ? INSTR_IF_S : INSTR_IF_F));*/
    in = ir_instr_new(ctx, self, VINSTR_COND);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, v, false)) {
        ir_instr_delete(in);
        return false;
    }

    in->bops[0] = ontrue;
    in->bops[1] = onfalse;

    vec_push(self->instr, in);

    vec_push(self->exits, ontrue);
    vec_push(self->exits, onfalse);
    vec_push(ontrue->entries,  self);
    vec_push(onfalse->entries, self);
    return true;
}

bool ir_block_create_jump(ir_block *self, lex_ctx_t ctx, ir_block *to)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;
    self->final = true;
    in = ir_instr_new(ctx, self, VINSTR_JUMP);
    if (!in)
        return false;

    in->bops[0] = to;
    vec_push(self->instr, in);

    vec_push(self->exits, to);
    vec_push(to->entries, self);
    return true;
}

bool ir_block_create_goto(ir_block *self, lex_ctx_t ctx, ir_block *to)
{
    self->owner->flags |= IR_FLAG_HAS_GOTO;
    return ir_block_create_jump(self, ctx, to);
}

ir_instr* ir_block_create_phi(ir_block *self, lex_ctx_t ctx, const char *label, qc_type ot)
{
    ir_value *out;
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return nullptr;
    in = ir_instr_new(ctx, self, VINSTR_PHI);
    if (!in)
        return nullptr;
    out = ir_value_out(self->owner, label, store_value, ot);
    if (!out) {
        ir_instr_delete(in);
        return nullptr;
    }
    if (!ir_instr_op(in, 0, out, true)) {
        ir_instr_delete(in);
        return nullptr;
    }
    vec_push(self->instr, in);
    return in;
}

ir_value* ir_phi_value(ir_instr *self)
{
    return self->_ops[0];
}

void ir_phi_add(ir_instr* self, ir_block *b, ir_value *v)
{
    ir_phi_entry_t pe;

    if (!vec_ir_block_find(self->owner->entries, b, nullptr)) {
        /* Must not be possible to cause this, otherwise the AST
         * is doing something wrong.
         */
        irerror(self->context, "Invalid entry block for PHI");
        exit(EXIT_FAILURE);
    }

    pe.value = v;
    pe.from = b;
    v->reads.push_back(self);
    self->phi.push_back(pe);
}

/* call related code */
ir_instr* ir_block_create_call(ir_block *self, lex_ctx_t ctx, const char *label, ir_value *func, bool noreturn)
{
    ir_value *out;
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return nullptr;
    in = ir_instr_new(ctx, self, (noreturn ? VINSTR_NRCALL : INSTR_CALL0));
    if (!in)
        return nullptr;
    if (noreturn) {
        self->final = true;
        self->is_return = true;
    }
    out = ir_value_out(self->owner, label, (func->outtype == TYPE_VOID) ? store_return : store_value, func->outtype);
    if (!out) {
        ir_instr_delete(in);
        return nullptr;
    }
    if (!ir_instr_op(in, 0, out, true) ||
        !ir_instr_op(in, 1, func, false))
    {
        ir_instr_delete(in);
        return nullptr;
    }
    vec_push(self->instr, in);
    /*
    if (noreturn) {
        if (!ir_block_create_return(self, ctx, nullptr)) {
            compile_error(ctx, "internal error: failed to generate dummy-return instruction");
            ir_instr_delete(in);
            return nullptr;
        }
    }
    */
    return in;
}

ir_value* ir_call_value(ir_instr *self)
{
    return self->_ops[0];
}

void ir_call_param(ir_instr* self, ir_value *v)
{
    self->params.push_back(v);
    v->reads.push_back(self);
}

/* binary op related code */

ir_value* ir_block_create_binop(ir_block *self, lex_ctx_t ctx,
                                const char *label, int opcode,
                                ir_value *left, ir_value *right)
{
    qc_type ot = TYPE_VOID;
    switch (opcode) {
        case INSTR_ADD_F:
        case INSTR_SUB_F:
        case INSTR_DIV_F:
        case INSTR_MUL_F:
        case INSTR_MUL_V:
        case INSTR_AND:
        case INSTR_OR:
#if 0
        case INSTR_AND_I:
        case INSTR_AND_IF:
        case INSTR_AND_FI:
        case INSTR_OR_I:
        case INSTR_OR_IF:
        case INSTR_OR_FI:
#endif
        case INSTR_BITAND:
        case INSTR_BITOR:
        case VINSTR_BITXOR:
#if 0
        case INSTR_SUB_S: /* -- offset of string as float */
        case INSTR_MUL_IF:
        case INSTR_MUL_FI:
        case INSTR_DIV_IF:
        case INSTR_DIV_FI:
        case INSTR_BITOR_IF:
        case INSTR_BITOR_FI:
        case INSTR_BITAND_FI:
        case INSTR_BITAND_IF:
        case INSTR_EQ_I:
        case INSTR_NE_I:
#endif
            ot = TYPE_FLOAT;
            break;
#if 0
        case INSTR_ADD_I:
        case INSTR_ADD_IF:
        case INSTR_ADD_FI:
        case INSTR_SUB_I:
        case INSTR_SUB_FI:
        case INSTR_SUB_IF:
        case INSTR_MUL_I:
        case INSTR_DIV_I:
        case INSTR_BITAND_I:
        case INSTR_BITOR_I:
        case INSTR_XOR_I:
        case INSTR_RSHIFT_I:
        case INSTR_LSHIFT_I:
            ot = TYPE_INTEGER;
            break;
#endif
        case INSTR_ADD_V:
        case INSTR_SUB_V:
        case INSTR_MUL_VF:
        case INSTR_MUL_FV:
        case VINSTR_BITAND_V:
        case VINSTR_BITOR_V:
        case VINSTR_BITXOR_V:
        case VINSTR_BITAND_VF:
        case VINSTR_BITOR_VF:
        case VINSTR_BITXOR_VF:
        case VINSTR_CROSS:
#if 0
        case INSTR_DIV_VF:
        case INSTR_MUL_IV:
        case INSTR_MUL_VI:
#endif
            ot = TYPE_VECTOR;
            break;
#if 0
        case INSTR_ADD_SF:
            ot = TYPE_POINTER;
            break;
#endif
    /*
     * after the following default case, the value of opcode can never
     * be 1, 2, 3, 4, 5, 6, 7, 8, 9, 62, 63, 64, 65
     */
        default:
            /* ranges: */
            /* boolean operations result in floats */

            /*
             * opcode >= 10 takes true branch opcode is at least 10
             * opcode <= 23 takes false branch opcode is at least 24
             */
            if (opcode >= INSTR_EQ_F && opcode <= INSTR_GT)
                ot = TYPE_FLOAT;

            /*
             * At condition "opcode <= 23", the value of "opcode" must be
             * at least 24.
             * At condition "opcode <= 23", the value of "opcode" cannot be
             * equal to any of {1, 2, 3, 4, 5, 6, 7, 8, 9, 62, 63, 64, 65}.
             * The condition "opcode <= 23" cannot be true.
             *
             * Thus ot=2 (TYPE_FLOAT) can never be true
             */
#if 0
            else if (opcode >= INSTR_LE && opcode <= INSTR_GT)
                ot = TYPE_FLOAT;
            else if (opcode >= INSTR_LE_I && opcode <= INSTR_EQ_FI)
                ot = TYPE_FLOAT;
#endif
            break;
    };
    if (ot == TYPE_VOID) {
        /* The AST or parser were supposed to check this! */
        return nullptr;
    }

    return ir_block_create_general_instr(self, ctx, label, opcode, left, right, ot);
}

ir_value* ir_block_create_unary(ir_block *self, lex_ctx_t ctx,
                                const char *label, int opcode,
                                ir_value *operand)
{
    qc_type ot = TYPE_FLOAT;
    switch (opcode) {
        case INSTR_NOT_F:
        case INSTR_NOT_V:
        case INSTR_NOT_S:
        case INSTR_NOT_ENT:
        case INSTR_NOT_FNC: /*
        case INSTR_NOT_I:   */
            ot = TYPE_FLOAT;
            break;

        /*
         * Negation for virtual instructions is emulated with 0-value. Thankfully
         * the operand for 0 already exists so we just source it from here.
         */
        case VINSTR_NEG_F:
            return ir_block_create_general_instr(self, ctx, label, INSTR_SUB_F, nullptr, operand, ot);
        case VINSTR_NEG_V:
            return ir_block_create_general_instr(self, ctx, label, INSTR_SUB_V, nullptr, operand, TYPE_VECTOR);

        default:
            ot = operand->vtype;
            break;
    };
    if (ot == TYPE_VOID) {
        /* The AST or parser were supposed to check this! */
        return nullptr;
    }

    /* let's use the general instruction creator and pass nullptr for OPB */
    return ir_block_create_general_instr(self, ctx, label, opcode, operand, nullptr, ot);
}

static ir_value* ir_block_create_general_instr(ir_block *self, lex_ctx_t ctx, const char *label,
                                        int op, ir_value *a, ir_value *b, qc_type outype)
{
    ir_instr *instr;
    ir_value *out;

    out = ir_value_out(self->owner, label, store_value, outype);
    if (!out)
        return nullptr;

    instr = ir_instr_new(ctx, self, op);
    if (!instr) {
        return nullptr;
    }

    if (!ir_instr_op(instr, 0, out, true) ||
        !ir_instr_op(instr, 1, a, false) ||
        !ir_instr_op(instr, 2, b, false) )
    {
        goto on_error;
    }

    vec_push(self->instr, instr);

    return out;
on_error:
    ir_instr_delete(instr);
    return nullptr;
}

ir_value* ir_block_create_fieldaddress(ir_block *self, lex_ctx_t ctx, const char *label, ir_value *ent, ir_value *field)
{
    ir_value *v;

    /* Support for various pointer types todo if so desired */
    if (ent->vtype != TYPE_ENTITY)
        return nullptr;

    if (field->vtype != TYPE_FIELD)
        return nullptr;

    v = ir_block_create_general_instr(self, ctx, label, INSTR_ADDRESS, ent, field, TYPE_POINTER);
    v->fieldtype = field->fieldtype;
    return v;
}

ir_value* ir_block_create_load_from_ent(ir_block *self, lex_ctx_t ctx, const char *label, ir_value *ent, ir_value *field, qc_type outype)
{
    int op;
    if (ent->vtype != TYPE_ENTITY)
        return nullptr;

    /* at some point we could redirect for TYPE_POINTER... but that could lead to carelessness */
    if (field->vtype != TYPE_FIELD)
        return nullptr;

    switch (outype)
    {
        case TYPE_FLOAT:    op = INSTR_LOAD_F;   break;
        case TYPE_VECTOR:   op = INSTR_LOAD_V;   break;
        case TYPE_STRING:   op = INSTR_LOAD_S;   break;
        case TYPE_FIELD:    op = INSTR_LOAD_FLD; break;
        case TYPE_ENTITY:   op = INSTR_LOAD_ENT; break;
        case TYPE_FUNCTION: op = INSTR_LOAD_FNC; break;
#if 0
        case TYPE_POINTER: op = INSTR_LOAD_I;   break;
        case TYPE_INTEGER: op = INSTR_LOAD_I;   break;
#endif
        default:
            irerror(self->context, "invalid type for ir_block_create_load_from_ent: %s", type_name[outype]);
            return nullptr;
    }

    return ir_block_create_general_instr(self, ctx, label, op, ent, field, outype);
}

/* PHI resolving breaks the SSA, and must thus be the last
 * step before life-range calculation.
 */

static bool ir_block_naive_phi(ir_block *self);
bool ir_function_naive_phi(ir_function *self)
{
    for (auto& b : self->blocks)
        if (!ir_block_naive_phi(b.get()))
            return false;
    return true;
}

static bool ir_block_naive_phi(ir_block *self)
{
    size_t i;
    /* FIXME: optionally, create_phi can add the phis
     * to a list so we don't need to loop through blocks
     * - anyway: "don't optimize YET"
     */
    for (i = 0; i < vec_size(self->instr); ++i)
    {
        ir_instr *instr = self->instr[i];
        if (instr->opcode != VINSTR_PHI)
            continue;

        vec_remove(self->instr, i, 1);
        --i; /* NOTE: i+1 below */

        for (auto &it : instr->phi) {
            ir_value *v = it.value;
            ir_block *b = it.from;
            if (v->store == store_value && v->reads.size() == 1 && v->writes.size() == 1) {
                /* replace the value */
                if (!ir_instr_op(v->writes[0], 0, instr->_ops[0], true))
                    return false;
            } else {
                /* force a move instruction */
                ir_instr *prevjump = vec_last(b->instr);
                vec_pop(b->instr);
                b->final = false;
                instr->_ops[0]->store = store_global;
                if (!ir_block_create_store(b, instr->context, instr->_ops[0], v))
                    return false;
                instr->_ops[0]->store = store_value;
                vec_push(b->instr, prevjump);
                b->final = true;
            }
        }
        ir_instr_delete(instr);
    }
    return true;
}

/***********************************************************************
 *IR Temp allocation code
 * Propagating value life ranges by walking through the function backwards
 * until no more changes are made.
 * In theory this should happen once more than once for every nested loop
 * level.
 * Though this implementation might run an additional time for if nests.
 */

/* Enumerate instructions used by value's life-ranges
 */
static void ir_block_enumerate(ir_block *self, size_t *_eid)
{
    size_t i;
    size_t eid = *_eid;
    for (i = 0; i < vec_size(self->instr); ++i)
    {
        self->instr[i]->eid = eid++;
    }
    *_eid = eid;
}

/* Enumerate blocks and instructions.
 * The block-enumeration is unordered!
 * We do not really use the block enumreation, however
 * the instruction enumeration is important for life-ranges.
 */
void ir_function_enumerate(ir_function *self)
{
    size_t instruction_id = 0;
    size_t block_eid = 0;
    for (auto& block : self->blocks)
    {
        /* each block now gets an additional "entry" instruction id
         * we can use to avoid point-life issues
         */
        block->entry_id = instruction_id;
        block->eid      = block_eid;
        ++instruction_id;
        ++block_eid;

        ir_block_enumerate(block.get(), &instruction_id);
    }
}

/* Local-value allocator
 * After finishing creating the liferange of all values used in a function
 * we can allocate their global-positions.
 * This is the counterpart to register-allocation in register machines.
 */
struct function_allocator {
    ir_value **locals;
    size_t *sizes;
    size_t *positions;
    bool *unique;
};

static bool function_allocator_alloc(function_allocator *alloc, ir_value *var)
{
    ir_value *slot;
    size_t vsize = ir_value_sizeof(var);

    var->code.local = vec_size(alloc->locals);

    slot = new ir_value("reg", store_global, var->vtype);
    if (!slot)
        return false;

    if (!ir_value_life_merge_into(slot, var))
        goto localerror;

    vec_push(alloc->locals, slot);
    vec_push(alloc->sizes, vsize);
    vec_push(alloc->unique, var->unique_life);

    return true;

localerror:
    delete slot;
    return false;
}

static bool ir_function_allocator_assign(ir_function *self, function_allocator *alloc, ir_value *v)
{
    size_t a;
    ir_value *slot;

    if (v->unique_life)
        return function_allocator_alloc(alloc, v);

    for (a = 0; a < vec_size(alloc->locals); ++a)
    {
        /* if it's reserved for a unique liferange: skip */
        if (alloc->unique[a])
            continue;

        slot = alloc->locals[a];

        /* never resize parameters
         * will be required later when overlapping temps + locals
         */
        if (a < vec_size(self->params) &&
            alloc->sizes[a] < ir_value_sizeof(v))
        {
            continue;
        }

        if (ir_values_overlap(v, slot))
            continue;

        if (!ir_value_life_merge_into(slot, v))
            return false;

        /* adjust size for this slot */
        if (alloc->sizes[a] < ir_value_sizeof(v))
            alloc->sizes[a] = ir_value_sizeof(v);

        v->code.local = a;
        return true;
    }
    if (a >= vec_size(alloc->locals)) {
        if (!function_allocator_alloc(alloc, v))
            return false;
    }
    return true;
}

bool ir_function_allocate_locals(ir_function *self)
{
    bool   retval = true;
    size_t pos;
    bool   opt_gt = OPTS_OPTIMIZATION(OPTIM_GLOBAL_TEMPS);

    function_allocator lockalloc, globalloc;

    if (self->locals.empty() && self->values.empty())
        return true;

    globalloc.locals    = nullptr;
    globalloc.sizes     = nullptr;
    globalloc.positions = nullptr;
    globalloc.unique    = nullptr;
    lockalloc.locals    = nullptr;
    lockalloc.sizes     = nullptr;
    lockalloc.positions = nullptr;
    lockalloc.unique    = nullptr;

    size_t i;
    for (i = 0; i < self->locals.size(); ++i)
    {
        ir_value *v = self->locals[i].get();
        if ((self->flags & IR_FLAG_MASK_NO_LOCAL_TEMPS) || !OPTS_OPTIMIZATION(OPTIM_LOCAL_TEMPS)) {
            v->locked      = true;
            v->unique_life = true;
        }
        else if (i >= vec_size(self->params))
            break;
        else
            v->locked = true; /* lock parameters locals */
        if (!function_allocator_alloc((v->locked || !opt_gt ? &lockalloc : &globalloc), v))
            goto error;
    }
    for (; i < self->locals.size(); ++i)
    {
        ir_value *v = self->locals[i].get();
        if (v->life.empty())
            continue;
        if (!ir_function_allocator_assign(self, (v->locked || !opt_gt ? &lockalloc : &globalloc), v))
            goto error;
    }

    /* Allocate a slot for any value that still exists */
    for (i = 0; i < self->values.size(); ++i)
    {
        ir_value *v = self->values[i].get();

        if (v->life.empty())
            continue;

        /* CALL optimization:
         * If the value is a parameter-temp: 1 write, 1 read from a CALL
         * and it's not "locked", write it to the OFS_PARM directly.
         */
        if (OPTS_OPTIMIZATION(OPTIM_CALL_STORES) && !v->locked && !v->unique_life) {
            if (v->reads.size() == 1 && v->writes.size() == 1 &&
                (v->reads[0]->opcode == VINSTR_NRCALL ||
                 (v->reads[0]->opcode >= INSTR_CALL0 && v->reads[0]->opcode <= INSTR_CALL8)
                )
               )
            {
                size_t param;
                ir_instr *call = v->reads[0];
                if (!vec_ir_value_find(call->params, v, &param)) {
                    irerror(call->context, "internal error: unlocked parameter %s not found", v->name.c_str());
                    goto error;
                }
                ++opts_optimizationcount[OPTIM_CALL_STORES];
                v->callparam = true;
                if (param < 8)
                    ir_value_code_setaddr(v, OFS_PARM0 + 3*param);
                else {
                    size_t nprotos = self->owner->extparam_protos.size();
                    ir_value *ep;
                    param -= 8;
                    if (nprotos > param)
                        ep = self->owner->extparam_protos[param].get();
                    else
                    {
                        ep = ir_gen_extparam_proto(self->owner);
                        while (++nprotos <= param)
                            ep = ir_gen_extparam_proto(self->owner);
                    }
                    ir_instr_op(v->writes[0], 0, ep, true);
                    call->params[param+8] = ep;
                }
                continue;
            }
            if (v->writes.size() == 1 && v->writes[0]->opcode == INSTR_CALL0) {
                v->store = store_return;
                if (v->members[0]) v->members[0]->store = store_return;
                if (v->members[1]) v->members[1]->store = store_return;
                if (v->members[2]) v->members[2]->store = store_return;
                ++opts_optimizationcount[OPTIM_CALL_STORES];
                continue;
            }
        }

        if (!ir_function_allocator_assign(self, (v->locked || !opt_gt ? &lockalloc : &globalloc), v))
            goto error;
    }

    if (!lockalloc.sizes && !globalloc.sizes) {
        goto cleanup;
    }
    vec_push(lockalloc.positions, 0);
    vec_push(globalloc.positions, 0);

    /* Adjust slot positions based on sizes */
    if (lockalloc.sizes) {
        pos = (vec_size(lockalloc.sizes) ? lockalloc.positions[0] : 0);
        for (i = 1; i < vec_size(lockalloc.sizes); ++i)
        {
            pos = lockalloc.positions[i-1] + lockalloc.sizes[i-1];
            vec_push(lockalloc.positions, pos);
        }
        self->allocated_locals = pos + vec_last(lockalloc.sizes);
    }
    if (globalloc.sizes) {
        pos = (vec_size(globalloc.sizes) ? globalloc.positions[0] : 0);
        for (i = 1; i < vec_size(globalloc.sizes); ++i)
        {
            pos = globalloc.positions[i-1] + globalloc.sizes[i-1];
            vec_push(globalloc.positions, pos);
        }
        self->globaltemps = pos + vec_last(globalloc.sizes);
    }

    /* Locals need to know their new position */
    for (auto& local : self->locals) {
        if (local->locked || !opt_gt)
            local->code.local = lockalloc.positions[local->code.local];
        else
            local->code.local = globalloc.positions[local->code.local];
    }
    /* Take over the actual slot positions on values */
    for (auto& value : self->values) {
        if (value->locked || !opt_gt)
            value->code.local = lockalloc.positions[value->code.local];
        else
            value->code.local = globalloc.positions[value->code.local];
    }

    goto cleanup;

error:
    retval = false;
cleanup:
    for (i = 0; i < vec_size(lockalloc.locals); ++i)
        delete lockalloc.locals[i];
    for (i = 0; i < vec_size(globalloc.locals); ++i)
        delete globalloc.locals[i];
    vec_free(globalloc.unique);
    vec_free(globalloc.locals);
    vec_free(globalloc.sizes);
    vec_free(globalloc.positions);
    vec_free(lockalloc.unique);
    vec_free(lockalloc.locals);
    vec_free(lockalloc.sizes);
    vec_free(lockalloc.positions);
    return retval;
}

/* Get information about which operand
 * is read from, or written to.
 */
static void ir_op_read_write(int op, size_t *read, size_t *write)
{
    switch (op)
    {
    case VINSTR_JUMP:
    case INSTR_GOTO:
        *write = 0;
        *read = 0;
        break;
    case INSTR_IF:
    case INSTR_IFNOT:
#if 0
    case INSTR_IF_S:
    case INSTR_IFNOT_S:
#endif
    case INSTR_RETURN:
    case VINSTR_COND:
        *write = 0;
        *read = 1;
        break;
    case INSTR_STOREP_F:
    case INSTR_STOREP_V:
    case INSTR_STOREP_S:
    case INSTR_STOREP_ENT:
    case INSTR_STOREP_FLD:
    case INSTR_STOREP_FNC:
        *write = 0;
        *read  = 7;
        break;
    default:
        *write = 1;
        *read = 6;
        break;
    };
}

static bool ir_block_living_add_instr(ir_block *self, size_t eid) {
    bool changed = false;
    for (auto &it : self->living)
        if (ir_value_life_merge(it, eid))
            changed = true;
    return changed;
}

static bool ir_block_living_lock(ir_block *self) {
    bool changed = false;
    for (auto &it : self->living) {
        if (it->locked)
            continue;
        it->locked = true;
        changed = true;
    }
    return changed;
}

static bool ir_block_life_propagate(ir_block *self, bool *changed)
{
    ir_instr *instr;
    ir_value *value;
    size_t i, o, p, mem;
    /* bitmasks which operands are read from or written to */
    size_t read, write;
    char dbg_ind[16];
    dbg_ind[0] = '#';
    dbg_ind[1] = '0';
    (void)dbg_ind;

    self->living.clear();

    p = vec_size(self->exits);
    for (i = 0; i < p; ++i) {
        ir_block *prev = self->exits[i];
        for (auto &it : prev->living)
            if (!vec_ir_value_find(self->living, it, nullptr))
                self->living.push_back(it);
    }

    i = vec_size(self->instr);
    while (i)
    { --i;
        instr = self->instr[i];

        /* See which operands are read and write operands */
        ir_op_read_write(instr->opcode, &read, &write);

        /* Go through the 3 main operands
         * writes first, then reads
         */
        for (o = 0; o < 3; ++o)
        {
            if (!instr->_ops[o]) /* no such operand */
                continue;

            value = instr->_ops[o];

            /* We only care about locals */
            /* we also calculate parameter liferanges so that locals
             * can take up parameter slots */
            if (value->store != store_value &&
                value->store != store_local &&
                value->store != store_param)
                continue;

            /* write operands */
            /* When we write to a local, we consider it "dead" for the
             * remaining upper part of the function, since in SSA a value
             * can only be written once (== created)
             */
            if (write & (1<<o))
            {
                size_t idx;
                bool in_living = vec_ir_value_find(self->living, value, &idx);
                if (!in_living)
                {
                    /* If the value isn't alive it hasn't been read before... */
                    /* TODO: See if the warning can be emitted during parsing or AST processing
                     * otherwise have warning printed here.
                     * IF printing a warning here: include filecontext_t,
                     * and make sure it's only printed once
                     * since this function is run multiple times.
                     */
                    /* con_err( "Value only written %s\n", value->name); */
                    if (ir_value_life_merge(value, instr->eid))
                        *changed = true;
                } else {
                    /* since 'living' won't contain it
                     * anymore, merge the value, since
                     * (A) doesn't.
                     */
                    if (ir_value_life_merge(value, instr->eid))
                        *changed = true;
                    // Then remove
                    self->living.erase(self->living.begin() + idx);
                }
                /* Removing a vector removes all members */
                for (mem = 0; mem < 3; ++mem) {
                    if (value->members[mem] && vec_ir_value_find(self->living, value->members[mem], &idx)) {
                        if (ir_value_life_merge(value->members[mem], instr->eid))
                            *changed = true;
                        self->living.erase(self->living.begin() + idx);
                    }
                }
                /* Removing the last member removes the vector */
                if (value->memberof) {
                    value = value->memberof;
                    for (mem = 0; mem < 3; ++mem) {
                        if (value->members[mem] && vec_ir_value_find(self->living, value->members[mem], nullptr))
                            break;
                    }
                    if (mem == 3 && vec_ir_value_find(self->living, value, &idx)) {
                        if (ir_value_life_merge(value, instr->eid))
                            *changed = true;
                        self->living.erase(self->living.begin() + idx);
                    }
                }
            }
        }

        /* These operations need a special case as they can break when using
         * same source and destination operand otherwise, as the engine may
         * read the source multiple times. */
        if (instr->opcode == INSTR_MUL_VF ||
            instr->opcode == VINSTR_BITAND_VF ||
            instr->opcode == VINSTR_BITOR_VF ||
            instr->opcode == VINSTR_BITXOR ||
            instr->opcode == VINSTR_BITXOR_VF ||
            instr->opcode == VINSTR_BITXOR_V ||
            instr->opcode == VINSTR_CROSS)
        {
            value = instr->_ops[2];
            /* the float source will get an additional lifetime */
            if (ir_value_life_merge(value, instr->eid+1))
                *changed = true;
            if (value->memberof && ir_value_life_merge(value->memberof, instr->eid+1))
                *changed = true;
        }

        if (instr->opcode == INSTR_MUL_FV ||
            instr->opcode == INSTR_LOAD_V ||
            instr->opcode == VINSTR_BITXOR ||
            instr->opcode == VINSTR_BITXOR_VF ||
            instr->opcode == VINSTR_BITXOR_V ||
            instr->opcode == VINSTR_CROSS)
        {
            value = instr->_ops[1];
            /* the float source will get an additional lifetime */
            if (ir_value_life_merge(value, instr->eid+1))
                *changed = true;
            if (value->memberof && ir_value_life_merge(value->memberof, instr->eid+1))
                *changed = true;
        }

        for (o = 0; o < 3; ++o)
        {
            if (!instr->_ops[o]) /* no such operand */
                continue;

            value = instr->_ops[o];

            /* We only care about locals */
            /* we also calculate parameter liferanges so that locals
             * can take up parameter slots */
            if (value->store != store_value &&
                value->store != store_local &&
                value->store != store_param)
                continue;

            /* read operands */
            if (read & (1<<o))
            {
                if (!vec_ir_value_find(self->living, value, nullptr))
                    self->living.push_back(value);
                /* reading adds the full vector */
                if (value->memberof && !vec_ir_value_find(self->living, value->memberof, nullptr))
                    self->living.push_back(value->memberof);
                for (mem = 0; mem < 3; ++mem) {
                    if (value->members[mem] && !vec_ir_value_find(self->living, value->members[mem], nullptr))
                        self->living.push_back(value->members[mem]);
                }
            }
        }
        /* PHI operands are always read operands */
        for (auto &it : instr->phi) {
            value = it.value;
            if (!vec_ir_value_find(self->living, value, nullptr))
                self->living.push_back(value);
            /* reading adds the full vector */
            if (value->memberof && !vec_ir_value_find(self->living, value->memberof, nullptr))
                self->living.push_back(value->memberof);
            for (mem = 0; mem < 3; ++mem) {
                if (value->members[mem] && !vec_ir_value_find(self->living, value->members[mem], nullptr))
                    self->living.push_back(value->members[mem]);
            }
        }

        /* on a call, all these values must be "locked" */
        if (instr->opcode >= INSTR_CALL0 && instr->opcode <= INSTR_CALL8) {
            if (ir_block_living_lock(self))
                *changed = true;
        }
        /* call params are read operands too */
        for (auto &it : instr->params) {
            value = it;
            if (!vec_ir_value_find(self->living, value, nullptr))
                self->living.push_back(value);
            /* reading adds the full vector */
            if (value->memberof && !vec_ir_value_find(self->living, value->memberof, nullptr))
                self->living.push_back(value->memberof);
            for (mem = 0; mem < 3; ++mem) {
                if (value->members[mem] && !vec_ir_value_find(self->living, value->members[mem], nullptr))
                    self->living.push_back(value->members[mem]);
            }
        }

        /* (A) */
        if (ir_block_living_add_instr(self, instr->eid))
            *changed = true;
    }
    /* the "entry" instruction ID */
    if (ir_block_living_add_instr(self, self->entry_id))
        *changed = true;

    return true;
}

bool ir_function_calculate_liferanges(ir_function *self)
{
    size_t i, s;
    bool changed;

    /* parameters live at 0 */
    for (i = 0; i < vec_size(self->params); ++i)
        if (!ir_value_life_merge(self->locals[i].get(), 0))
            compile_error(self->context, "internal error: failed value-life merging");

    do {
        self->run_id++;
        changed = false;
        i = self->blocks.size();
        while (i--) {
            ir_block_life_propagate(self->blocks[i].get(), &changed);
        }
    } while (changed);

    if (self->blocks.size()) {
        ir_block *block = self->blocks[0].get();
        for (auto &it : block->living) {
            ir_value *v = it;
            if (v->store != store_local)
                continue;
            if (v->vtype == TYPE_VECTOR)
                continue;
            self->flags |= IR_FLAG_HAS_UNINITIALIZED;
            /* find the instruction reading from it */
            for (s = 0; s < v->reads.size(); ++s) {
                if (v->reads[s]->eid == v->life[0].end)
                    break;
            }
            if (s < v->reads.size()) {
                if (irwarning(v->context, WARN_USED_UNINITIALIZED,
                              "variable `%s` may be used uninitialized in this function\n"
                              " -> %s:%i",
                              v->name.c_str(),
                              v->reads[s]->context.file, v->reads[s]->context.line)
                   )
                {
                    return false;
                }
                continue;
            }
            if (v->memberof) {
                ir_value *vec = v->memberof;
                for (s = 0; s < vec->reads.size(); ++s) {
                    if (vec->reads[s]->eid == v->life[0].end)
                        break;
                }
                if (s < vec->reads.size()) {
                    if (irwarning(v->context, WARN_USED_UNINITIALIZED,
                                  "variable `%s` may be used uninitialized in this function\n"
                                  " -> %s:%i",
                                  v->name.c_str(),
                                  vec->reads[s]->context.file, vec->reads[s]->context.line)
                       )
                    {
                        return false;
                    }
                    continue;
                }
            }
            if (irwarning(v->context, WARN_USED_UNINITIALIZED,
                          "variable `%s` may be used uninitialized in this function", v->name.c_str()))
            {
                return false;
            }
        }
    }
    return true;
}

/***********************************************************************
 *IR Code-Generation
 *
 * Since the IR has the convention of putting 'write' operands
 * at the beginning, we have to rotate the operands of instructions
 * properly in order to generate valid QCVM code.
 *
 * Having destinations at a fixed position is more convenient. In QC
 * this is *mostly* OPC,  but FTE adds at least 2 instructions which
 * read from from OPA,  and store to OPB rather than OPC.   Which is
 * partially the reason why the implementation of these instructions
 * in darkplaces has been delayed for so long.
 *
 * Breaking conventions is annoying...
 */
static bool ir_builder_gen_global(ir_builder *self, ir_value *global, bool islocal);

static bool gen_global_field(code_t *code, ir_value *global)
{
    if (global->hasvalue)
    {
        ir_value *fld = global->constval.vpointer;
        if (!fld) {
            irerror(global->context, "Invalid field constant with no field: %s", global->name.c_str());
            return false;
        }

        /* copy the field's value */
        ir_value_code_setaddr(global, code->globals.size());
        code->globals.push_back(fld->code.fieldaddr);
        if (global->fieldtype == TYPE_VECTOR) {
            code->globals.push_back(fld->code.fieldaddr+1);
            code->globals.push_back(fld->code.fieldaddr+2);
        }
    }
    else
    {
        ir_value_code_setaddr(global, code->globals.size());
        code->globals.push_back(0);
        if (global->fieldtype == TYPE_VECTOR) {
            code->globals.push_back(0);
            code->globals.push_back(0);
        }
    }
    if (global->code.globaladdr < 0)
        return false;
    return true;
}

static bool gen_global_pointer(code_t *code, ir_value *global)
{
    if (global->hasvalue)
    {
        ir_value *target = global->constval.vpointer;
        if (!target) {
            irerror(global->context, "Invalid pointer constant: %s", global->name.c_str());
            /* nullptr pointers are pointing to the nullptr constant, which also
             * sits at address 0, but still has an ir_value for itself.
             */
            return false;
        }

        /* Here, relocations ARE possible - in fteqcc-enhanced-qc:
         * void() foo; <- proto
         * void() *fooptr = &foo;
         * void() foo = { code }
         */
        if (!target->code.globaladdr) {
            /* FIXME: Check for the constant nullptr ir_value!
             * because then code.globaladdr being 0 is valid.
             */
            irerror(global->context, "FIXME: Relocation support");
            return false;
        }

        ir_value_code_setaddr(global, code->globals.size());
        code->globals.push_back(target->code.globaladdr);
    }
    else
    {
        ir_value_code_setaddr(global, code->globals.size());
        code->globals.push_back(0);
    }
    if (global->code.globaladdr < 0)
        return false;
    return true;
}

static bool gen_blocks_recursive(code_t *code, ir_function *func, ir_block *block)
{
    prog_section_statement_t stmt;
    ir_instr *instr;
    ir_block *target;
    ir_block *ontrue;
    ir_block *onfalse;
    size_t    stidx;
    size_t    i;
    int       j;

    block->generated = true;
    block->code_start = code->statements.size();
    for (i = 0; i < vec_size(block->instr); ++i)
    {
        instr = block->instr[i];

        if (instr->opcode == VINSTR_PHI) {
            irerror(block->context, "cannot generate virtual instruction (phi)");
            return false;
        }

        if (instr->opcode == VINSTR_JUMP) {
            target = instr->bops[0];
            /* for uncoditional jumps, if the target hasn't been generated
             * yet, we generate them right here.
             */
            if (!target->generated)
                return gen_blocks_recursive(code, func, target);

            /* otherwise we generate a jump instruction */
            stmt.opcode = INSTR_GOTO;
            stmt.o1.s1 = target->code_start - code->statements.size();
            stmt.o2.s1 = 0;
            stmt.o3.s1 = 0;
            if (stmt.o1.s1 != 1)
                code_push_statement(code, &stmt, instr->context);

            /* no further instructions can be in this block */
            return true;
        }

        if (instr->opcode == VINSTR_BITXOR) {
            stmt.opcode = INSTR_BITOR;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);
            stmt.opcode = INSTR_BITAND;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]);
            stmt.o3.s1 = ir_value_code_addr(func->owner->vinstr_temp[0]);
            code_push_statement(code, &stmt, instr->context);
            stmt.opcode = INSTR_SUB_F;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[0]);
            stmt.o2.s1 = ir_value_code_addr(func->owner->vinstr_temp[0]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);

            /* instruction generated */
            continue;
        }

        if (instr->opcode == VINSTR_BITAND_V) {
            stmt.opcode = INSTR_BITAND;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);
            ++stmt.o1.s1;
            ++stmt.o2.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->context);
            ++stmt.o1.s1;
            ++stmt.o2.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->context);

            /* instruction generated */
            continue;
        }

        if (instr->opcode == VINSTR_BITOR_V) {
            stmt.opcode = INSTR_BITOR;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);
            ++stmt.o1.s1;
            ++stmt.o2.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->context);
            ++stmt.o1.s1;
            ++stmt.o2.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->context);

            /* instruction generated */
            continue;
        }

        if (instr->opcode == VINSTR_BITXOR_V) {
            for (j = 0; j < 3; ++j) {
                stmt.opcode = INSTR_BITOR;
                stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]) + j;
                stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]) + j;
                stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]) + j;
                code_push_statement(code, &stmt, instr->context);
                stmt.opcode = INSTR_BITAND;
                stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]) + j;
                stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]) + j;
                stmt.o3.s1 = ir_value_code_addr(func->owner->vinstr_temp[0]) + j;
                code_push_statement(code, &stmt, instr->context);
            }
            stmt.opcode = INSTR_SUB_V;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[0]);
            stmt.o2.s1 = ir_value_code_addr(func->owner->vinstr_temp[0]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);

            /* instruction generated */
            continue;
        }

        if (instr->opcode == VINSTR_BITAND_VF) {
            stmt.opcode = INSTR_BITAND;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);
            ++stmt.o1.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->context);
            ++stmt.o1.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->context);

            /* instruction generated */
            continue;
        }

        if (instr->opcode == VINSTR_BITOR_VF) {
            stmt.opcode = INSTR_BITOR;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);
            ++stmt.o1.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->context);
            ++stmt.o1.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->context);

            /* instruction generated */
            continue;
        }

        if (instr->opcode == VINSTR_BITXOR_VF) {
            for (j = 0; j < 3; ++j) {
                stmt.opcode = INSTR_BITOR;
                stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]) + j;
                stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]);
                stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]) + j;
                code_push_statement(code, &stmt, instr->context);
                stmt.opcode = INSTR_BITAND;
                stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]) + j;
                stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]);
                stmt.o3.s1 = ir_value_code_addr(func->owner->vinstr_temp[0]) + j;
                code_push_statement(code, &stmt, instr->context);
            }
            stmt.opcode = INSTR_SUB_V;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[0]);
            stmt.o2.s1 = ir_value_code_addr(func->owner->vinstr_temp[0]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);

            /* instruction generated */
            continue;
        }

        if (instr->opcode == VINSTR_CROSS) {
            stmt.opcode = INSTR_MUL_F;
            for (j = 0; j < 3; ++j) {
                stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]) + (j + 1) % 3;
                stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]) + (j + 2) % 3;
                stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]) + j;
                code_push_statement(code, &stmt, instr->context);
                stmt.o1.s1 = ir_value_code_addr(instr->_ops[1]) + (j + 2) % 3;
                stmt.o2.s1 = ir_value_code_addr(instr->_ops[2]) + (j + 1) % 3;
                stmt.o3.s1 = ir_value_code_addr(func->owner->vinstr_temp[0]) + j;
                code_push_statement(code, &stmt, instr->context);
            }
            stmt.opcode = INSTR_SUB_V;
            stmt.o1.s1 = ir_value_code_addr(instr->_ops[0]);
            stmt.o2.s1 = ir_value_code_addr(func->owner->vinstr_temp[0]);
            stmt.o3.s1 = ir_value_code_addr(instr->_ops[0]);
            code_push_statement(code, &stmt, instr->context);

            /* instruction generated */
            continue;
        }

        if (instr->opcode == VINSTR_COND) {
            ontrue  = instr->bops[0];
            onfalse = instr->bops[1];
            /* TODO: have the AST signal which block should
             * come first: eg. optimize IFs without ELSE...
             */

            stmt.o1.u1 = ir_value_code_addr(instr->_ops[0]);
            stmt.o2.u1 = 0;
            stmt.o3.s1 = 0;

            if (ontrue->generated) {
                stmt.opcode = INSTR_IF;
                stmt.o2.s1 = ontrue->code_start - code->statements.size();
                if (stmt.o2.s1 != 1)
                    code_push_statement(code, &stmt, instr->context);
            }
            if (onfalse->generated) {
                stmt.opcode = INSTR_IFNOT;
                stmt.o2.s1 = onfalse->code_start - code->statements.size();
                if (stmt.o2.s1 != 1)
                    code_push_statement(code, &stmt, instr->context);
            }
            if (!ontrue->generated) {
                if (onfalse->generated)
                    return gen_blocks_recursive(code, func, ontrue);
            }
            if (!onfalse->generated) {
                if (ontrue->generated)
                    return gen_blocks_recursive(code, func, onfalse);
            }
            /* neither ontrue nor onfalse exist */
            stmt.opcode = INSTR_IFNOT;
            if (!instr->likely) {
                /* Honor the likelyhood hint */
                ir_block *tmp = onfalse;
                stmt.opcode = INSTR_IF;
                onfalse = ontrue;
                ontrue = tmp;
            }
            stidx = code->statements.size();
            code_push_statement(code, &stmt, instr->context);
            /* on false we jump, so add ontrue-path */
            if (!gen_blocks_recursive(code, func, ontrue))
                return false;
            /* fixup the jump address */
            code->statements[stidx].o2.s1 = code->statements.size() - stidx;
            /* generate onfalse path */
            if (onfalse->generated) {
                /* fixup the jump address */
                code->statements[stidx].o2.s1 = onfalse->code_start - stidx;
                if (stidx+2 == code->statements.size() && code->statements[stidx].o2.s1 == 1) {
                    code->statements[stidx] = code->statements[stidx+1];
                    if (code->statements[stidx].o1.s1 < 0)
                        code->statements[stidx].o1.s1++;
                    code_pop_statement(code);
                }
                stmt.opcode = code->statements.back().opcode;
                if (stmt.opcode == INSTR_GOTO ||
                    stmt.opcode == INSTR_IF ||
                    stmt.opcode == INSTR_IFNOT ||
                    stmt.opcode == INSTR_RETURN ||
                    stmt.opcode == INSTR_DONE)
                {
                    /* no use jumping from here */
                    return true;
                }
                /* may have been generated in the previous recursive call */
                stmt.opcode = INSTR_GOTO;
                stmt.o1.s1 = onfalse->code_start - code->statements.size();
                stmt.o2.s1 = 0;
                stmt.o3.s1 = 0;
                if (stmt.o1.s1 != 1)
                    code_push_statement(code, &stmt, instr->context);
                return true;
            }
            else if (stidx+2 == code->statements.size() && code->statements[stidx].o2.s1 == 1) {
                code->statements[stidx] = code->statements[stidx+1];
                if (code->statements[stidx].o1.s1 < 0)
                    code->statements[stidx].o1.s1++;
                code_pop_statement(code);
            }
            /* if not, generate now */
            return gen_blocks_recursive(code, func, onfalse);
        }

        if ( (instr->opcode >= INSTR_CALL0 && instr->opcode <= INSTR_CALL8)
           || instr->opcode == VINSTR_NRCALL)
        {
            size_t p, first;
            ir_value *retvalue;

            first = instr->params.size();
            if (first > 8)
                first = 8;
            for (p = 0; p < first; ++p)
            {
                ir_value *param = instr->params[p];
                if (param->callparam)
                    continue;

                stmt.opcode = INSTR_STORE_F;
                stmt.o3.u1 = 0;

                if (param->vtype == TYPE_FIELD)
                    stmt.opcode = field_store_instr[param->fieldtype];
                else if (param->vtype == TYPE_NIL)
                    stmt.opcode = INSTR_STORE_V;
                else
                    stmt.opcode = type_store_instr[param->vtype];
                stmt.o1.u1 = ir_value_code_addr(param);
                stmt.o2.u1 = OFS_PARM0 + 3 * p;

                if (param->vtype == TYPE_VECTOR && (param->flags & IR_FLAG_SPLIT_VECTOR)) {
                    /* fetch 3 separate floats */
                    stmt.opcode = INSTR_STORE_F;
                    stmt.o1.u1 = ir_value_code_addr(param->members[0]);
                    code_push_statement(code, &stmt, instr->context);
                    stmt.o2.u1++;
                    stmt.o1.u1 = ir_value_code_addr(param->members[1]);
                    code_push_statement(code, &stmt, instr->context);
                    stmt.o2.u1++;
                    stmt.o1.u1 = ir_value_code_addr(param->members[2]);
                    code_push_statement(code, &stmt, instr->context);
                }
                else
                    code_push_statement(code, &stmt, instr->context);
            }
            /* Now handle extparams */
            first = instr->params.size();
            for (; p < first; ++p)
            {
                ir_builder *ir = func->owner;
                ir_value *param = instr->params[p];
                ir_value *targetparam;

                if (param->callparam)
                    continue;

                if (p-8 >= ir->extparams.size())
                    ir_gen_extparam(ir);

                targetparam = ir->extparams[p-8].get();

                stmt.opcode = INSTR_STORE_F;
                stmt.o3.u1 = 0;

                if (param->vtype == TYPE_FIELD)
                    stmt.opcode = field_store_instr[param->fieldtype];
                else if (param->vtype == TYPE_NIL)
                    stmt.opcode = INSTR_STORE_V;
                else
                    stmt.opcode = type_store_instr[param->vtype];
                stmt.o1.u1 = ir_value_code_addr(param);
                stmt.o2.u1 = ir_value_code_addr(targetparam);
                if (param->vtype == TYPE_VECTOR && (param->flags & IR_FLAG_SPLIT_VECTOR)) {
                    /* fetch 3 separate floats */
                    stmt.opcode = INSTR_STORE_F;
                    stmt.o1.u1 = ir_value_code_addr(param->members[0]);
                    code_push_statement(code, &stmt, instr->context);
                    stmt.o2.u1++;
                    stmt.o1.u1 = ir_value_code_addr(param->members[1]);
                    code_push_statement(code, &stmt, instr->context);
                    stmt.o2.u1++;
                    stmt.o1.u1 = ir_value_code_addr(param->members[2]);
                    code_push_statement(code, &stmt, instr->context);
                }
                else
                    code_push_statement(code, &stmt, instr->context);
            }

            stmt.opcode = INSTR_CALL0 + instr->params.size();
            if (stmt.opcode > INSTR_CALL8)
                stmt.opcode = INSTR_CALL8;
            stmt.o1.u1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o2.u1 = 0;
            stmt.o3.u1 = 0;
            code_push_statement(code, &stmt, instr->context);

            retvalue = instr->_ops[0];
            if (retvalue && retvalue->store != store_return &&
                (retvalue->store == store_global || retvalue->life.size()))
            {
                /* not to be kept in OFS_RETURN */
                if (retvalue->vtype == TYPE_FIELD && OPTS_FLAG(ADJUST_VECTOR_FIELDS))
                    stmt.opcode = field_store_instr[retvalue->fieldtype];
                else
                    stmt.opcode = type_store_instr[retvalue->vtype];
                stmt.o1.u1 = OFS_RETURN;
                stmt.o2.u1 = ir_value_code_addr(retvalue);
                stmt.o3.u1 = 0;
                code_push_statement(code, &stmt, instr->context);
            }
            continue;
        }

        if (instr->opcode == INSTR_STATE) {
            stmt.opcode = instr->opcode;
            if (instr->_ops[0])
                stmt.o1.u1 = ir_value_code_addr(instr->_ops[0]);
            if (instr->_ops[1])
                stmt.o2.u1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o3.u1 = 0;
            code_push_statement(code, &stmt, instr->context);
            continue;
        }

        stmt.opcode = instr->opcode;
        stmt.o1.u1 = 0;
        stmt.o2.u1 = 0;
        stmt.o3.u1 = 0;

        /* This is the general order of operands */
        if (instr->_ops[0])
            stmt.o3.u1 = ir_value_code_addr(instr->_ops[0]);

        if (instr->_ops[1])
            stmt.o1.u1 = ir_value_code_addr(instr->_ops[1]);

        if (instr->_ops[2])
            stmt.o2.u1 = ir_value_code_addr(instr->_ops[2]);

        if (stmt.opcode == INSTR_RETURN || stmt.opcode == INSTR_DONE)
        {
            stmt.o1.u1 = stmt.o3.u1;
            stmt.o3.u1 = 0;
        }
        else if ((stmt.opcode >= INSTR_STORE_F &&
                  stmt.opcode <= INSTR_STORE_FNC) ||
                 (stmt.opcode >= INSTR_STOREP_F &&
                  stmt.opcode <= INSTR_STOREP_FNC))
        {
            /* 2-operand instructions with A -> B */
            stmt.o2.u1 = stmt.o3.u1;
            stmt.o3.u1 = 0;

            /* tiny optimization, don't output
             * STORE a, a
             */
            if (stmt.o2.u1 == stmt.o1.u1 &&
                OPTS_OPTIMIZATION(OPTIM_PEEPHOLE))
            {
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                continue;
            }
        }
        code_push_statement(code, &stmt, instr->context);
    }
    return true;
}

static bool gen_function_code(code_t *code, ir_function *self)
{
    ir_block *block;
    prog_section_statement_t stmt, *retst;

    /* Starting from entry point, we generate blocks "as they come"
     * for now. Dead blocks will not be translated obviously.
     */
    if (self->blocks.empty()) {
        irerror(self->context, "Function '%s' declared without body.", self->name.c_str());
        return false;
    }

    block = self->blocks[0].get();
    if (block->generated)
        return true;

    if (!gen_blocks_recursive(code, self, block)) {
        irerror(self->context, "failed to generate blocks for '%s'", self->name.c_str());
        return false;
    }

    /* code_write and qcvm -disasm need to know that the function ends here */
    retst = &code->statements.back();
    if (OPTS_OPTIMIZATION(OPTIM_VOID_RETURN) &&
        self->outtype == TYPE_VOID &&
        retst->opcode == INSTR_RETURN &&
        !retst->o1.u1 && !retst->o2.u1 && !retst->o3.u1)
    {
        retst->opcode = INSTR_DONE;
        ++opts_optimizationcount[OPTIM_VOID_RETURN];
    } else {
        lex_ctx_t last;

        stmt.opcode = INSTR_DONE;
        stmt.o1.u1  = 0;
        stmt.o2.u1  = 0;
        stmt.o3.u1  = 0;
        last.line   = code->linenums.back();
        last.column = code->columnnums.back();

        code_push_statement(code, &stmt, last);
    }
    return true;
}

static qcint_t ir_builder_filestring(ir_builder *ir, const char *filename)
{
    /* NOTE: filename pointers are copied, we never strdup them,
     * thus we can use pointer-comparison to find the string.
     */
    qcint_t  str;

    for (size_t i = 0; i != ir->filenames.size(); ++i) {
        if (!strcmp(ir->filenames[i], filename))
            return i;
    }

    str = code_genstring(ir->code, filename);
    ir->filenames.push_back(filename);
    ir->filestrings.push_back(str);
    return str;
}

static bool gen_global_function(ir_builder *ir, ir_value *global)
{
    prog_section_function_t fun;
    ir_function            *irfun;

    size_t i;

    if (!global->hasvalue || (!global->constval.vfunc)) {
        irerror(global->context, "Invalid state of function-global: not constant: %s", global->name.c_str());
        return false;
    }

    irfun = global->constval.vfunc;
    fun.name = global->code.name;
    fun.file = ir_builder_filestring(ir, global->context.file);
    fun.profile = 0; /* always 0 */
    fun.nargs = vec_size(irfun->params);
    if (fun.nargs > 8)
        fun.nargs = 8;

    for (i = 0; i < 8; ++i) {
        if ((int32_t)i >= fun.nargs)
            fun.argsize[i] = 0;
        else
            fun.argsize[i] = type_sizeof_[irfun->params[i]];
    }

    fun.firstlocal = 0;
    fun.locals = irfun->allocated_locals;

    if (irfun->builtin)
        fun.entry = irfun->builtin+1;
    else {
        irfun->code_function_def = ir->code->functions.size();
        fun.entry = ir->code->statements.size();
    }

    ir->code->functions.push_back(fun);
    return true;
}

static ir_value* ir_gen_extparam_proto(ir_builder *ir)
{
    char      name[128];

    util_snprintf(name, sizeof(name), "EXTPARM#%i", (int)(ir->extparam_protos.size()));
    ir_value *global = new ir_value(name, store_global, TYPE_VECTOR);
    ir->extparam_protos.emplace_back(global);

    return global;
}

static void ir_gen_extparam(ir_builder *ir)
{
    prog_section_def_t def;
    ir_value          *global;

    if (ir->extparam_protos.size() < ir->extparams.size()+1)
        global = ir_gen_extparam_proto(ir);
    else
        global = ir->extparam_protos[ir->extparams.size()].get();

    def.name = code_genstring(ir->code, global->name.c_str());
    def.type = TYPE_VECTOR;
    def.offset = ir->code->globals.size();

    ir->code->defs.push_back(def);

    ir_value_code_setaddr(global, def.offset);

    ir->code->globals.push_back(0);
    ir->code->globals.push_back(0);
    ir->code->globals.push_back(0);

    ir->extparams.emplace_back(global);
}

static bool gen_function_extparam_copy(code_t *code, ir_function *self)
{
    ir_builder *ir = self->owner;

    size_t numparams = vec_size(self->params);
    if (!numparams)
        return true;

    prog_section_statement_t stmt;
    stmt.opcode = INSTR_STORE_F;
    stmt.o3.s1 = 0;
    for (size_t i = 8; i < numparams; ++i) {
        size_t ext = i - 8;
        if (ext >= ir->extparams.size())
            ir_gen_extparam(ir);

        ir_value *ep = ir->extparams[ext].get();

        stmt.opcode = type_store_instr[self->locals[i]->vtype];
        if (self->locals[i]->vtype == TYPE_FIELD &&
            self->locals[i]->fieldtype == TYPE_VECTOR)
        {
            stmt.opcode = INSTR_STORE_V;
        }
        stmt.o1.u1 = ir_value_code_addr(ep);
        stmt.o2.u1 = ir_value_code_addr(self->locals[i].get());
        code_push_statement(code, &stmt, self->context);
    }

    return true;
}

static bool gen_function_varargs_copy(code_t *code, ir_function *self)
{
    size_t i, ext, numparams, maxparams;

    ir_builder *ir = self->owner;
    ir_value   *ep;
    prog_section_statement_t stmt;

    numparams = vec_size(self->params);
    if (!numparams)
        return true;

    stmt.opcode = INSTR_STORE_V;
    stmt.o3.s1 = 0;
    maxparams = numparams + self->max_varargs;
    for (i = numparams; i < maxparams; ++i) {
        if (i < 8) {
            stmt.o1.u1 = OFS_PARM0 + 3*i;
            stmt.o2.u1 = ir_value_code_addr(self->locals[i].get());
            code_push_statement(code, &stmt, self->context);
            continue;
        }
        ext = i - 8;
        while (ext >= ir->extparams.size())
            ir_gen_extparam(ir);

        ep = ir->extparams[ext].get();

        stmt.o1.u1 = ir_value_code_addr(ep);
        stmt.o2.u1 = ir_value_code_addr(self->locals[i].get());
        code_push_statement(code, &stmt, self->context);
    }

    return true;
}

static bool gen_function_locals(ir_builder *ir, ir_value *global)
{
    prog_section_function_t *def;
    ir_function             *irfun;
    uint32_t                 firstlocal, firstglobal;

    irfun = global->constval.vfunc;
    def   = &ir->code->functions[0] + irfun->code_function_def;

    if (OPTS_OPTION_BOOL(OPTION_G) ||
        !OPTS_OPTIMIZATION(OPTIM_OVERLAP_LOCALS)        ||
        (irfun->flags & IR_FLAG_MASK_NO_OVERLAP))
    {
        firstlocal = def->firstlocal = ir->code->globals.size();
    } else {
        firstlocal = def->firstlocal = ir->first_common_local;
        ++opts_optimizationcount[OPTIM_OVERLAP_LOCALS];
    }

    firstglobal = (OPTS_OPTIMIZATION(OPTIM_GLOBAL_TEMPS) ? ir->first_common_globaltemp : firstlocal);

    for (size_t i = ir->code->globals.size(); i < firstlocal + irfun->allocated_locals; ++i)
        ir->code->globals.push_back(0);

    for (auto& lp : irfun->locals) {
        ir_value *v = lp.get();
        if (v->locked || !OPTS_OPTIMIZATION(OPTIM_GLOBAL_TEMPS)) {
            ir_value_code_setaddr(v, firstlocal + v->code.local);
            if (!ir_builder_gen_global(ir, v, true)) {
                irerror(v->context, "failed to generate local %s", v->name.c_str());
                return false;
            }
        }
        else
            ir_value_code_setaddr(v, firstglobal + v->code.local);
    }
    for (auto& vp : irfun->values) {
        ir_value *v = vp.get();
        if (v->callparam)
            continue;
        if (v->locked)
            ir_value_code_setaddr(v, firstlocal + v->code.local);
        else
            ir_value_code_setaddr(v, firstglobal + v->code.local);
    }
    return true;
}

static bool gen_global_function_code(ir_builder *ir, ir_value *global)
{
    prog_section_function_t *fundef;
    ir_function             *irfun;

    (void)ir;

    irfun = global->constval.vfunc;
    if (!irfun) {
        if (global->cvq == CV_NONE) {
            if (irwarning(global->context, WARN_IMPLICIT_FUNCTION_POINTER,
                          "function `%s` has no body and in QC implicitly becomes a function-pointer",
                          global->name.c_str()))
            {
                /* Not bailing out just now. If this happens a lot you don't want to have
                 * to rerun gmqcc for each such function.
                 */

                /* return false; */
            }
        }
        /* this was a function pointer, don't generate code for those */
        return true;
    }

    if (irfun->builtin)
        return true;

    /*
     * If there is no definition and the thing is eraseable, we can ignore
     * outputting the function to begin with.
     */
    if (global->flags & IR_FLAG_ERASABLE && irfun->code_function_def < 0) {
        return true;
    }

    if (irfun->code_function_def < 0) {
        irerror(irfun->context, "`%s`: IR global wasn't generated, failed to access function-def", irfun->name.c_str());
        return false;
    }
    fundef = &ir->code->functions[irfun->code_function_def];

    fundef->entry = ir->code->statements.size();
    if (!gen_function_locals(ir, global)) {
        irerror(irfun->context, "Failed to generate locals for function %s", irfun->name.c_str());
        return false;
    }
    if (!gen_function_extparam_copy(ir->code, irfun)) {
        irerror(irfun->context, "Failed to generate extparam-copy code for function %s", irfun->name.c_str());
        return false;
    }
    if (irfun->max_varargs && !gen_function_varargs_copy(ir->code, irfun)) {
        irerror(irfun->context, "Failed to generate vararg-copy code for function %s", irfun->name.c_str());
        return false;
    }
    if (!gen_function_code(ir->code, irfun)) {
        irerror(irfun->context, "Failed to generate code for function %s", irfun->name.c_str());
        return false;
    }
    return true;
}

static void gen_vector_defs(code_t *code, prog_section_def_t def, const char *name)
{
    char  *component;
    size_t len, i;

    if (!name || name[0] == '#' || OPTS_FLAG(SINGLE_VECTOR_DEFS))
        return;

    def.type = TYPE_FLOAT;

    len = strlen(name);

    component = (char*)mem_a(len+3);
    memcpy(component, name, len);
    len += 2;
    component[len-0] = 0;
    component[len-2] = '_';

    component[len-1] = 'x';

    for (i = 0; i < 3; ++i) {
        def.name = code_genstring(code, component);
        code->defs.push_back(def);
        def.offset++;
        component[len-1]++;
    }

    mem_d(component);
}

static void gen_vector_fields(code_t *code, prog_section_field_t fld, const char *name)
{
    char  *component;
    size_t len, i;

    if (!name || OPTS_FLAG(SINGLE_VECTOR_DEFS))
        return;

    fld.type = TYPE_FLOAT;

    len = strlen(name);

    component = (char*)mem_a(len+3);
    memcpy(component, name, len);
    len += 2;
    component[len-0] = 0;
    component[len-2] = '_';

    component[len-1] = 'x';

    for (i = 0; i < 3; ++i) {
        fld.name = code_genstring(code, component);
        code->fields.push_back(fld);
        fld.offset++;
        component[len-1]++;
    }

    mem_d(component);
}

static bool ir_builder_gen_global(ir_builder *self, ir_value *global, bool islocal)
{
    size_t             i;
    int32_t           *iptr;
    prog_section_def_t def;
    bool               pushdef = opts.optimizeoff;

    /* we don't generate split-vectors */
    if (global->vtype == TYPE_VECTOR && (global->flags & IR_FLAG_SPLIT_VECTOR))
        return true;

    def.type = global->vtype;
    def.offset = self->code->globals.size();
    def.name = 0;
    if (OPTS_OPTION_BOOL(OPTION_G) || !islocal)
    {
        pushdef = true;

        /*
         * if we're eraseable and the function isn't referenced ignore outputting
         * the function.
         */
        if (global->flags & IR_FLAG_ERASABLE && global->reads.empty()) {
            return true;
        }

        if (OPTS_OPTIMIZATION(OPTIM_STRIP_CONSTANT_NAMES) &&
            !(global->flags & IR_FLAG_INCLUDE_DEF) &&
            (global->name[0] == '#' || global->cvq == CV_CONST))
        {
            pushdef = false;
        }

        if (pushdef) {
            if (global->name[0] == '#') {
                if (!self->str_immediate)
                    self->str_immediate = code_genstring(self->code, "IMMEDIATE");
                def.name = global->code.name = self->str_immediate;
            }
            else
                def.name = global->code.name = code_genstring(self->code, global->name.c_str());
        }
        else
            def.name   = 0;
        if (islocal) {
            def.offset = ir_value_code_addr(global);
            self->code->defs.push_back(def);
            if (global->vtype == TYPE_VECTOR)
                gen_vector_defs(self->code, def, global->name.c_str());
            else if (global->vtype == TYPE_FIELD && global->fieldtype == TYPE_VECTOR)
                gen_vector_defs(self->code, def, global->name.c_str());
            return true;
        }
    }
    if (islocal)
        return true;

    switch (global->vtype)
    {
    case TYPE_VOID:
        if (0 == global->name.compare("end_sys_globals")) {
            // TODO: remember this point... all the defs before this one
            // should be checksummed and added to progdefs.h when we generate it.
        }
        else if (0 == global->name.compare("end_sys_fields")) {
            // TODO: same as above but for entity-fields rather than globsl
        }
        else if(irwarning(global->context, WARN_VOID_VARIABLES, "unrecognized variable of type void `%s`",
                          global->name.c_str()))
        {
            /* Not bailing out */
            /* return false; */
        }
        /* I'd argue setting it to 0 is sufficient, but maybe some depend on knowing how far
         * the system fields actually go? Though the engine knows this anyway...
         * Maybe this could be an -foption
         * fteqcc creates data for end_sys_* - of size 1, so let's do the same
         */
        ir_value_code_setaddr(global, self->code->globals.size());
        self->code->globals.push_back(0);
        /* Add the def */
        if (pushdef) self->code->defs.push_back(def);
        return true;
    case TYPE_POINTER:
        if (pushdef) self->code->defs.push_back(def);
        return gen_global_pointer(self->code, global);
    case TYPE_FIELD:
        if (pushdef) {
            self->code->defs.push_back(def);
            if (global->fieldtype == TYPE_VECTOR)
                gen_vector_defs(self->code, def, global->name.c_str());
        }
        return gen_global_field(self->code, global);
    case TYPE_ENTITY:
        /* fall through */
    case TYPE_FLOAT:
    {
        ir_value_code_setaddr(global, self->code->globals.size());
        if (global->hasvalue) {
            iptr = (int32_t*)&global->constval.ivec[0];
            self->code->globals.push_back(*iptr);
        } else {
            self->code->globals.push_back(0);
        }
        if (!islocal && global->cvq != CV_CONST)
            def.type |= DEF_SAVEGLOBAL;
        if (pushdef) self->code->defs.push_back(def);

        return global->code.globaladdr >= 0;
    }
    case TYPE_STRING:
    {
        ir_value_code_setaddr(global, self->code->globals.size());
        if (global->hasvalue) {
            uint32_t load = code_genstring(self->code, global->constval.vstring);
            self->code->globals.push_back(load);
        } else {
            self->code->globals.push_back(0);
        }
        if (!islocal && global->cvq != CV_CONST)
            def.type |= DEF_SAVEGLOBAL;
        if (pushdef) self->code->defs.push_back(def);
        return global->code.globaladdr >= 0;
    }
    case TYPE_VECTOR:
    {
        size_t d;
        ir_value_code_setaddr(global, self->code->globals.size());
        if (global->hasvalue) {
            iptr = (int32_t*)&global->constval.ivec[0];
            self->code->globals.push_back(iptr[0]);
            if (global->code.globaladdr < 0)
                return false;
            for (d = 1; d < type_sizeof_[global->vtype]; ++d) {
                self->code->globals.push_back(iptr[d]);
            }
        } else {
            self->code->globals.push_back(0);
            if (global->code.globaladdr < 0)
                return false;
            for (d = 1; d < type_sizeof_[global->vtype]; ++d) {
                self->code->globals.push_back(0);
            }
        }
        if (!islocal && global->cvq != CV_CONST)
            def.type |= DEF_SAVEGLOBAL;

        if (pushdef) {
            self->code->defs.push_back(def);
            def.type &= ~DEF_SAVEGLOBAL;
            gen_vector_defs(self->code, def, global->name.c_str());
        }
        return global->code.globaladdr >= 0;
    }
    case TYPE_FUNCTION:
        ir_value_code_setaddr(global, self->code->globals.size());
        if (!global->hasvalue) {
            self->code->globals.push_back(0);
            if (global->code.globaladdr < 0)
                return false;
        } else {
            self->code->globals.push_back(self->code->functions.size());
            if (!gen_global_function(self, global))
                return false;
        }
        if (!islocal && global->cvq != CV_CONST)
            def.type |= DEF_SAVEGLOBAL;
        if (pushdef) self->code->defs.push_back(def);
        return true;
    case TYPE_VARIANT:
        /* assume biggest type */
            ir_value_code_setaddr(global, self->code->globals.size());
            self->code->globals.push_back(0);
            for (i = 1; i < type_sizeof_[TYPE_VARIANT]; ++i)
                self->code->globals.push_back(0);
            return true;
    default:
        /* refuse to create 'void' type or any other fancy business. */
        irerror(global->context, "Invalid type for global variable `%s`: %s",
                global->name.c_str(), type_name[global->vtype]);
        return false;
    }
}

static GMQCC_INLINE void ir_builder_prepare_field(code_t *code, ir_value *field)
{
    field->code.fieldaddr = code_alloc_field(code, type_sizeof_[field->fieldtype]);
}

static bool ir_builder_gen_field(ir_builder *self, ir_value *field)
{
    prog_section_def_t def;
    prog_section_field_t fld;

    (void)self;

    def.type   = (uint16_t)field->vtype;
    def.offset = (uint16_t)self->code->globals.size();

    /* create a global named the same as the field */
    if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_GMQCC) {
        /* in our standard, the global gets a dot prefix */
        size_t len = field->name.length();
        char name[1024];

        /* we really don't want to have to allocate this, and 1024
         * bytes is more than enough for a variable/field name
         */
        if (len+2 >= sizeof(name)) {
            irerror(field->context, "invalid field name size: %u", (unsigned int)len);
            return false;
        }

        name[0] = '.';
        memcpy(name+1, field->name.c_str(), len); // no strncpy - we used strlen above
        name[len+1] = 0;

        def.name = code_genstring(self->code, name);
        fld.name = def.name + 1; /* we reuse that string table entry */
    } else {
        /* in plain QC, there cannot be a global with the same name,
         * and so we also name the global the same.
         * FIXME: fteqcc should create a global as well
         * check if it actually uses the same name. Probably does
         */
        def.name = code_genstring(self->code, field->name.c_str());
        fld.name = def.name;
    }

    field->code.name = def.name;

    self->code->defs.push_back(def);

    fld.type = field->fieldtype;

    if (fld.type == TYPE_VOID) {
        irerror(field->context, "field is missing a type: %s - don't know its size", field->name.c_str());
        return false;
    }

    fld.offset = field->code.fieldaddr;

    self->code->fields.push_back(fld);

    ir_value_code_setaddr(field, self->code->globals.size());
    self->code->globals.push_back(fld.offset);
    if (fld.type == TYPE_VECTOR) {
        self->code->globals.push_back(fld.offset+1);
        self->code->globals.push_back(fld.offset+2);
    }

    if (field->fieldtype == TYPE_VECTOR) {
        gen_vector_defs  (self->code, def, field->name.c_str());
        gen_vector_fields(self->code, fld, field->name.c_str());
    }

    return field->code.globaladdr >= 0;
}

static void ir_builder_collect_reusables(ir_builder *builder) {
    std::vector<ir_value*> reusables;

    for (auto& gp : builder->globals) {
        ir_value *value = gp.get();
        if (value->vtype != TYPE_FLOAT || !value->hasvalue)
            continue;
        if (value->cvq == CV_CONST || (value->name.length() >= 1 && value->name[0] == '#'))
            reusables.emplace_back(value);
    }
    builder->const_floats = move(reusables);
}

static void ir_builder_split_vector(ir_builder *self, ir_value *vec) {
    ir_value* found[3] = { nullptr, nullptr, nullptr };

    // must not be written to
    if (vec->writes.size())
        return;
    // must not be trying to access individual members
    if (vec->members[0] || vec->members[1] || vec->members[2])
        return;
    // should be actually used otherwise it won't be generated anyway
    if (vec->reads.empty())
        return;
    //size_t count = vec->reads.size();
    //if (!count)
    //    return;

    // may only be used directly as function parameters, so if we find some other instruction cancel
    for (ir_instr *user : vec->reads) {
        // we only split vectors if they're used directly as parameter to a call only!
        if ((user->opcode < INSTR_CALL0 || user->opcode > INSTR_CALL8) && user->opcode != VINSTR_NRCALL)
            return;
    }

    vec->flags |= IR_FLAG_SPLIT_VECTOR;

    // find existing floats making up the split
    for (ir_value *c : self->const_floats) {
        if (!found[0] && c->constval.vfloat == vec->constval.vvec.x)
            found[0] = c;
        if (!found[1] && c->constval.vfloat == vec->constval.vvec.y)
            found[1] = c;
        if (!found[2] && c->constval.vfloat == vec->constval.vvec.z)
            found[2] = c;
        if (found[0] && found[1] && found[2])
            break;
    }

    // generate floats for not yet found components
    if (!found[0])
        found[0] = ir_builder_imm_float(self, vec->constval.vvec.x, true);
    if (!found[1]) {
        if (vec->constval.vvec.y == vec->constval.vvec.x)
            found[1] = found[0];
        else
            found[1] = ir_builder_imm_float(self, vec->constval.vvec.y, true);
    }
    if (!found[2]) {
        if (vec->constval.vvec.z == vec->constval.vvec.x)
            found[2] = found[0];
        else if (vec->constval.vvec.z == vec->constval.vvec.y)
            found[2] = found[1];
        else
            found[2] = ir_builder_imm_float(self, vec->constval.vvec.z, true);
    }

    // the .members array should be safe to use here
    vec->members[0] = found[0];
    vec->members[1] = found[1];
    vec->members[2] = found[2];

    // register the readers for these floats
    found[0]->reads.insert(found[0]->reads.end(), vec->reads.begin(), vec->reads.end());
    found[1]->reads.insert(found[1]->reads.end(), vec->reads.begin(), vec->reads.end());
    found[2]->reads.insert(found[2]->reads.end(), vec->reads.begin(), vec->reads.end());
}

static void ir_builder_split_vectors(ir_builder *self) {
    for (auto& gp : self->globals) {
        ir_value *v = gp.get();
        if (v->vtype != TYPE_VECTOR || !v->name.length() || v->name[0] != '#')
            continue;
        ir_builder_split_vector(self, v);
    }
}

bool ir_builder_generate(ir_builder *self, const char *filename)
{
    prog_section_statement_t stmt;
    char  *lnofile = nullptr;

    if (OPTS_FLAG(SPLIT_VECTOR_PARAMETERS)) {
        ir_builder_collect_reusables(self);
        if (!self->const_floats.empty())
            ir_builder_split_vectors(self);
    }

    for (auto& fp : self->fields)
        ir_builder_prepare_field(self->code, fp.get());

    for (auto& gp : self->globals) {
        ir_value *global = gp.get();
        if (!ir_builder_gen_global(self, global, false)) {
            return false;
        }
        if (global->vtype == TYPE_FUNCTION) {
            ir_function *func = global->constval.vfunc;
            if (func && self->max_locals < func->allocated_locals &&
                !(func->flags & IR_FLAG_MASK_NO_OVERLAP))
            {
                self->max_locals = func->allocated_locals;
            }
            if (func && self->max_globaltemps < func->globaltemps)
                self->max_globaltemps = func->globaltemps;
        }
    }

    for (auto& fp : self->fields) {
        if (!ir_builder_gen_field(self, fp.get()))
            return false;
    }

    // generate nil
    ir_value_code_setaddr(self->nil, self->code->globals.size());
    self->code->globals.push_back(0);
    self->code->globals.push_back(0);
    self->code->globals.push_back(0);

    // generate virtual-instruction temps
    for (size_t i = 0; i < IR_MAX_VINSTR_TEMPS; ++i) {
        ir_value_code_setaddr(self->vinstr_temp[i], self->code->globals.size());
        self->code->globals.push_back(0);
        self->code->globals.push_back(0);
        self->code->globals.push_back(0);
    }

    // generate global temps
    self->first_common_globaltemp = self->code->globals.size();
    self->code->globals.insert(self->code->globals.end(), self->max_globaltemps, 0);
    // FIXME:DELME:
    //for (size_t i = 0; i < self->max_globaltemps; ++i) {
    //    self->code->globals.push_back(0);
    //}
    // generate common locals
    self->first_common_local = self->code->globals.size();
    self->code->globals.insert(self->code->globals.end(), self->max_locals, 0);
    // FIXME:DELME:
    //for (i = 0; i < self->max_locals; ++i) {
    //    self->code->globals.push_back(0);
    //}

    // generate function code

    for (auto& gp : self->globals) {
        ir_value *global = gp.get();
        if (global->vtype == TYPE_FUNCTION) {
            if (!gen_global_function_code(self, global)) {
                return false;
            }
        }
    }

    if (self->code->globals.size() >= 65536) {
        irerror(self->globals.back()->context,
            "This progs file would require more globals than the metadata can handle (%zu). Bailing out.",
            self->code->globals.size());
        return false;
    }

    /* DP errors if the last instruction is not an INSTR_DONE. */
    if (self->code->statements.back().opcode != INSTR_DONE)
    {
        lex_ctx_t last;

        stmt.opcode = INSTR_DONE;
        stmt.o1.u1  = 0;
        stmt.o2.u1  = 0;
        stmt.o3.u1  = 0;
        last.line   = self->code->linenums.back();
        last.column = self->code->columnnums.back();

        code_push_statement(self->code, &stmt, last);
    }

    if (OPTS_OPTION_BOOL(OPTION_PP_ONLY))
        return true;

    if (self->code->statements.size() != self->code->linenums.size()) {
        con_err("Linecounter wrong: %lu != %lu\n",
                self->code->statements.size(),
                self->code->linenums.size());
    } else if (OPTS_FLAG(LNO)) {
        char  *dot;
        size_t filelen = strlen(filename);

        memcpy(vec_add(lnofile, filelen+1), filename, filelen+1);
        dot = strrchr(lnofile, '.');
        if (!dot) {
            vec_pop(lnofile);
        } else {
            vec_shrinkto(lnofile, dot - lnofile);
        }
        memcpy(vec_add(lnofile, 5), ".lno", 5);
    }

    if (!code_write(self->code, filename, lnofile)) {
        vec_free(lnofile);
        return false;
    }

    vec_free(lnofile);
    return true;
}

/***********************************************************************
 *IR DEBUG Dump functions...
 */

#define IND_BUFSZ 1024

static const char *qc_opname(int op)
{
    if (op < 0) return "<INVALID>";
    if (op < VINSTR_END)
        return util_instr_str[op];
    switch (op) {
        case VINSTR_END:       return "END";
        case VINSTR_PHI:       return "PHI";
        case VINSTR_JUMP:      return "JUMP";
        case VINSTR_COND:      return "COND";
        case VINSTR_BITXOR:    return "BITXOR";
        case VINSTR_BITAND_V:  return "BITAND_V";
        case VINSTR_BITOR_V:   return "BITOR_V";
        case VINSTR_BITXOR_V:  return "BITXOR_V";
        case VINSTR_BITAND_VF: return "BITAND_VF";
        case VINSTR_BITOR_VF:  return "BITOR_VF";
        case VINSTR_BITXOR_VF: return "BITXOR_VF";
        case VINSTR_CROSS:     return "CROSS";
        case VINSTR_NEG_F:     return "NEG_F";
        case VINSTR_NEG_V:     return "NEG_V";
        default:               return "<UNK>";
    }
}

void ir_builder_dump(ir_builder *b, int (*oprintf)(const char*, ...))
{
    size_t i;
    char indent[IND_BUFSZ];
    indent[0] = '\t';
    indent[1] = 0;

    oprintf("module %s\n", b->name.c_str());
    for (i = 0; i < b->globals.size(); ++i)
    {
        oprintf("global ");
        if (b->globals[i]->hasvalue)
            oprintf("%s = ", b->globals[i]->name.c_str());
        ir_value_dump(b->globals[i].get(), oprintf);
        oprintf("\n");
    }
    for (i = 0; i < b->functions.size(); ++i)
        ir_function_dump(b->functions[i].get(), indent, oprintf);
    oprintf("endmodule %s\n", b->name.c_str());
}

static const char *storenames[] = {
    "[global]", "[local]", "[param]", "[value]", "[return]"
};

void ir_function_dump(ir_function *f, char *ind,
                      int (*oprintf)(const char*, ...))
{
    size_t i;
    if (f->builtin != 0) {
        oprintf("%sfunction %s = builtin %i\n", ind, f->name.c_str(), -f->builtin);
        return;
    }
    oprintf("%sfunction %s\n", ind, f->name.c_str());
    util_strncat(ind, "\t", IND_BUFSZ-1);
    if (f->locals.size())
    {
        oprintf("%s%i locals:\n", ind, (int)f->locals.size());
        for (i = 0; i < f->locals.size(); ++i) {
            oprintf("%s\t", ind);
            ir_value_dump(f->locals[i].get(), oprintf);
            oprintf("\n");
        }
    }
    oprintf("%sliferanges:\n", ind);
    for (i = 0; i < f->locals.size(); ++i) {
        const char *attr = "";
        size_t l, m;
        ir_value *v = f->locals[i].get();
        if (v->unique_life && v->locked)
            attr = "unique,locked ";
        else if (v->unique_life)
            attr = "unique ";
        else if (v->locked)
            attr = "locked ";
        oprintf("%s\t%s: %s %s %s%s@%i ", ind, v->name.c_str(), type_name[v->vtype],
                storenames[v->store],
                attr, (v->callparam ? "callparam " : ""),
                (int)v->code.local);
        if (v->life.empty())
            oprintf("[null]");
        for (l = 0; l < v->life.size(); ++l) {
            oprintf("[%i,%i] ", v->life[l].start, v->life[l].end);
        }
        oprintf("\n");
        for (m = 0; m < 3; ++m) {
            ir_value *vm = v->members[m];
            if (!vm)
                continue;
            oprintf("%s\t%s: @%i ", ind, vm->name.c_str(), (int)vm->code.local);
            for (l = 0; l < vm->life.size(); ++l) {
                oprintf("[%i,%i] ", vm->life[l].start, vm->life[l].end);
            }
            oprintf("\n");
        }
    }
    for (i = 0; i < f->values.size(); ++i) {
        const char *attr = "";
        size_t l, m;
        ir_value *v = f->values[i].get();
        if (v->unique_life && v->locked)
            attr = "unique,locked ";
        else if (v->unique_life)
            attr = "unique ";
        else if (v->locked)
            attr = "locked ";
        oprintf("%s\t%s: %s %s %s%s@%i ", ind, v->name.c_str(), type_name[v->vtype],
                storenames[v->store],
                attr, (v->callparam ? "callparam " : ""),
                (int)v->code.local);
        if (v->life.empty())
            oprintf("[null]");
        for (l = 0; l < v->life.size(); ++l) {
            oprintf("[%i,%i] ", v->life[l].start, v->life[l].end);
        }
        oprintf("\n");
        for (m = 0; m < 3; ++m) {
            ir_value *vm = v->members[m];
            if (!vm)
                continue;
            if (vm->unique_life && vm->locked)
                attr = "unique,locked ";
            else if (vm->unique_life)
                attr = "unique ";
            else if (vm->locked)
                attr = "locked ";
            oprintf("%s\t%s: %s@%i ", ind, vm->name.c_str(), attr, (int)vm->code.local);
            for (l = 0; l < vm->life.size(); ++l) {
                oprintf("[%i,%i] ", vm->life[l].start, vm->life[l].end);
            }
            oprintf("\n");
        }
    }
    if (f->blocks.size())
    {
        oprintf("%slife passes: %i\n", ind, (int)f->run_id);
        for (i = 0; i < f->blocks.size(); ++i) {
            ir_block_dump(f->blocks[i].get(), ind, oprintf);
        }

    }
    ind[strlen(ind)-1] = 0;
    oprintf("%sendfunction %s\n", ind, f->name.c_str());
}

void ir_block_dump(ir_block* b, char *ind,
                   int (*oprintf)(const char*, ...))
{
    size_t i;
    oprintf("%s:%s\n", ind, b->label.c_str());
    util_strncat(ind, "\t", IND_BUFSZ-1);

    if (b->instr && b->instr[0])
        oprintf("%s (%i) [entry]\n", ind, (int)(b->instr[0]->eid-1));
    for (i = 0; i < vec_size(b->instr); ++i)
        ir_instr_dump(b->instr[i], ind, oprintf);
    ind[strlen(ind)-1] = 0;
}

static void dump_phi(ir_instr *in, int (*oprintf)(const char*, ...))
{
    oprintf("%s <- phi ", in->_ops[0]->name.c_str());
    for (auto &it : in->phi) {
        oprintf("([%s] : %s) ", it.from->label.c_str(),
                                it.value->name.c_str());
    }
    oprintf("\n");
}

void ir_instr_dump(ir_instr *in, char *ind,
                       int (*oprintf)(const char*, ...))
{
    size_t i;
    const char *comma = nullptr;

    oprintf("%s (%i) ", ind, (int)in->eid);

    if (in->opcode == VINSTR_PHI) {
        dump_phi(in, oprintf);
        return;
    }

    util_strncat(ind, "\t", IND_BUFSZ-1);

    if (in->_ops[0] && (in->_ops[1] || in->_ops[2])) {
        ir_value_dump(in->_ops[0], oprintf);
        if (in->_ops[1] || in->_ops[2])
            oprintf(" <- ");
    }
    if (in->opcode == INSTR_CALL0 || in->opcode == VINSTR_NRCALL) {
        oprintf("CALL%i\t", in->params.size());
    } else
        oprintf("%s\t", qc_opname(in->opcode));

    if (in->_ops[0] && !(in->_ops[1] || in->_ops[2])) {
        ir_value_dump(in->_ops[0], oprintf);
        comma = ",\t";
    }
    else
    {
        for (i = 1; i != 3; ++i) {
            if (in->_ops[i]) {
                if (comma)
                    oprintf(comma);
                ir_value_dump(in->_ops[i], oprintf);
                comma = ",\t";
            }
        }
    }
    if (in->bops[0]) {
        if (comma)
            oprintf(comma);
        oprintf("[%s]", in->bops[0]->label.c_str());
        comma = ",\t";
    }
    if (in->bops[1])
        oprintf("%s[%s]", comma, in->bops[1]->label.c_str());
    if (in->params.size()) {
        oprintf("\tparams: ");
        for (auto &it : in->params)
            oprintf("%s, ", it->name.c_str());
    }
    oprintf("\n");
    ind[strlen(ind)-1] = 0;
}

static void ir_value_dump_string(const char *str, int (*oprintf)(const char*, ...))
{
    oprintf("\"");
    for (; *str; ++str) {
        switch (*str) {
            case '\n': oprintf("\\n"); break;
            case '\r': oprintf("\\r"); break;
            case '\t': oprintf("\\t"); break;
            case '\v': oprintf("\\v"); break;
            case '\f': oprintf("\\f"); break;
            case '\b': oprintf("\\b"); break;
            case '\a': oprintf("\\a"); break;
            case '\\': oprintf("\\\\"); break;
            case '"': oprintf("\\\""); break;
            default: oprintf("%c", *str); break;
        }
    }
    oprintf("\"");
}

void ir_value_dump(ir_value* v, int (*oprintf)(const char*, ...))
{
    if (v->hasvalue) {
        switch (v->vtype) {
            default:
            case TYPE_VOID:
                oprintf("(void)");
                break;
            case TYPE_FUNCTION:
                oprintf("fn:%s", v->name.c_str());
                break;
            case TYPE_FLOAT:
                oprintf("%g", v->constval.vfloat);
                break;
            case TYPE_VECTOR:
                oprintf("'%g %g %g'",
                        v->constval.vvec.x,
                        v->constval.vvec.y,
                        v->constval.vvec.z);
                break;
            case TYPE_ENTITY:
                oprintf("(entity)");
                break;
            case TYPE_STRING:
                ir_value_dump_string(v->constval.vstring, oprintf);
                break;
#if 0
            case TYPE_INTEGER:
                oprintf("%i", v->constval.vint);
                break;
#endif
            case TYPE_POINTER:
                oprintf("&%s",
                    v->constval.vpointer->name.c_str());
                break;
        }
    } else {
        oprintf("%s", v->name.c_str());
    }
}

void ir_value_dump_life(const ir_value *self, int (*oprintf)(const char*,...))
{
    oprintf("Life of %12s:", self->name.c_str());
    for (size_t i = 0; i < self->life.size(); ++i)
    {
        oprintf(" + [%i, %i]\n", self->life[i].start, self->life[i].end);
    }
}
