/*
/+----------------------------------------------------------------------+
 | AOP                                                                  |
 +----------------------------------------------------------------------+
 | Copyright (c) 2012 Julien Salleyron, Gérald Croës                    |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt.                                 |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Author: Julien Salleyron <julien.salleyron@gmail.com>                |
 +----------------------------------------------------------------------+
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "main/php_ini.h"
#include "ext/standard/php_string.h"
#include "ext/pcre/php_pcre.h"
#include "Lexer.h"
#include "aop_joinpoint.h"
#include "aop.h"
#include "Zend/zend_operators.h"

ZEND_DECLARE_MODULE_GLOBALS(aop)

#define DEBUG_OBJECT_HANDLERS 1

static void php_aop_init_globals(zend_aop_globals *aop_globals)
{
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_add, 0, 0, 2) 
    ZEND_ARG_INFO(0,pointcut)
    ZEND_ARG_INFO(0,advice)
ZEND_END_ARG_INFO()


static zend_function_entry aop_functions[] =
{
    PHP_FE(aop_add_around, arginfo_aop_add)
    PHP_FE(aop_add_before,  arginfo_aop_add)
    PHP_FE(aop_add_after, arginfo_aop_add)
    PHP_FE(aop_add_after_returning, arginfo_aop_add)
    PHP_FE(aop_add_after_throwing, arginfo_aop_add)
    {NULL, NULL, NULL}
};

zend_module_entry aop_module_entry =
{
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_AOP_EXTNAME,
    aop_functions,
    PHP_MINIT(aop),
    PHP_MSHUTDOWN(aop),
    PHP_RINIT(aop),
    PHP_RSHUTDOWN(aop),
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_AOP_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_AOP
ZEND_GET_MODULE(aop)
#endif

static zend_class_entry* aop_class_entry;
static zend_class_entry* aop_const_class_entry;

zend_object_handlers AopJoinpoint_object_handlers;

static void aop_free_object(zend_object *object)
{
    AopJoinpoint_object *obj = (AopJoinpoint_object *)((char *)object - XtOffsetOf(AopJoinpoint_object, std));
    if (obj->value != NULL) {
        zval_ptr_dtor(&obj->value);
    }
    if (obj->args != NULL) {
        zval_ptr_dtor(&obj->args);
    }
    zend_object_std_dtor(&obj->std);
}

static zend_object *aop_create_object(zend_class_entry *type)
{
    AopJoinpoint_object *obj = (AopJoinpoint_object *)ecalloc(1, sizeof(AopJoinpoint_object));
    zend_object_std_init(&obj->std, type);
    object_properties_init(&obj->std, type);
    
    obj->value = NULL;
    obj->args = NULL;
    obj->std.handlers = &AopJoinpoint_object_handlers;
    
    return &obj->std;
}

ZEND_BEGIN_ARG_INFO(arginfo_aop_args_setArguments, 0)
    ZEND_ARG_ARRAY_INFO(0, arguments, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_aop_args_setReturnedValue, 0)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_aop_args_setAssignedValue, 0)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_args_returnbyref, 0, ZEND_RETURN_REFERENCE, -1)
ZEND_END_ARG_INFO()

static const zend_function_entry aop_methods[] = {
    PHP_ME(AopJoinpoint, getArguments, arginfo_aop_args_returnbyref, 0)
    PHP_ME(AopJoinpoint, getPropertyName, NULL, 0)
    PHP_ME(AopJoinpoint, getPropertyValue, NULL, 0)
    PHP_ME(AopJoinpoint, setArguments, arginfo_aop_args_setArguments, 0)
    PHP_ME(AopJoinpoint, getKindOfAdvice, NULL, 0)
    PHP_ME(AopJoinpoint, getReturnedValue, arginfo_aop_args_returnbyref, 0)
    PHP_ME(AopJoinpoint, getAssignedValue, arginfo_aop_args_returnbyref, 0)
    PHP_ME(AopJoinpoint, setReturnedValue, arginfo_aop_args_setReturnedValue, 0)
    PHP_ME(AopJoinpoint, setAssignedValue, arginfo_aop_args_setAssignedValue, 0)
    PHP_ME(AopJoinpoint, getPointcut, NULL, 0)
    PHP_ME(AopJoinpoint, getObject, NULL, 0)
    PHP_ME(AopJoinpoint, getClassName, NULL, 0)
    PHP_ME(AopJoinpoint, getMethodName, NULL, 0)
    PHP_ME(AopJoinpoint, getFunctionName, NULL, 0)
    PHP_ME(AopJoinpoint, getException, NULL, 0)
    PHP_ME(AopJoinpoint, process, NULL, 0)
    {NULL, NULL, NULL}
};

PHP_RSHUTDOWN_FUNCTION(aop)
{
    int i;
    for (i=0;i<aop_g(object_cache_size);i++) {
        if (aop_g(object_cache)[i]!=NULL) {
            free_object_cache(aop_g(object_cache)[i]);
        }
    }
    efree(aop_g(object_cache));

    zend_hash_destroy(aop_g(function_cache));
    FREE_HASHTABLE(aop_g(function_cache));

    zend_hash_destroy(aop_g(pointcuts));
    FREE_HASHTABLE(aop_g(pointcuts));
    for (i = 0; i < aop_g(count_aopJoinpoint_cache); i++) {
        zval *aop_object = aop_g(aopJoinpoint_cache)[i];
        zval_ptr_dtor(&aop_object);
    }
    if (aop_g(aopJoinpoint_cache)!=NULL) {
        efree(aop_g(aopJoinpoint_cache));
    }
    return SUCCESS;
}
PHP_RINIT_FUNCTION(aop)
{
    aop_g(aopJoinpoint_cache) = NULL;
    aop_g(count_aopJoinpoint_cache) = 0;
    aop_g(overloaded) = 0;
    aop_g(in_ex) = 0;
    aop_g(lock_write_property) = 0;
    aop_g(lock_read_property) = 0;

    aop_g(object_cache_size) = 1024;
    aop_g(object_cache) = ecalloc(1024, sizeof (object_cache *));

    aop_g(pointcut_version) = 0;

    ALLOC_HASHTABLE(aop_g(pointcuts));
    zend_hash_init(aop_g(pointcuts), 16, NULL, free_pointcut,0);


    ALLOC_HASHTABLE(aop_g(function_cache));
    zend_hash_init(aop_g(function_cache), 16, NULL, free_pointcut_cache,0);

    return SUCCESS;
}

static void free_pointcut_cache (void * cache) {
    pointcut_cache *_cache = ((pointcut_cache *)cache);
    if (_cache->ht!=NULL) {
        zend_hash_destroy(_cache->ht);
        FREE_HASHTABLE(_cache->ht);
    }
}

static void free_object_cache (void * cache) {
    object_cache *_cache = ((object_cache *)cache);
    if (_cache->write!=NULL) {
        zend_hash_destroy(_cache->write);
        FREE_HASHTABLE(_cache->write);
    }
    if (_cache->read!=NULL) {
        zend_hash_destroy(_cache->read);
        FREE_HASHTABLE(_cache->read);
    }
    if (_cache->func!=NULL) {
        zend_hash_destroy(_cache->func);
        FREE_HASHTABLE(_cache->func);
    }

    efree(_cache);
}

static void free_pointcut(void *pc)
{
    pointcut *_pc = *((pointcut **)pc);
    if (_pc->class_name!=NULL) {
        efree(_pc->class_name);
    }
    
    if (_pc->method!=NULL) {
        efree(_pc->method);
    }
    if (_pc->selector!=NULL) {
        efree(_pc->selector);
    }
    if (_pc->fci.function_name) {
        zval_ptr_dtor((zval **)&_pc->fci.function_name);
    }
    if (_pc->fci.object_ptr) {
        zval_ptr_dtor((zval **)&_pc->fci.object_ptr);
    }
    /* Seems to be free by the engine (pce cache are in a hashtable)
    if (_pc->re_method!=NULL) {
        pcre_free(_pc->re_method);
    }
    if (_pc->re_class!=NULL) {
        php_printf("FREE PCRE CLASS\n");
        pcre_free(_pc->re_class);
    }
    //*/
    efree(_pc);
}

static zval *get_aopJoinpoint () {
    int i;
    zval *aop_object;
    for (i = 0; i < aop_g(count_aopJoinpoint_cache); i++) {
        zval *aop_object = aop_g(aopJoinpoint_cache)[i];
        if (Z_REFCOUNT_P(aop_object) == 1) {
            AopJoinpoint_object *obj = (AopJoinpoint_object *)((char *)Z_OBJ_P(aop_object) - XtOffsetOf(AopJoinpoint_object, std));
            if (obj->value) {
                zval_ptr_dtor(&obj->value);
            }
            obj->value = NULL;
            obj->member = NULL;
            obj->type = 0;
            obj->object = NULL;
            if (obj->args!=NULL) {
                zval_ptr_dtor(&obj->args);
            }
            obj->args=NULL;
            Z_ADDREF_P(aop_object);
            return aop_object;
        }
    }
    aop_g(count_aopJoinpoint_cache)++;
    if (aop_g(count_aopJoinpoint_cache) == 1) {
        aop_g(aopJoinpoint_cache) = emalloc(sizeof(zval *));
    } else {
        aop_g(aopJoinpoint_cache) = erealloc(aop_g(aopJoinpoint_cache), aop_g(count_aopJoinpoint_cache)*sizeof(zval *));
    }
    object_init_ex(aop_object, aop_class_entry);
    aop_g(aopJoinpoint_cache)[aop_g(count_aopJoinpoint_cache)-1] = aop_object;
    Z_ADDREF_P(aop_object);
    return aop_object;
}

ZEND_DLEXPORT zval *zend_std_get_property_ptr_ptr_overload(zval *object, zval *member) {
    zval *try_return;
    zend_execute_data *ex = EG(current_execute_data);
    //Test if ++
    if (ex->opline->opcode != ZEND_PRE_INC_OBJ && ex->opline->opcode != ZEND_POST_INC_OBJ && ex->opline->opcode != ZEND_PRE_DEC_OBJ && ex->opline->opcode != ZEND_POST_DEC_OBJ) {
        try_return = zend_std_get_property_ptr_ptr(object, member);
    } else {
        // Call original to not have a notice
        zend_std_get_property_ptr_ptr(object, member);
        return NULL;
    }
    return try_return;
}

ZEND_DLEXPORT zval * zend_std_read_property_overload(zval *object, zval *member, int type) {
    zval *to_return;
        if (aop_g(lock_read_property)>25) {
            zend_error(E_ERROR, "Too many level of nested advices. Are there any recursive call ?");
        }
        aop_g(lock_read_property)++;
        to_return = _test_read_pointcut_and_execute(NULL, NULL, object, member, type, EG(scope));
        aop_g(lock_read_property)--;
        return to_return;
}


void _test_func_pointcut_and_execute(zend_hash_position *pos, HashTable *ht, zend_execute_data *ex, zval *object, zend_class_entry *scope, zend_class_entry *called_scope, int args_overloaded, zval *args, zval **to_return_ptr_ptr) {
    zval *aop_object, *exception;
    AopJoinpoint_object *obj;
    pointcut *current_pc;
    pointcut **temp;
    zend_hash_position local_pos;
    if (pos == NULL) {
        pos = &local_pos;
    }
    if (ht==NULL) {
        ht = get_cache_func (object, ex); 
        if (ht==NULL) {
            aop_g(overloaded) = 0;
            execute_context (ex, object, scope, called_scope,args_overloaded, args, to_return_ptr_ptr);
            aop_g(overloaded) = 1;
            return;
        }
        zend_hash_internal_pointer_reset_ex(ht, pos);
    } else {
        zend_hash_move_forward_ex (ht, pos);
    }
    if (zend_hash_get_current_data_ex(ht, (void **)&temp, pos) != SUCCESS) {
        aop_g(overloaded) = 0;
        execute_context (ex, object, scope, called_scope,args_overloaded, args, to_return_ptr_ptr);
        aop_g(overloaded) = 1;
        return;
    }
    current_pc = *temp;

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *)((char *)Z_OBJ_P(aop_object) - XtOffsetOf(AopJoinpoint_object, std));
    obj->current_pointcut = current_pc;
    //obj->current_pointcut_index = current_pointcut_index; 
    obj->pos = pos;
    obj->advice = ht;
    obj->kind_of_advice = current_pc->kind_of_advice;
    obj->object = object;
    obj->to_return_ptr_ptr = to_return_ptr_ptr;
    obj->value = (*to_return_ptr_ptr);
    obj->ex = ex;
    obj->object = object;
    obj->scope = scope;
    obj->called_scope = called_scope;
    if ( args_overloaded) {
        Z_ADDREF_P(args);
        obj->args = args;
    }
    obj->args_overloaded = args_overloaded;
    obj->exception = NULL;
    if (current_pc->kind_of_advice & AOP_KIND_BEFORE) {
        if (!EG(exception)) {
            execute_pointcut(current_pc, aop_object);
        }
    }
    if (current_pc->kind_of_advice & AOP_KIND_AROUND) {
        if (!EG(exception)) {
            execute_pointcut(current_pc, aop_object);
            if (obj->value != NULL) {
                Z_ADDREF_P(obj->value);
                (*to_return_ptr_ptr) = obj->value;
            }
        }
    } else {
        _test_func_pointcut_and_execute(pos, ht, ex, object, scope, called_scope, obj->args_overloaded, obj->args, to_return_ptr_ptr);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AFTER) {
        if (current_pc->kind_of_advice & AOP_KIND_CATCH && EG(exception)) {
            exception = EG(exception); 
            obj->exception = exception;
            EG(exception)=NULL;
            execute_pointcut(current_pc, aop_object);
            EG(exception) = exception;
            if (obj->value != NULL) {
                Z_ADDREF_P(obj->value);
                (*to_return_ptr_ptr) = obj->value;
            }
        } else if (current_pc->kind_of_advice & AOP_KIND_RETURN && !EG(exception)) {
            execute_pointcut(current_pc, aop_object);
            if (obj->value != NULL) {
                (*to_return_ptr_ptr) = obj->value;
            }
        }
    }

    Z_DELREF_P(aop_object);
    return;
}
    
zval *_test_read_pointcut_and_execute(zend_hash_position *pos, HashTable *ht, zval *object, zval *member, int type, zend_class_entry *current_scope) {
    zval *temp_this, *to_return;
    zend_class_entry *scope;
    pointcut **temp;
    pointcut *current_pc;
    AopJoinpoint_object *obj;
    zval *aop_object;
    zend_hash_position local_pos;
    if (pos == NULL) {
        pos = &local_pos;
    }

    if (ht==NULL) {
        ht = get_cache_property (object, member, AOP_KIND_READ);
        zend_hash_internal_pointer_reset_ex(ht, pos);
    } else {
        zend_hash_move_forward_ex (ht, pos);
    }
    if (zend_hash_get_current_data_ex(ht, (void **)&temp, pos) != SUCCESS) {
        scope = EG(scope);
        temp_this = EG(This);
        EG(scope) = current_scope;
        EG(This) = object;
        to_return = zend_std_read_property(object, member, type);
        EG(This) = temp_this;
        EG(scope) = scope;
        return to_return;
    }
    current_pc = *temp;

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *)((char *)Z_OBJ_P(aop_object) - XtOffsetOf(AopJoinpoint_object, std));
    obj->current_pointcut = current_pc;
    obj->pos = pos;
    obj->advice = ht;
    obj->kind_of_advice = (current_pc->kind_of_advice&AOP_KIND_WRITE) ? (current_pc->kind_of_advice - AOP_KIND_WRITE) : current_pc->kind_of_advice;
    obj->object = object;
    obj->member = member;
    obj->type = type;
    obj->scope = current_scope;

    if (current_pc->kind_of_advice & AOP_KIND_BEFORE) {
        execute_pointcut (current_pc, aop_object);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AROUND) {
        execute_pointcut (current_pc, aop_object);
        to_return = obj->value;
    } else {
        to_return = _test_read_pointcut_and_execute(pos, ht, object, member, type, current_scope);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AFTER) {
        execute_pointcut (current_pc, aop_object);
        if (obj->value != NULL) {
            to_return = obj->value;
        }
    }
    Z_DELREF_P(aop_object);
    return to_return;
}

void _test_write_pointcut_and_execute(zend_hash_position *pos, HashTable *ht, zval *object, zval *member, zval *value, zend_class_entry *current_scope) {
    zval *temp_this, *to_return;
    zend_class_entry *scope;
    pointcut **temp;
    pointcut *current_pc;
    AopJoinpoint_object *obj;
    zval *aop_object;
    zend_hash_position local_pos;
    if (pos == NULL) {
        pos = &local_pos;
    }

    if (ht==NULL) {
        ht = get_cache_property (object, member, AOP_KIND_WRITE);
        zend_hash_internal_pointer_reset_ex(ht, pos);
    } else {
        zend_hash_move_forward_ex (ht, pos);
    }
    if (zend_hash_get_current_data_ex(ht, (void **)&temp, pos) != SUCCESS) {
        scope = EG(scope);
        temp_this = EG(This);
        EG(scope) = current_scope;
        EG(This) = object;
        zend_std_write_property(object,member,value);
        EG(This) = temp_this;
        EG(scope) = scope;
        return;
    }
    current_pc = *temp;

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *)((char *)Z_OBJ_P(aop_object) - XtOffsetOf(AopJoinpoint_object, std));
    obj->current_pointcut = current_pc;
    obj->pos = pos;
    obj->advice = ht;
    obj->kind_of_advice = (current_pc->kind_of_advice&AOP_KIND_READ) ? (current_pc->kind_of_advice - AOP_KIND_READ) : current_pc->kind_of_advice;
    obj->object = object;
    obj->member = member;
    obj->value = value;
    Z_SET_ISREF_P(value);
    Z_ADDREF_P(value);
    obj->scope = current_scope;

    if (current_pc->kind_of_advice & AOP_KIND_BEFORE) {
        execute_pointcut (current_pc, aop_object);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AROUND) {
        execute_pointcut (current_pc, aop_object);
    } else {
        value = obj->value;
        _test_write_pointcut_and_execute(pos, ht, object, member, value, current_scope);
    }
    if (current_pc->kind_of_advice & AOP_KIND_AFTER) {
        execute_pointcut (current_pc, aop_object);
        if (obj->value != NULL) {
            to_return = obj->value;
        }
    }
    Z_DELREF_P(aop_object);
}


static int test_property_scope (pointcut *current_pc, zend_class_entry *ce, zval *member) {
    zend_property_info *property_info = NULL;
    zend_string *member_str = zval_get_string(member);
    property_info = zend_hash_find_ptr(&ce->properties_info, member_str);
    zend_string_release(member_str);
    if (property_info != NULL) {
        if (current_pc->static_state != 2) {
            if (current_pc->static_state) {
                if (!(property_info->flags & ZEND_ACC_STATIC)) {
                    return 0;
                }
            } else {
                if ((property_info->flags & ZEND_ACC_STATIC)) {
                    return 0;
                }
            }
        }       
        if (current_pc->scope != 0 && !(current_pc->scope & (property_info->flags & ZEND_ACC_PPP_MASK))) {
            return 0;
        }
    } else {
        if (current_pc->scope != 0 && !(current_pc->scope & ZEND_ACC_PUBLIC)) {
            return 0;
        }
        if (current_pc->static_state == 1) {
            return 0;
        }
    }
    return 1;
}



static void execute_pointcut (pointcut *pointcut_to_execute, zval *arg) {
    zval params[1];
    zval zret;
    zval *zret_ptr = &zret;
    
    ZVAL_COPY(&params[0], arg);
    ZVAL_NULL(&zret);

    pointcut_to_execute->fci.param_count = 1;
    pointcut_to_execute->fci.params = params;
    pointcut_to_execute->fci.retval = &zret;
    if (zend_call_function(&(pointcut_to_execute->fci), &(pointcut_to_execute->fcic)) == FAILURE) {
        zend_error(E_ERROR, "Problem in AOP Callback");
    }
    if (Z_TYPE(zret) != IS_NULL && Z_TYPE(zret) != IS_UNDEF) {
        AopJoinpoint_object *obj = (AopJoinpoint_object *)((char *)Z_OBJ_P(arg) - XtOffsetOf(AopJoinpoint_object, std));
        if (obj->value != NULL) {
            zval_ptr_dtor(&obj->value);
        }
        obj->value = emalloc(sizeof(zval));
        *obj->value = zret;
        ZVAL_UNDEF(&zret);
    } else {
        if (!EG(exception)) {
            zval_ptr_dtor(&zret);
        }
    }
    zval_ptr_dtor(&params[0]);
}

ZEND_DLEXPORT void zend_std_write_property_overload(zval *object, zval *member, zval *value) {
        if (aop_g(lock_write_property) > 25) {
            zend_error(E_ERROR, "Too many level of nested advices. Are there any recursive call ?");
        }
        aop_g(lock_write_property)++;
        _test_write_pointcut_and_execute(NULL, NULL, object, member, value, EG(scope));
        aop_g(lock_write_property)--;
}

PHP_INI_BEGIN()
    STD_PHP_INI_BOOLEAN("aop.enable","1",PHP_INI_ALL, OnUpdateBool, aop_enable, zend_aop_globals, aop_globals)
PHP_INI_END()

PHP_MINIT_FUNCTION(aop)
{
    zend_class_entry ce;
    ZEND_INIT_MODULE_GLOBALS(aop, php_aop_init_globals, NULL);
    REGISTER_INI_ENTRIES();

    INIT_CLASS_ENTRY(ce, "AopJoinpoint", aop_methods);
    aop_class_entry = zend_register_internal_class(&ce);
    aop_class_entry->create_object = aop_create_object;
    memcpy(&AopJoinpoint_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    AopJoinpoint_object_handlers.clone_obj = NULL;

    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE", AOP_KIND_BEFORE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER", AOP_KIND_AFTER, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND", AOP_KIND_AROUND, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_PROPERTY", AOP_KIND_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_FUNCTION", AOP_KIND_FUNCTION, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_METHOD", AOP_KIND_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_READ", AOP_KIND_READ, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_WRITE", AOP_KIND_WRITE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND_WRITE_PROPERTY", AOP_KIND_AROUND_WRITE_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND_READ_PROPERTY", AOP_KIND_AROUND_READ_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE_WRITE_PROPERTY", AOP_KIND_BEFORE_WRITE_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE_READ_PROPERTY", AOP_KIND_BEFORE_READ_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER_WRITE_PROPERTY", AOP_KIND_AFTER_WRITE_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER_READ_PROPERTY", AOP_KIND_AFTER_READ_PROPERTY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE_METHOD", AOP_KIND_BEFORE_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER_METHOD", AOP_KIND_AFTER_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND_METHOD", AOP_KIND_AROUND_METHOD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_BEFORE_FUNCTION", AOP_KIND_BEFORE_FUNCTION, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AFTER_FUNCTION", AOP_KIND_AFTER_FUNCTION, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("AOP_KIND_AROUND_FUNCTION", AOP_KIND_AROUND_FUNCTION, CONST_CS | CONST_PERSISTENT);

#if ZEND_MODULE_API_NO < 20100525
    zend_std_write_property = std_object_handlers.write_property;
#endif
    std_object_handlers.write_property = zend_std_write_property_overload;
    zend_std_read_property = std_object_handlers.read_property;
    std_object_handlers.read_property = zend_std_read_property_overload;

    zend_std_get_property_ptr_ptr = std_object_handlers.get_property_ptr_ptr;
    std_object_handlers.get_property_ptr_ptr = zend_std_get_property_ptr_ptr_overload;

#if ZEND_MODULE_API_NO >= 20121212
    _zend_execute_ex = zend_execute_ex;
    zend_execute_ex  = aop_execute_ex;
    _zend_execute = _zend_execute_overload;
#else
    _zend_execute = zend_execute;
    zend_execute  = aop_execute;
#endif
    _zend_execute_internal = zend_execute_internal;
    zend_execute_internal  = aop_execute_internal;

    return SUCCESS;
}




static pointcut * alloc_pointcut () {
    pointcut *pc = (pointcut *)emalloc(sizeof(pointcut));


    pc->scope = 0;
    pc->static_state = 2;
    pc->method_jok = 0;
    pc->class_jok = 0;
    pc->class_name = NULL;
    pc->method = NULL;
    pc->selector = NULL;
    pc->kind_of_advice = 0;
    //pc->fci = NULL;
    //pc->fcic = NULL;
    pc->re_method = NULL;
    pc->re_class = NULL;
    return pc;
}

static void add_pointcut (zend_fcall_info fci, zend_fcall_info_cache fcic, char *selector, int selector_len, int type , zval **return_value_ptr) {
    pointcut *pc = NULL;
    char *temp_str = NULL;
    int is_class = 0;
	scanner_state *state = (scanner_state *)emalloc(sizeof(scanner_state)); 
    scanner_token *token = (scanner_token *)emalloc(sizeof(scanner_token));
    
    if (selector_len < 2) {
        zend_error(E_ERROR, "The given pointcut is invalid. You must specify a function call, a method call or a property operation"); 
    }

    pc = alloc_pointcut();
    pc->selector = estrdup(selector);
    pc->fci = fci;
    pc->fcic = fcic;
    pc->kind_of_advice = type;

    state->start = selector;
    state->end = state->start;
    while(0 <= scan(state, token)) {
    //    php_printf("TOKEN %d \n", token->TOKEN);
        switch (token->TOKEN) {
            case TOKEN_STATIC:
                pc->static_state=token->int_val;
                break;
            case TOKEN_SCOPE:
                pc->scope |= token->int_val;
                break;
            case TOKEN_CLASS:
                pc->class_name=estrdup(temp_str);
                efree(temp_str);
                temp_str=NULL;
                is_class=1;
                break;
            case TOKEN_PROPERTY:
                pc->kind_of_advice |= AOP_KIND_PROPERTY | token->int_val;
                break;
            case TOKEN_FUNCTION:
                if (is_class) {
                    pc->kind_of_advice |= AOP_KIND_METHOD;
                } else {
                    pc->kind_of_advice |= AOP_KIND_FUNCTION;
                }
                break;
            case TOKEN_TEXT:
                if (temp_str!=NULL) {
                    efree(temp_str);
                }
                temp_str=estrdup(token->str_val);
                efree(token->str_val);
                break;
            default:
                break;
        }
    }
    pc->method = temp_str;
    
    if (pc->kind_of_advice==type) {
        pc->kind_of_advice |= AOP_KIND_READ | AOP_KIND_WRITE | AOP_KIND_PROPERTY;
    }
    make_regexp_on_pointcut(&pc);

    zend_hash_next_index_insert_ptr(aop_g(pointcuts), pc);
    aop_g(pointcut_version)++;
    efree(state);
    efree(token);




}

void make_regexp_on_pointcut (pointcut **pc) { 
    pcre_extra *pcre_extra = NULL;
    int preg_options = 0;
    int *replace_count, *new_length;
    char *regexp;
    char *regexp_buffer;
    char tempregexp[500];

    (*pc)->method_jok = (strchr((*pc)->method, '*') != NULL);
    replace_count = emalloc (sizeof(int));
    new_length = emalloc (sizeof(int));
    regexp = estrdup((*pc)->method);
    regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "**\\", 3, "[.#}", 4, new_length, 0, replace_count);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "**", 2, "[.#]", 4, new_length, 0, replace_count);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "\\", 1, "\\\\", 2, new_length, 0, replace_count);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "*", 1, "[^\\\\]*", 6, new_length, 0, replace_count);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "[.#]", 4, ".*", 2, new_length, 0, replace_count);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "[.#}", 4, "(.*\\\\)?", 7, new_length, 0, replace_count);
    efree(regexp);
    regexp = regexp_buffer;
    if (regexp[0]!='\\') {
        sprintf((char *)tempregexp, "/^%s$/i", regexp);
    } else {
        sprintf((char *)tempregexp, "/^%s$/i", regexp+2);
    }
    efree(regexp);
    (*pc)->re_method = pcre_get_compiled_regex(tempregexp, &pcre_extra, &preg_options);
    //efree(tempregexp);
    if (!(*pc)->re_method) {
        php_error_docref(NULL, E_WARNING, "Invalid expression");
    }
    if ((*pc)->class_name != NULL) {
        regexp = estrdup((*pc)->class_name);
        regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "**\\", 3, "[.#}", 4, new_length, 0, replace_count);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "**", 2, "[.#]", 4, new_length, 0, replace_count);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "\\", 1, "\\\\", 2, new_length, 0, replace_count);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "*", 1, "[^\\\\]*", 6, new_length, 0, replace_count);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "[.#]", 4, ".*", 2, new_length, 0, replace_count);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = php_str_to_str_ex(regexp, strlen(regexp), "[.#}", 4, "(.*\\\\)?", 7, new_length, 0, replace_count);
        efree(regexp);
        regexp = regexp_buffer;
        if (regexp[0]!='\\') {
            sprintf((char *)tempregexp, "/^%s$/i", regexp);
        } else {
            sprintf((char *)tempregexp, "/^%s$/i", regexp+2);
        }
        efree(regexp);
        (*pc)->re_class = pcre_get_compiled_regex(tempregexp, &pcre_extra, &preg_options);
        if (!(*pc)->re_class) {
            php_error_docref(NULL, E_WARNING, "Invalid expression");
        }
    }
    efree(replace_count);
    efree(new_length);
}

PHP_FUNCTION(aop_add_around)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_around() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AROUND, return_value_ptr);
}

PHP_FUNCTION(aop_add_before)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic = { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_before() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_BEFORE, return_value_ptr);
}

PHP_FUNCTION(aop_add_after_throwing)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_after() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }

    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_CATCH, return_value_ptr);

}

PHP_FUNCTION(aop_add_after_returning)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_after() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }

    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_RETURN, return_value_ptr);

}

PHP_FUNCTION(aop_add_after)
{
    zend_fcall_info fci;
    zend_fcall_info_cache fcic= { 0, NULL, NULL, NULL, NULL };
    char *selector;
    int selector_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sf", &selector, &selector_len, &fci, &fcic) == FAILURE) {
        zend_error(E_ERROR, "aop_add_after() expects a string for the pointcut as a first argument and a callback as a second argument");
        return;
    }
    if (fci.function_name) {
        Z_ADDREF_P(fci.function_name);
    }
    if (fci.object_ptr) {
        Z_ADDREF_P(fci.object_ptr);
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_CATCH|AOP_KIND_RETURN, return_value_ptr);
}


ZEND_DLEXPORT void aop_execute_ex (zend_execute_data *execute_data) {
    zend_op_array *op_array = execute_data->func->op_array;
    if (aop_g(in_ex)) {
        aop_g(in_ex) = 0;
        _zend_execute_ex(execute_data);
    } else {
        zend_vm_stack_free();
        if (EG(This)) {
            //zval_ptr_dtor(&EG(This));
        }
        EG(current_execute_data) = execute_data->prev_execute_data;
        aop_execute(op_array);
    }
}

ZEND_DLEXPORT void _zend_execute_overload (zend_op_array *ops) {
    aop_g(in_ex) = 1;
    zend_execute(ops);
}

ZEND_DLEXPORT void aop_execute (zend_op_array *ops) {
    zend_execute_data *data;
    zend_function *curr_func = NULL;
    int must_return = (EG(return_value_ptr_ptr)!=NULL);

    if (!aop_g(aop_enable)) {
        _zend_execute(ops);
        return;
    }

    data = EG(current_execute_data);

    if (data) {
        curr_func = data->func;
    }
    if (ops->type==ZEND_EVAL_CODE || curr_func == NULL || curr_func->common.function_name == NULL || aop_g(overloaded) || EG(exception)) {
        _zend_execute(ops);
        return;
    }
    if (!EG(return_value_ptr_ptr)) {
        EG(return_value_ptr_ptr) = emalloc(sizeof(zval *));
        *(EG(return_value_ptr_ptr)) = NULL;
    }
    aop_g(overloaded) = 1;
    _test_func_pointcut_and_execute(NULL, NULL, EG(current_execute_data), EG(This), EG(scope),EG(called_scope), 0, NULL, EG(return_value_ptr_ptr));
    aop_g(overloaded) = 0;
    if (!must_return) {
        if (*EG(return_value_ptr_ptr)) {
            zval_ptr_dtor(EG(return_value_ptr_ptr));
            efree(EG(return_value_ptr_ptr));
        } else {
            efree(EG(return_value_ptr_ptr));
        }
    } else if (!must_return) {
        efree(EG(return_value_ptr_ptr));
    } else {
        if (!*EG(return_value_ptr_ptr)) {
            *EG(return_value_ptr_ptr) = emalloc(sizeof(zval));
            ZVAL_NULL(*EG(return_value_ptr_ptr));
        }
    }
}

ZEND_DLEXPORT void aop_execute_internal (zend_execute_data *current_execute_data, zend_fcall_info *fci, zval *return_value) {
        zend_execute_data *data;
        zend_function *curr_func = NULL;
        zval *to_return_ptr = return_value;

        if (!aop_g(aop_enable)) {
            if (_zend_execute_internal) {
                _zend_execute_internal(current_execute_data, fci, return_value);
            } else {
                zend_execute_internal(current_execute_data, fci, return_value);
            }
            return;
        }

        data = EG(current_execute_data);

        if (data) {
            curr_func = data->func;
        }
        if (curr_func == NULL || curr_func->common.function_name == NULL || aop_g(overloaded) || EG(exception)) {
            if (_zend_execute_internal) {
                _zend_execute_internal(current_execute_data, fci, return_value);
            } else {
                zend_execute_internal(current_execute_data, fci, return_value);
            }
            return;
        }   

        aop_g(overloaded) = 1;
        _test_func_pointcut_and_execute(NULL,NULL, EG(current_execute_data), current_execute_data->This, EG(scope), EG(called_scope), 0, NULL, &to_return_ptr);
        aop_g(overloaded) = 0;
    }

    static void execute_context (zend_execute_data *ex, zval *object, zend_class_entry *calling_scope, zend_class_entry *called_scope, int args_overloaded, zval *args, zval **to_return_ptr_ptr) {
        zval **return_value_ptr;
        zval ***params;
        zend_uint i;
        zval **original_return_value;
        HashTable *calling_symbol_table;
        zend_op_array *original_op_array;
        zend_op **original_opline_ptr;
        zend_class_entry *current_scope;
        zend_class_entry *current_called_scope;
        //    zend_class_entry *calling_scope = NULL;
        zval *current_this;
        zend_execute_data *original_execute_data;
        zend_execute_data execute_data;
        zval *original_object;
        zend_hash_position pos;
        zval ** temp = NULL;
        int arg_count = 0;

        if (!EG(active)) {
            //TODO ERROR
            return ;
        }

        if (EG(exception)) {
            //TODO ERROR
            return ;
        }
        execute_data = *ex;

        //EX(function_state).function = fci_cache->function_handler;
        original_object = EX(object);
        EX(object) = object;
        if (object && Z_TYPE_P(object) == IS_OBJECT && Z_OBJ_P(object) == NULL) {
            //TODO ERROR
            php_printf("ERRRORR");
            return ;
        }
        original_execute_data = EG(current_execute_data);
        EG(current_execute_data) = ex;

        if (args_overloaded) {
            if (args && Z_TYPE_P(args) == IS_ARRAY) {
                args_overloaded = 1;
                arg_count=0;
                zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(args), &pos);
                while (zend_hash_get_current_data_ex(Z_ARRVAL_P(args), (void **)&temp, &pos) == SUCCESS) {
                    arg_count++;
                    if (arg_count == 1) {
                        params = emalloc(sizeof(zval **));
                    } else {
                        params = erealloc(params, arg_count*sizeof(zval **));
                    }
                    params[arg_count-1] = temp;
                    zend_hash_move_forward_ex(Z_ARRVAL_P(args), &pos);
                }
            }

            if (arg_count > 0) {
                //Copy from zend_call_function
                ZEND_VM_STACK_GROW_IF_NEEDED((int) arg_count + 1);
                for (i=0; i < arg_count; i++) {
                    zval *param;
                    if (ARG_SHOULD_BE_SENT_BY_REF(ex->func, i + 1)) {
                        if (!Z_ISREF_P(*params[i]) && Z_REFCOUNT_P(*params[i]) > 1) {
                            zval *new_zval;

                            if (!ARG_MAY_BE_SENT_BY_REF(ex->func, i + 1)) {
                                if (i || UNEXPECTED(ZEND_VM_STACK_ELEMETS(EG(argument_stack)) == (EG(argument_stack)->top))) {
                                    zend_vm_stack_push((void *) (zend_uintptr_t)i);
                                    zend_vm_stack_clear_multiple(0);
                                }

                                zend_error(E_WARNING, "Parameter %d to %s%s%s() expected to be a reference, value given",
                                        i+1,
                                        ex->func->common.scope ? ZSTR_VAL(ex->func->common.scope->name) : "",
                                        ex->func->common.scope ? "::" : "",
                                        ZSTR_VAL(ex->func->common.function_name)
                                        );
                                return;
                            }

                            new_zval = emalloc(sizeof(zval));
                            *new_zval = **params[i];
                            zval_copy_ctor(new_zval);
                            Z_REFCOUNT_P(new_zval) = 1;
                            Z_DELREF_PP(params[i]);
                            *params[i] = new_zval;
                        }
                        Z_ADDREF_P(*params[i]);
                        Z_SET_ISREF_P(*params[i]);
                        param = *params[i];
                    } else if (Z_ISREF_P(*params[i]) && (ex->func->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) == 0 ) {
                        param = emalloc(sizeof(zval));
                        *param = **(params[i]);
                        ZVAL_UNDEF(param);
                        zval_copy_ctor(param);
                    } else if (*params[i] != &EG(uninitialized_zval)) {
                        Z_ADDREF_P(*params[i]);
                        param = *params[i];
                    } else {
                        param = emalloc(sizeof(zval));
                        *param = **(params[i]);
                        ZVAL_UNDEF(param);
                    }
                    zend_vm_stack_push(param);
                }
                EG(current_execute_data)->func->common.num_args = arg_count;
            }
        } else {
            arg_count = ZEND_CALL_NUM_ARGS(ex);
        }

        current_scope = EG(scope);
        EG(scope) = calling_scope;
        current_this = EG(This);
        current_called_scope = EG(called_scope);
        if (called_scope) {
            EG(called_scope) = called_scope;
        } else if (ex->func->type != ZEND_INTERNAL_FUNCTION) {
            EG(called_scope) = NULL;
        }

        if (object) {
            if ((ex->func->common.fn_flags & ZEND_ACC_STATIC)) {
                EG(This) = NULL;
            } else {
                EG(This) = object;

                if (!Z_ISREF_P(EG(This))) {
                    Z_ADDREF_P(EG(This)); 
                } else {
                    zval *this_ptr;
                    this_ptr = emalloc(sizeof(zval));
                    *this_ptr = *EG(This);
                    ZVAL_UNDEF(this_ptr);
                    zval_copy_ctor(this_ptr);
                    EG(This) = this_ptr;
                }
            }
        } else {
            EG(This) = NULL;
        }

        //    EX(prev_execute_data) = EG(current_execute_data);
        if (ex->func->type == ZEND_USER_FUNCTION) {
            calling_symbol_table = EG(active_symbol_table);
            EG(scope) = ex->func->common.scope;

            original_return_value = EG(return_value_ptr_ptr);
            original_op_array = EG(active_op_array);
            EG(return_value_ptr_ptr) = to_return_ptr_ptr;
            EG(active_op_array) = &ex->func->op_array;
            original_opline_ptr = EG(opline_ptr);
            _zend_execute(EG(active_op_array));

            if (EG(symtable_cache_ptr)>=EG(symtable_cache_limit)) {
                zend_hash_destroy(EG(active_symbol_table));
                FREE_HASHTABLE(EG(active_symbol_table));
            } else {
                /* clean before putting into the cache, since clean
                   could call dtors, which could use cached hash */
                if (EG(active_symbol_table)) {
                    zend_hash_clean(EG(active_symbol_table));
                    *(++EG(symtable_cache_ptr)) = EG(active_symbol_table);
                }
            }
            EG(active_op_array) = original_op_array;
            EG(return_value_ptr_ptr)=original_return_value;
            EG(opline_ptr) = original_opline_ptr;
            EG(active_symbol_table) = calling_symbol_table;
        } else if (ex->func->type == ZEND_INTERNAL_FUNCTION) {
            int call_via_handler = (ex->func->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) != 0;
            if (to_return_ptr_ptr==NULL) {
                to_return_ptr_ptr = emalloc(sizeof(zval *));
            }
            if ((*to_return_ptr_ptr)==NULL) {
                *to_return_ptr_ptr = emalloc(sizeof(zval));
                ZVAL_NULL(*to_return_ptr_ptr);
            }
            if (ex->func->common.scope) {
                EG(scope) = ex->func->common.scope;
            }
            ((zend_internal_function *) ex->func)->handler(arg_count, *to_return_ptr_ptr, to_return_ptr_ptr, object, 1);
            /*  We shouldn't fix bad extensions here,
                because it can break proper ones (Bug #34045)
                if (!EX(function_state).function->common.return_reference)
                {
                ZVAL_UNDEF(*fci->retval_ptr_ptr);
                }*/

        } else { /* ZEND_OVERLOADED_FUNCTION */
            if ((*to_return_ptr_ptr)==NULL) {
                *to_return_ptr_ptr = emalloc(sizeof(zval));
                ZVAL_NULL(*to_return_ptr_ptr);
            }
            if (object) {
                Z_OBJ_HT_P(object)->call_method(Z_OBJCE_P(object), ex->func->common.function_name, arg_count, *to_return_ptr_ptr, to_return_ptr_ptr, object, 1);
            } else {
                zend_error(E_ERROR, "Cannot call overloaded function for non-object");
            }

            if (ex->func->type == ZEND_OVERLOADED_FUNCTION_TEMPORARY) {
                efree((char*)ex->func->common.function_name);
            }
            efree(ex->func);

        }


        EG(current_execute_data) =  original_execute_data;
        if (args_overloaded) {
            zend_vm_stack_clear_multiple(0);
        }

        if (EG(This)) {
            zval_ptr_dtor(&EG(This));
        }
        EG(called_scope) = current_called_scope;
        EG(scope) = current_scope;
        EG(This) = current_this;
        //    EG(current_execute_data) = EX(prev_execute_data);
        EX(object) = original_object;
        if (args_overloaded) {
            //zval_ptr_dtor(&args);
            efree(params);
        }
    }

    static int strcmp_with_joker_case(char *str_with_jok, char *str, int case_sensitive) {
        int joker = 0;
        if (str_with_jok[0] == '*') {
            if (str_with_jok[1] == '\0') {
                return 1;
            }
        }
        if (str_with_jok[0] == '*') {
            if (case_sensitive) {
                return !strcmp(str_with_jok+1, str+(strlen(str)-(strlen(str_with_jok)-1)));
            } else {
                return !strcasecmp(str_with_jok+1, str+(strlen(str)-(strlen(str_with_jok)-1)));
            }
        }
        if (str_with_jok[strlen(str_with_jok)-1] == '*') {
            if (case_sensitive) {
                return !strncmp(str_with_jok, str, strlen(str_with_jok)-1);
            } else {
                return !strncasecmp(str_with_jok, str, strlen(str_with_jok)-1);
            }
        }
        if (case_sensitive) {
            return !strcmp(str_with_jok, str);
        } else {
            return !strcasecmp(str_with_jok, str);
        }
    }

    static int strcmp_with_joker(char *str_with_jok, char *str) {
        return strcmp_with_joker_case (str_with_jok, str, 0);
    }

    static int pointcut_match_zend_class_entry (pointcut *pc, zend_class_entry *ce) {
        int i, matches;

        matches = pcre_exec(pc->re_class, NULL, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name), 0, 0, NULL, 0);
        if (matches >= 0) {
            return 1;
        }
        for (i = 0; i < (int) ce->num_interfaces; i++) {
            matches = pcre_exec(pc->re_class, NULL, ZSTR_VAL(ce->interfaces[i]->name), ZSTR_LEN(ce->interfaces[i]->name), 0, 0, NULL, 0);
            if (matches >= 0) {
                return 1;
            }
        }
#if ZEND_MODULE_API_NO >= 20100525
        for (i = 0; i < (int) ce->num_traits; i++) {
            matches = pcre_exec(pc->re_class, NULL, ZSTR_VAL(ce->traits[i]->name), ZSTR_LEN(ce->traits[i]->name), 0, 0, NULL, 0);
            if (matches>=0) {
                return 1;
            }
        }
#endif
        ce = ce->parent;
        while (ce != NULL) {
            matches = pcre_exec(pc->re_class, NULL, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name), 0, 0, NULL, 0);
            if (matches >= 0) {
                return 1;
            }
            ce = ce->parent;
        }
        return 0;
    }

    static int pointcut_match_zend_function (pointcut *pc, zend_function *curr_func, zend_execute_data *data) {
        int comp_start = 0;
        if (pc->static_state != 2) {
            if (pc->static_state) {
                if (!(curr_func->common.fn_flags & ZEND_ACC_STATIC)) {
                    return 0;
                }
            } else {
                if ((curr_func->common.fn_flags & ZEND_ACC_STATIC)) {
                    return 0;
                }
            }
        }
        if (pc->scope != 0 && !(pc->scope & (curr_func->common.fn_flags & ZEND_ACC_PPP_MASK))) {
            return 0;
        }
        if (pc->class_name == NULL && pc->method[0] == '*' && pc->method[1]=='\0') {
            return 1;
        }
        if (pc->class_name == NULL && curr_func->common.scope != NULL) {    
            return 0;
        }
        if (pc->method_jok) {
            int matches = pcre_exec(pc->re_method, NULL, ZSTR_VAL(curr_func->common.function_name), ZSTR_LEN(curr_func->common.function_name), 0, 0, NULL, 0);

            if (matches < 0) {
                return 0;
            }
        } else {
            if (pc->method[0]=='\\') {
                comp_start=1;
            }
            if (strcasecmp(pc->method+comp_start, ZSTR_VAL(curr_func->common.function_name))) {
                return 0;
            }
        }
        return 1;
    }

    zval *get_current_args (zend_execute_data *ex) {
        int arg_count;
        int i;
        zval *return_value;
        zval *element;

        if (!ex || !ex->func) {
            zend_error(E_WARNING, "Problem in AOP getArgs");
            return NULL;
        }

        arg_count = ZEND_CALL_NUM_ARGS(ex);

        return_value = emalloc(sizeof(zval));
        ZVAL_ARR(return_value, zend_new_array(arg_count));

        for (i=0; i < arg_count; i++) {
            element = ZEND_CALL_ARG(ex, i);
            zend_hash_next_index_insert(Z_ARRVAL_P(return_value), element);
        }
        return return_value;
    }

    PHP_MSHUTDOWN_FUNCTION(aop)
    {
#if ZEND_MODULE_API_NO >= 20121212
        zend_execute_ex  = _zend_execute;
#else
        zend_execute  = _zend_execute;
#endif
        zend_execute_internal  = _zend_execute_internal;
        
        UNREGISTER_INI_ENTRIES();
        return SUCCESS;
    }


    HashTable *calculate_class_pointcuts (zend_class_entry *ce, int kind_of_advice) {
        pointcut **pc;
        zend_hash_position pos;
        HashTable *ht;
        ALLOC_HASHTABLE(ht);
        //No free because pointcuts are free in the aop_g(pointcuts)
        zend_hash_init(ht, 16, NULL, NULL ,0);


        zend_hash_internal_pointer_reset_ex(aop_g(pointcuts), &pos);
        while (zend_hash_get_current_data_ex(aop_g(pointcuts), (void **)&pc, &pos) == SUCCESS) {
            if (!((*pc)->kind_of_advice & kind_of_advice)) {
                zend_hash_move_forward_ex (aop_g(pointcuts), &pos);
            } else if ((ce==NULL 
                        && ((*pc)->kind_of_advice & AOP_KIND_FUNCTION)) 
                    || (ce!=NULL && pointcut_match_zend_class_entry (*pc, ce))) {
                zend_hash_next_index_insert_ptr(ht, pc);
                zend_hash_move_forward_ex (aop_g(pointcuts), &pos);
            } else {
                zend_hash_move_forward_ex (aop_g(pointcuts), &pos);
            }
        }
        return ht;

    }

    HashTable *calculate_function_pointcuts (zval *object, zend_execute_data *ex) {
        zend_function *curr_func;
        HashTable *ht = NULL;
        HashTable *class_pointcuts;
        zend_hash_position pos;
        pointcut **pc;
        zend_class_entry *ce = NULL;
        if (object != NULL) {
            ce = Z_OBJCE_P(object);
        }
        if (ex) {
            curr_func = ex->func;
        }
        if (ce==NULL && curr_func->common.fn_flags & ZEND_ACC_STATIC) {
            ce = curr_func->common.scope;
        }
        ALLOC_HASHTABLE(ht);
        //No free because pointcuts are free in the aop_g(pointcuts)
        zend_hash_init(ht, 16, NULL, NULL ,0);

        class_pointcuts = calculate_class_pointcuts(ce, AOP_KIND_FUNCTION | AOP_KIND_METHOD);

        zend_hash_internal_pointer_reset_ex(class_pointcuts, &pos);
        while (zend_hash_get_current_data_ex(class_pointcuts, (void **)&pc, &pos) == SUCCESS) {
            if (pointcut_match_zend_function((*pc), curr_func, ex)) {
                zend_hash_next_index_insert (ht, pc, sizeof(pointcut **), NULL);
            }
            zend_hash_move_forward_ex (class_pointcuts, &pos);
        }

        zend_hash_destroy(class_pointcuts);
        FREE_HASHTABLE(class_pointcuts);

        return ht;
    }
    HashTable *calculate_property_pointcuts (zval *object, zval *member, int kind AOP_KEY_D) {

        zval *tmp_member;
        HashTable *class_pointcuts;
        HashTable *ht;
        zend_hash_position pos;
        pointcut **pc;
        ALLOC_HASHTABLE(ht);
        //No free because pointcuts are free in the aop_g(pointcuts)
        zend_hash_init(ht, 16, NULL, NULL ,0);

        class_pointcuts = calculate_class_pointcuts(Z_OBJCE_P(object), kind);

    if (Z_TYPE_P(member) != IS_STRING ) {
        tmp_member = emalloc(sizeof(zval));
        *tmp_member = *member;
        ZVAL_UNDEF(tmp_member);
        zval_copy_ctor(tmp_member);
        convert_to_string(tmp_member);
        member = tmp_member;
#if ZEND_MODULE_API_NO >= 20100525
        key = NULL;
#endif
    }


        zend_hash_internal_pointer_reset_ex(class_pointcuts, &pos);
        while (zend_hash_get_current_data_ex(class_pointcuts, (void **)&pc, &pos) == SUCCESS) {
            if ((*pc)->method[0] != '*') {
                if (!strcmp_with_joker_case((*pc)->method, Z_STRVAL_P(member), 1)) {
                    zend_hash_move_forward_ex (class_pointcuts, &pos);
                    continue;
                }
            }
            //Scope
            if ((*pc)->static_state != 2 || (*pc)->scope != 0) {
                if (!test_property_scope(*pc, Z_OBJCE_P(object), member AOP_KEY_C)) {
                    zend_hash_move_forward_ex (aop_g(pointcuts), &pos);
                    continue;
                }
            }
            zend_hash_next_index_insert_ptr(ht, pc);
            zend_hash_move_forward_ex (aop_g(pointcuts), &pos);
        }
        zend_hash_destroy(class_pointcuts);
        FREE_HASHTABLE(class_pointcuts);
        return ht;

    }

//Cache on object (zval)
//pointcut's HashTable
HashTable *get_object_cache_write (zval *object) //aop_g(object_cache_write)
{
    object_cache *cache;
    cache = get_object_cache(object);
    if (cache->write == NULL) {
        ALLOC_HASHTABLE(cache->write);
        zend_hash_init(cache->write, 16, NULL, free_pointcut_cache ,0);
    }
    return cache->write;;

}
HashTable *get_object_cache_read (zval *object) //aop_g(object_cache_read)
{
    object_cache *cache;
    cache = get_object_cache(object);
    if (cache->read == NULL) {
        ALLOC_HASHTABLE(cache->read);
        zend_hash_init(cache->read, 16, NULL, free_pointcut_cache,0);
    }
    return cache->read;;

}
HashTable *get_object_cache_func (zval *object)
{
    object_cache *cache;
    cache = get_object_cache(object);
    if (cache->func == NULL) {
        ALLOC_HASHTABLE(cache->func);
        zend_hash_init(cache->func, 16, NULL, free_pointcut_cache ,0);
    }
    return cache->func;;

}

object_cache *get_object_cache (zval *object)
{
    int i;
    zend_object_handle handle;
    handle = Z_OBJ_HANDLE_P(object);
    if (handle>=aop_g(object_cache_size)) {
        aop_g(object_cache) = erealloc(aop_g(object_cache), sizeof (object_cache)*handle+1);
        for (i = aop_g(object_cache_size); i <= handle; i++) {
            aop_g(object_cache)[i] = NULL;
        }
        aop_g(object_cache_size) = handle+1;
    }
    if (aop_g(object_cache)[handle]==NULL) {
        aop_g(object_cache)[handle] = emalloc(sizeof(object_cache));
        aop_g(object_cache)[handle]->write = NULL;
        aop_g(object_cache)[handle]->read = NULL;
        aop_g(object_cache)[handle]->func = NULL;
    }
    return aop_g(object_cache)[handle];
}

HashTable * get_cache_property (zval *object, zval *member, int type) {
    HashTable *ht_object_cache;
    pointcut_cache *cache = NULL;
    pointcut_cache *_cache = NULL;

	char *key_str;
    int key_len;
    zval *tmp_member = NULL;
    int member_need_free = 0;
    ulong h;

    if (type & AOP_KIND_READ) {
        ht_object_cache = get_object_cache_read(object);
    } else {
        ht_object_cache = get_object_cache_write(object);
    }
    
    if (Z_TYPE_P(member) != IS_STRING ) {
        tmp_member = emalloc(sizeof(zval));
        *tmp_member = *member;
        ZVAL_UNDEF(tmp_member);
        zval_copy_ctor(tmp_member);
        convert_to_string(tmp_member);
        member = tmp_member;
#if ZEND_MODULE_API_NO >= 20100525
        key = NULL;
#endif
        member_need_free = 1;
    }
    
    key_str = Z_STRVAL_P(member);
    key_len = Z_STRLEN_P(member);
#if ZEND_MODULE_API_NO < 20100525
    h = zend_get_hash_value(key_str, key_len);
#else
    h = key ? key->hash_value : zend_get_hash_value(key_str, key_len);
#endif
    cache = zend_hash_str_find_ptr(ht_object_cache, key_str, key_len);
    if (cache!=NULL 
    && (cache->version < aop_g(pointcut_version) 
        || cache->ce != Z_OBJCE_P(object))) {
        zend_hash_str_del(ht_object_cache, key_str, key_len);
        cache = NULL;
    }
    if (cache == NULL) {
        cache = emalloc(sizeof(pointcut_cache));
        cache->ht = calculate_property_pointcuts (object, member, type);
        cache->version = aop_g(pointcut_version);
        cache->ce = Z_OBJCE_P(object);
        _cache = zend_hash_str_add_ptr(ht_object_cache, key_str, key_len, cache);
        if (_cache != cache) {
            efree(cache);
        }
        cache = _cache;
    }
    if (member_need_free) {
        zval_ptr_dtor(&member);
    }
    return cache->ht;
}

HashTable * get_cache_func (zval *object, zend_execute_data *ex) {
    HashTable *ht_object_cache;
    zend_function *curr_func;
    pointcut_cache *cache = NULL;
    pointcut_cache *_cache = NULL;
    char *key_str;
    int key_len;
    if (ex) {
        curr_func = ex->func;
    }
    if (object == NULL) {
        ht_object_cache = aop_g(function_cache);
        if (curr_func->common.fn_flags & ZEND_ACC_STATIC) {
            key_str = (char *)emalloc (ZSTR_LEN(curr_func->common.scope->name) + ZSTR_LEN(curr_func->common.function_name) + 3);
            sprintf((char *)key_str, "%s::%s", ZSTR_VAL(curr_func->common.scope->name), ZSTR_VAL(curr_func->common.function_name));
            key_len = strlen (key_str);
        } else {
            key_str = estrdup(ZSTR_VAL(curr_func->common.function_name));
            key_len = strlen(key_str);
        }
        } else {
            key_str = estrdup(ZSTR_VAL(curr_func->common.function_name));
            key_len = strlen(key_str);
            ht_object_cache = get_object_cache_func(object);
        }
    cache = zend_hash_str_find_ptr(ht_object_cache, key_str, key_len);
    if (cache!=NULL 
            && (cache->version < aop_g(pointcut_version) 
                || (object!=NULL && cache->ce != Z_OBJCE_P(object)))) {
        zend_hash_str_del(ht_object_cache, key_str, key_len);
        //free_pointcut_cache((void *)cache);
//        efree(cache);
        cache = NULL;
    }
    if (cache == NULL) {
		cache = (pointcut_cache *)emalloc(sizeof(pointcut_cache));
        cache->ht = calculate_function_pointcuts (object, ex);
        cache->version = aop_g(pointcut_version);
        if (object==NULL) {
            cache->ce = NULL;
        } else {
            cache->ce = Z_OBJCE_P(object);
        }

        _cache = zend_hash_str_add_ptr(ht_object_cache, key_str, key_len, cache);
        if (_cache != cache) {
            efree(cache);
        }
        cache = _cache;
    }
    efree(key_str);
    return cache->ht;
}
