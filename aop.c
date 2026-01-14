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
#include <pcre2.h>
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

/* Define _zend_execute_internal here - declared as extern in aop.h */
/* _zend_execute_internal is declared as extern in aop.h, defined here */
void (*_zend_execute_internal) (zend_execute_data *current_execute_data, zval *return_value) = NULL;

static void aop_free_object(zend_object *object)
{
    AopJoinpoint_object *obj = (AopJoinpoint_object *)((char *)object - XtOffsetOf(AopJoinpoint_object, std));
    if (obj->value != NULL) {
        zval_ptr_dtor(obj->value);
        efree(obj->value);
    }
    if (obj->args != NULL) {
        zval_ptr_dtor(obj->args);
        efree(obj->args);
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getPropertyName, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getPropertyValue, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getKindOfAdvice, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getPointcut, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getObject, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getClassName, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getMethodName, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getFunctionName, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_getException, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_aop_process, 0, 0, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry aop_methods[] = {
    PHP_ME(AopJoinpoint, getArguments, arginfo_aop_args_returnbyref, 0)
    PHP_ME(AopJoinpoint, getPropertyName, arginfo_aop_getPropertyName, 0)
    PHP_ME(AopJoinpoint, getPropertyValue, arginfo_aop_getPropertyValue, 0)
    PHP_ME(AopJoinpoint, setArguments, arginfo_aop_args_setArguments, 0)
    PHP_ME(AopJoinpoint, getKindOfAdvice, arginfo_aop_getKindOfAdvice, 0)
    PHP_ME(AopJoinpoint, getReturnedValue, arginfo_aop_args_returnbyref, 0)
    PHP_ME(AopJoinpoint, getAssignedValue, arginfo_aop_args_returnbyref, 0)
    PHP_ME(AopJoinpoint, setReturnedValue, arginfo_aop_args_setReturnedValue, 0)
    PHP_ME(AopJoinpoint, setAssignedValue, arginfo_aop_args_setAssignedValue, 0)
    PHP_ME(AopJoinpoint, getPointcut, arginfo_aop_getPointcut, 0)
    PHP_ME(AopJoinpoint, getObject, arginfo_aop_getObject, 0)
    PHP_ME(AopJoinpoint, getClassName, arginfo_aop_getClassName, 0)
    PHP_ME(AopJoinpoint, getMethodName, arginfo_aop_getMethodName, 0)
    PHP_ME(AopJoinpoint, getFunctionName, arginfo_aop_getFunctionName, 0)
    PHP_ME(AopJoinpoint, getException, arginfo_aop_getException, 0)
    PHP_ME(AopJoinpoint, process, arginfo_aop_process, 0)
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
        zval_ptr_dtor(aop_object);
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
    zend_hash_init(aop_g(pointcuts), 16, NULL, (dtor_func_t)free_pointcut, 0);


    ALLOC_HASHTABLE(aop_g(function_cache));
    zend_hash_init(aop_g(function_cache), 16, NULL, (dtor_func_t)free_pointcut_cache, 0);

    return SUCCESS;
}

static void free_pointcut_cache (zval *zv) {
    pointcut_cache *_cache = (pointcut_cache *)Z_PTR_P(zv);
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

static void free_pointcut(zval *zv)
{
    pointcut **_pc_ptr = (pointcut **)Z_PTR_P(zv);
    pointcut *_pc = *_pc_ptr;
    if (_pc->class_name!=NULL) {
        efree(_pc->class_name);
    }
    
    if (_pc->method!=NULL) {
        efree(_pc->method);
    }
    if (_pc->selector!=NULL) {
        efree(_pc->selector);
    }
    if (Z_TYPE(_pc->fci.function_name) != IS_UNDEF) {
        zval_ptr_dtor(&_pc->fci.function_name); /* function_name is zval, &function_name is zval* - correct */
    }
    if (_pc->fci.object != NULL) {
        /* object is zend_object*, not zval - refcounting handled by zend_call_function */
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
                zval_ptr_dtor(obj->value);
                efree(obj->value);
            }
            obj->value = NULL;
            obj->member = NULL;
            obj->type = 0;
            obj->object = NULL;
            if (obj->args!=NULL) {
                zval_ptr_dtor(obj->args);
                efree(obj->args);
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

ZEND_DLEXPORT zval *zend_std_get_property_ptr_ptr_overload(zend_object *object, zend_string *member, int type, void **cache_slot) {
    zval *try_return;
    zend_execute_data *ex = EG(current_execute_data);
    //Test if ++
    if (ex && ex->opline && ex->opline->opcode != ZEND_PRE_INC_OBJ && ex->opline->opcode != ZEND_POST_INC_OBJ && ex->opline->opcode != ZEND_PRE_DEC_OBJ && ex->opline->opcode != ZEND_POST_DEC_OBJ) {
        try_return = zend_std_get_property_ptr_ptr(object, member, type, cache_slot);
    } else {
        // Call original to not have a notice
        zend_std_get_property_ptr_ptr(object, member, type, cache_slot);
        return NULL;
    }
    return try_return;
}

ZEND_DLEXPORT zval * zend_std_read_property_overload(zend_object *object, zend_string *member, int type, void **cache_slot, zval *rv) {
    zval *to_return;
    zval obj_zv, member_zv;
    zend_class_entry *scope = NULL;
    zend_execute_data *ex = EG(current_execute_data);
    
    if (aop_g(lock_read_property)>25) {
        zend_error(E_ERROR, "Too many level of nested advices. Are there any recursive call ?");
    }
    
    /* Convert zend_object* to zval* */
    ZVAL_OBJ(&obj_zv, object);
    /* Convert zend_string* to zval* */
    ZVAL_STR(&member_zv, member);
    
    /* Get scope from current execute data */
    if (ex && ex->func) {
        scope = ex->func->common.scope;
    }
    
    aop_g(lock_read_property)++;
    to_return = _test_read_pointcut_and_execute(NULL, NULL, &obj_zv, &member_zv, type, scope);
    aop_g(lock_read_property)--;
    return to_return;
}


void _test_func_pointcut_and_execute(HashPosition *pos, HashTable *ht, zend_execute_data *ex, zval *object, zend_class_entry *scope, zend_class_entry *called_scope, int args_overloaded, zval *args, zval **to_return_ptr_ptr) {
    zval *aop_object, *exception;
    AopJoinpoint_object *obj;
    pointcut *current_pc;
    pointcut **temp;
    HashPosition local_pos;
    if (pos == NULL) {
        pos = &local_pos;
    }
    if (ht==NULL) {
        /* For functions, object should be NULL or not an object */
        zval *func_object = (object && Z_TYPE_P(object) == IS_OBJECT) ? object : NULL;
        ht = get_cache_func (func_object, ex);
        if (ht==NULL) {
            /* No pointcuts match - for now, skip execute_context to avoid segfaults.
             * The original function should execute normally without AOP interception.
             * TODO: Fix execute_context to properly handle PHP 8 argument access.
             */
            aop_g(overloaded) = 0;
            /* Don't call execute_context - let the original function execute normally */
            aop_g(overloaded) = 1;
            return;
        }
        zend_hash_internal_pointer_reset_ex(ht, pos);
    } else {
        zend_hash_move_forward_ex (ht, pos);
    }
    zval *zv = zend_hash_get_current_data_ex(ht, pos);
    if (zv == NULL) {
        aop_g(overloaded) = 0;
        execute_context (ex, object, scope, called_scope,args_overloaded, args, to_return_ptr_ptr);
        aop_g(overloaded) = 1;
        return;
    }
    temp = (pointcut **)Z_PTR_P(zv);
    current_pc = *temp;

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *)((char *)Z_OBJ_P(aop_object) - XtOffsetOf(AopJoinpoint_object, std));
    obj->current_pointcut = current_pc;
    //obj->current_pointcut_index = current_pointcut_index; 
    obj->pos = *pos;
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
            zend_object *exception = EG(exception); 
            /* obj->exception is zval* but exception is zend_object* - storing as zval for compatibility */
            if (obj->exception) {
                zval_ptr_dtor(obj->exception);
                efree(obj->exception);
            }
            obj->exception = emalloc(sizeof(zval));
            ZVAL_OBJ(obj->exception, exception);
            Z_ADDREF_P(obj->exception);
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
    
zval *_test_read_pointcut_and_execute(HashPosition *pos, HashTable *ht, zval *object, zval *member, int type, zend_class_entry *current_scope) {
    zval *to_return = NULL;
    zend_class_entry *scope;
    pointcut **temp;
    pointcut *current_pc;
    AopJoinpoint_object *obj;
    zval *aop_object;
    HashPosition local_pos;
    zval rv_tmp;
    zval *rv = &rv_tmp;
    if (pos == NULL) {
        pos = &local_pos;
    }

    if (ht==NULL) {
        ht = get_cache_property (object, member, AOP_KIND_READ);
        zend_hash_internal_pointer_reset_ex(ht, pos);
    } else {
        zend_hash_move_forward_ex (ht, pos);
    }
    zval *zv = zend_hash_get_current_data_ex(ht, pos);
    if (zv == NULL) {
        scope = current_scope; /* Use passed scope parameter */
        /* Call original handler - need to convert zval* to zend_object* and zend_string* */
        zend_object *obj = Z_OBJ_P(object);
        zend_string *member_str = Z_STR_P(member);
        to_return = zend_std_read_property(obj, member_str, type, NULL, rv);
        return to_return;
    }
    temp = (pointcut **)Z_PTR_P(zv);
    current_pc = *temp;

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *)((char *)Z_OBJ_P(aop_object) - XtOffsetOf(AopJoinpoint_object, std));
    obj->current_pointcut = current_pc;
    obj->pos = *pos;
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

void _test_write_pointcut_and_execute(HashPosition *pos, HashTable *ht, zval *object, zval *member, zval *value, zend_class_entry *current_scope) {
    zval *temp_this, *to_return;
    zend_class_entry *scope;
    pointcut **temp;
    pointcut *current_pc;
    AopJoinpoint_object *obj;
    zval *aop_object;
    HashPosition local_pos;
    if (pos == NULL) {
        pos = &local_pos;
    }

    if (ht==NULL) {
        ht = get_cache_property (object, member, AOP_KIND_WRITE);
        zend_hash_internal_pointer_reset_ex(ht, pos);
    } else {
        zend_hash_move_forward_ex (ht, pos);
    }
    zval *zv = zend_hash_get_current_data_ex(ht, pos);
    if (zv == NULL) {
        scope = current_scope; /* Use passed scope parameter */
        /* Call original handler - need to convert zval* to zend_object* and zend_string* */
        zend_object *obj = Z_OBJ_P(object);
        zend_string *member_str = Z_STR_P(member);
        zend_std_write_property(obj, member_str, value, NULL);
        return;
    }
    temp = (pointcut **)Z_PTR_P(zv);
    current_pc = *temp;

    aop_object = get_aopJoinpoint();
    obj = (AopJoinpoint_object *)((char *)Z_OBJ_P(aop_object) - XtOffsetOf(AopJoinpoint_object, std));
    obj->current_pointcut = current_pc;
    obj->pos = *pos;
    obj->advice = ht;
    obj->kind_of_advice = (current_pc->kind_of_advice&AOP_KIND_READ) ? (current_pc->kind_of_advice - AOP_KIND_READ) : current_pc->kind_of_advice;
    obj->object = object;
    obj->member = member;
    obj->value = value;
    /* Z_SET_ISREF_P removed in PHP 8 - refcount handling is automatic */
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
            zval_ptr_dtor(obj->value);
            efree(obj->value);
        }
        obj->value = emalloc(sizeof(zval));
        *obj->value = zret;
        ZVAL_UNDEF(&zret);
    } else {
        if (!EG(exception)) {
            zval_ptr_dtor(&zret);
        }
    }
    zval_ptr_dtor(&params[0]); /* params[0] is zval, &params[0] is zval* - correct */
}

ZEND_DLEXPORT void zend_std_write_property_overload(zend_object *object, zend_string *member, zval *value, void **cache_slot) {
    zval obj_zv, member_zv;
    zend_class_entry *scope = NULL;
    zend_execute_data *ex = EG(current_execute_data);
    
    if (aop_g(lock_write_property) > 25) {
        zend_error(E_ERROR, "Too many level of nested advices. Are there any recursive call ?");
    }
    
    /* Convert zend_object* to zval* */
    ZVAL_OBJ(&obj_zv, object);
    /* Convert zend_string* to zval* */
    ZVAL_STR(&member_zv, member);
    
    /* Get scope from current execute data */
    if (ex && ex->func) {
        scope = ex->func->common.scope;
    }
    
    aop_g(lock_write_property)++;
    _test_write_pointcut_and_execute(NULL, NULL, &obj_zv, &member_zv, value, scope);
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

/* In PHP 8.4, std_object_handlers is const and cannot be modified.
 * Object handler overriding would need to be done per-class during class registration.
 * For now, this is disabled - property AOP functionality will not work without per-class handler setup.
 */
#if 0
    zend_std_write_property = std_object_handlers.write_property;
    std_object_handlers.write_property = zend_std_write_property_overload;
    zend_std_read_property = std_object_handlers.read_property;
    std_object_handlers.read_property = zend_std_read_property_overload;
    zend_std_get_property_ptr_ptr = std_object_handlers.get_property_ptr_ptr;
    std_object_handlers.get_property_ptr_ptr = zend_std_get_property_ptr_ptr_overload;
#endif

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

static void add_pointcut (zend_fcall_info fci, zend_fcall_info_cache fcic, char *selector, int selector_len, int type , zval *return_value) {
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

/* Simple string replacement helper - forward declaration */
static char *aop_str_replace(const char *haystack, size_t haystack_len, 
                              const char *needle, size_t needle_len,
                              const char *replacement, size_t replacement_len);

void make_regexp_on_pointcut (pointcut **pc) { 
    uint32_t capture_count = 0;
    int *replace_count, *new_length;
    char *regexp;
    char *regexp_buffer;
    char tempregexp[500];
    zend_string *regex_str;

    (*pc)->method_jok = (strchr((*pc)->method, '*') != NULL);
    /* replace_count and new_length no longer needed */
    regexp = estrdup((*pc)->method);
    /* Use simple string replacement */
    regexp_buffer = aop_str_replace(regexp, strlen(regexp), "**\\", 3, "[.#}", 4);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = aop_str_replace(regexp, strlen(regexp), "**", 2, "[.#]", 4);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = aop_str_replace(regexp, strlen(regexp), "\\", 1, "\\\\", 2);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = aop_str_replace(regexp, strlen(regexp), "*", 1, "[^\\\\]*", 6);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = aop_str_replace(regexp, strlen(regexp), "[.#]", 4, ".*", 2);
    efree(regexp);
    regexp = regexp_buffer;
    regexp_buffer = aop_str_replace(regexp, strlen(regexp), "[.#}", 4, "(.*\\\\)?", 7);
    efree(regexp);
    regexp = regexp_buffer;
    if (regexp[0]!='\\') {
        sprintf((char *)tempregexp, "/^%s$/i", regexp);
    } else {
        sprintf((char *)tempregexp, "/^%s$/i", regexp+2);
    }
    efree(regexp);
    regex_str = zend_string_init(tempregexp, strlen(tempregexp), 0);
    (*pc)->re_method = pcre_get_compiled_regex(regex_str, &capture_count);
    zend_string_release(regex_str);
    //efree(tempregexp);
    if (!(*pc)->re_method) {
        php_error_docref(NULL, E_WARNING, "Invalid expression");
    }
    if ((*pc)->class_name != NULL) {
        regexp = estrdup((*pc)->class_name);
        regexp_buffer = aop_str_replace(regexp, strlen(regexp), "**\\", 3, "[.#}", 4);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = aop_str_replace(regexp, strlen(regexp), "**", 2, "[.#]", 4);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = aop_str_replace(regexp, strlen(regexp), "\\", 1, "\\\\", 2);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = aop_str_replace(regexp, strlen(regexp), "*", 1, "[^\\\\]*", 6);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = aop_str_replace(regexp, strlen(regexp), "[.#]", 4, ".*", 2);
        efree(regexp);
        regexp = regexp_buffer;
        regexp_buffer = aop_str_replace(regexp, strlen(regexp), "[.#}", 4, "(.*\\\\)?", 7);
        efree(regexp);
        regexp = regexp_buffer;
        if (regexp[0]!='\\') {
            sprintf((char *)tempregexp, "/^%s$/i", regexp);
        } else {
            sprintf((char *)tempregexp, "/^%s$/i", regexp+2);
        }
        efree(regexp);
        regex_str = zend_string_init(tempregexp, strlen(tempregexp), 0);
        (*pc)->re_class = pcre_get_compiled_regex(regex_str, &capture_count);
        zend_string_release(regex_str);
        if (!(*pc)->re_class) {
            php_error_docref(NULL, E_WARNING, "Invalid expression");
        }
    }
    /* replace_count and new_length no longer needed */
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
    if (Z_TYPE(fci.function_name) != IS_UNDEF) {
        Z_ADDREF_P(&fci.function_name);
    }
    if (fci.object != NULL) {
        /* fci.object is zend_object*, refcounting handled by zend_call_function */
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AROUND, return_value);
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
    if (Z_TYPE(fci.function_name) != IS_UNDEF) {
        Z_ADDREF_P(&fci.function_name);
    }
    if (fci.object != NULL) {
        /* fci.object is zend_object*, refcounting handled by zend_call_function */
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_BEFORE, return_value);
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
    if (Z_TYPE(fci.function_name) != IS_UNDEF) {
        Z_ADDREF_P(&fci.function_name);
    }
    if (fci.object != NULL) {
        /* fci.object is zend_object*, refcounting handled by zend_call_function */
    }

    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_CATCH, return_value);

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
    if (Z_TYPE(fci.function_name) != IS_UNDEF) {
        Z_ADDREF_P(&fci.function_name);
    }
    if (fci.object != NULL) {
        /* fci.object is zend_object*, refcounting handled by zend_call_function */
    }

    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_RETURN, return_value);

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
    if (Z_TYPE(fci.function_name) != IS_UNDEF) {
        Z_ADDREF_P(&fci.function_name);
    }
    if (fci.object != NULL) {
        /* fci.object is zend_object*, refcounting handled by zend_call_function */
    }
    add_pointcut(fci, fcic, selector, selector_len, AOP_KIND_AFTER|AOP_KIND_CATCH|AOP_KIND_RETURN, return_value);
}


ZEND_DLEXPORT void aop_execute_ex (zend_execute_data *execute_data) {
    zend_op_array *op_array = &execute_data->func->op_array;
    if (aop_g(in_ex)) {
        aop_g(in_ex) = 0;
        _zend_execute_ex(execute_data);
    } else {
        /* zend_vm_stack_free() removed in PHP 8 - stack management is automatic */
        /* EG(This) removed in PHP 8 - This is per-execute-data */
        EG(current_execute_data) = execute_data->prev_execute_data;
        aop_execute(op_array);
    }
}

ZEND_DLEXPORT void _zend_execute_overload (zend_op_array *ops, zval *return_value) {
    aop_g(in_ex) = 1;
    zend_execute(ops, return_value);
}

ZEND_DLEXPORT void aop_execute (zend_op_array *ops) {
    zend_execute_data *data;
    zend_function *curr_func = NULL;
    zval retval;
    ZVAL_UNDEF(&retval);

    if (!aop_g(aop_enable)) {
        zval dummy_retval;
        ZVAL_UNDEF(&dummy_retval);
        _zend_execute(ops, &dummy_retval);
        return;
    }

    data = EG(current_execute_data);

    if (data) {
        curr_func = data->func;
    }
    if (ops->type==ZEND_EVAL_CODE || curr_func == NULL || curr_func->common.function_name == NULL || aop_g(overloaded) || EG(exception)) {
        zval dummy_retval;
        ZVAL_UNDEF(&dummy_retval);
        _zend_execute(ops, &dummy_retval);
        return;
    }
    
    aop_g(overloaded) = 1;
    {
        zend_execute_data *ex = EG(current_execute_data);
        zval *this_zv = ex ? &ex->This : NULL;
        zend_class_entry *scope = ex && ex->func ? ex->func->common.scope : NULL;
        zend_class_entry *called_scope = scope;
        zval *retval_ptr = &retval;
        _test_func_pointcut_and_execute(NULL, NULL, ex, this_zv, scope, called_scope, 0, NULL, &retval_ptr);
    }
    aop_g(overloaded) = 0;
}

ZEND_DLEXPORT void aop_execute_internal (zend_execute_data *current_execute_data, zval *return_value) {
        zend_execute_data *data;
        zend_function *curr_func = NULL;
        zval *to_return_ptr = return_value;
        zend_class_entry *scope = NULL;
        zend_class_entry *called_scope = NULL;

        if (!aop_g(aop_enable)) {
            if (_zend_execute_internal) {
                _zend_execute_internal(current_execute_data, return_value);
            } else {
                zend_execute_internal(current_execute_data, return_value);
            }
            return;
        }

        data = EG(current_execute_data);

        if (data) {
            curr_func = data->func;
            if (curr_func) {
                scope = curr_func->common.scope;
                called_scope = scope; /* called_scope same as scope for internal functions */
            }
        }
        if (curr_func == NULL || curr_func->common.function_name == NULL || aop_g(overloaded) || EG(exception)) {
            if (_zend_execute_internal) {
                _zend_execute_internal(current_execute_data, return_value);
            } else {
                zend_execute_internal(current_execute_data, return_value);
            }
            return;
        }   

        aop_g(overloaded) = 1;
        {
            zval *this_zv = NULL;
            zend_execute_data *ex_data = current_execute_data ? current_execute_data : EG(current_execute_data);
            if (ex_data && Z_TYPE(ex_data->This) == IS_OBJECT) {
                this_zv = &ex_data->This;
            }
            _test_func_pointcut_and_execute(NULL,NULL, ex_data, this_zv, scope, called_scope, 0, NULL, &to_return_ptr);
        }
        aop_g(overloaded) = 0;
    }

    /*
     * execute_context:
     * Re-implements the \"proceed\" logic for joinpoints using the modern
     * zend_call_function() API instead of manipulating removed executor
     * globals. This makes the extension compatible with PHP 8+ executor changes.
     */
    static void execute_context (zend_execute_data *ex, zval *object, zend_class_entry *calling_scope, zend_class_entry *called_scope, int args_overloaded, zval *args, zval **to_return_ptr_ptr) {
        zend_fcall_info fci;
        zend_fcall_info_cache fcc;
        zval retval;
        zval *fci_params = NULL;
        int arg_count = 0;
        int i = 0;

        if (!EG(active) || EG(exception)) {
            return;
        }

        memset(&fci, 0, sizeof(fci));
        memset(&fcc, 0, sizeof(fcc));
        ZVAL_UNDEF(&retval);

        fci.size = sizeof(fci);
        fci.object = (object && Z_TYPE_P(object) == IS_OBJECT) ? Z_OBJ_P(object) : NULL;
        fci.retval = &retval;
        /* no_separation removed in PHP 8 */

        /* Build argument list either from overloaded args array or from the current call frame */
        if (args_overloaded && args && Z_TYPE_P(args) == IS_ARRAY) {
            HashTable *ht = Z_ARRVAL_P(args);
            zval *zv;

            arg_count = zend_hash_num_elements(ht);
            if (arg_count > 0) {
                fci_params = safe_emalloc(arg_count, sizeof(zval), 0);
                ZEND_HASH_FOREACH_VAL(ht, zv) {
                    ZVAL_COPY(&fci_params[i], zv);
                    i++;
                } ZEND_HASH_FOREACH_END();
            }
        } else {
            if (!ex || !ex->func) {
                return; /* Can't get arguments without execute_data */
            }
            arg_count = ZEND_CALL_NUM_ARGS(ex);
            if (arg_count > 0) {
                fci_params = safe_emalloc(arg_count, sizeof(zval), 0);
                /* In PHP 8, arguments are stored in the call frame */
                zval *call_args = ZEND_CALL_VAR_NUM(ex, 0);
                if (call_args) {
                    for (i = 0; i < arg_count; i++) {
                        zval *src = ZEND_CALL_VAR_NUM(ex, i);
                        if (src && Z_TYPE_P(src) != IS_UNDEF) {
                            ZVAL_COPY(&fci_params[i], src);
                        } else {
                            ZVAL_NULL(&fci_params[i]);
                        }
                    }
                } else {
                    /* Fallback: try ZEND_CALL_ARG */
                    for (i = 0; i < arg_count; i++) {
                        zval *src = ZEND_CALL_ARG(ex, i);
                        if (src && Z_TYPE_P(src) != IS_UNDEF) {
                            ZVAL_COPY(&fci_params[i], src);
                        } else {
                            ZVAL_NULL(&fci_params[i]);
                        }
                    }
                }
            }
        }

        fci.param_count = arg_count;
        fci.params = fci_params;

        if (!ex || !ex->func) {
            if (fci_params) {
                for (i = 0; i < arg_count; i++) {
                    zval_ptr_dtor(&fci_params[i]);
                }
                efree(fci_params);
            }
            return; /* Can't call without function */
        }

        /* For user functions, we need to execute the op_array directly */
        if (ex->func->type == ZEND_USER_FUNCTION) {
            /* For user functions, we can't easily re-execute them here.
             * The original code relied on executor globals that no longer exist.
             * This is a limitation - around advice on user functions may not work correctly.
             * We'll try to call it via zend_call_function but it may not work for all cases.
             */
            if (ex->func->common.function_name) {
                ZVAL_STR_COPY(&fci.function_name, ex->func->common.function_name);
            }
            fcc.function_handler = ex->func;
            fcc.calling_scope = calling_scope ? calling_scope : ex->func->common.scope;
            fcc.called_scope = called_scope ? called_scope : fcc.calling_scope;
            fcc.object = fci.object;

            if (zend_call_function(&fci, &fcc) == SUCCESS) {
                if (to_return_ptr_ptr) {
                    if (*to_return_ptr_ptr == NULL) {
                        *to_return_ptr_ptr = emalloc(sizeof(zval));
                    }
                    ZVAL_COPY(*to_return_ptr_ptr, &retval);
                }
            }

            if (!Z_ISUNDEF(fci.function_name)) {
                zval_ptr_dtor(&fci.function_name);
            }
        } else {
            /* For internal functions, use the function handler */
            fcc.function_handler = ex->func;
            fcc.calling_scope = calling_scope ? calling_scope : ex->func->common.scope;
            fcc.called_scope = called_scope ? called_scope : fcc.calling_scope;
            fcc.object = fci.object;

            if (zend_call_function(&fci, &fcc) == SUCCESS) {
                if (to_return_ptr_ptr) {
                    if (*to_return_ptr_ptr == NULL) {
                        *to_return_ptr_ptr = emalloc(sizeof(zval));
                    }
                    ZVAL_COPY(*to_return_ptr_ptr, &retval);
                }
            }
        }

        if (!Z_ISUNDEF(retval)) {
            zval_ptr_dtor(&retval);
        }

        if (fci_params) {
            for (i = 0; i < arg_count; i++) {
                zval_ptr_dtor(&fci_params[i]);
            }
            efree(fci_params);
        }
    }

    /* Simple string replacement helper - implementation */
    static char *aop_str_replace(const char *haystack, size_t haystack_len, 
                                  const char *needle, size_t needle_len,
                                  const char *replacement, size_t replacement_len) {
        const char *pos;
        char *result;
        size_t result_len = 0;
        size_t count = 0;
        const char *p = haystack;
        
        /* Count occurrences */
        while ((pos = strstr(p, needle)) != NULL) {
            count++;
            p = pos + needle_len;
        }
        
        if (count == 0) {
            return estrndup(haystack, haystack_len);
        }
        
        /* Calculate result length */
        result_len = haystack_len + (replacement_len - needle_len) * count;
        result = emalloc(result_len + 1);
        
        /* Perform replacement */
        p = haystack;
        char *out = result;
        while ((pos = strstr(p, needle)) != NULL) {
            size_t copy_len = pos - p;
            memcpy(out, p, copy_len);
            out += copy_len;
            memcpy(out, replacement, replacement_len);
            out += replacement_len;
            p = pos + needle_len;
        }
        memcpy(out, p, haystack + haystack_len - p);
        out[result_len] = '\0';
        
        return result;
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
        int i;
        pcre2_match_data *match_data;
        int rc;
        zend_string *subject;

        if (!pc->re_class) return 0;

        subject = ce->name;
        match_data = pcre2_match_data_create_from_pattern(pc->re_class, NULL);
        if (match_data) {
            rc = pcre2_match(pc->re_class, (PCRE2_SPTR)ZSTR_VAL(subject), ZSTR_LEN(subject), 0, 0, match_data, NULL);
            pcre2_match_data_free(match_data);
            if (rc >= 0) return 1;
        }

        for (i = 0; i < (int) ce->num_interfaces; i++) {
            subject = ce->interfaces[i]->name;
            match_data = pcre2_match_data_create_from_pattern(pc->re_class, NULL);
            if (match_data) {
                rc = pcre2_match(pc->re_class, (PCRE2_SPTR)ZSTR_VAL(subject), ZSTR_LEN(subject), 0, 0, match_data, NULL);
                pcre2_match_data_free(match_data);
                if (rc >= 0) return 1;
            }
        }

        /* Traits are no longer directly accessible in PHP 8.4 - would need to iterate through class hierarchy differently */
        /* Skipping trait matching for now */

        ce = ce->parent;
        while (ce != NULL) {
            subject = ce->name;
            match_data = pcre2_match_data_create_from_pattern(pc->re_class, NULL);
            if (match_data) {
                rc = pcre2_match(pc->re_class, (PCRE2_SPTR)ZSTR_VAL(subject), ZSTR_LEN(subject), 0, 0, match_data, NULL);
                pcre2_match_data_free(match_data);
                if (rc >= 0) return 1;
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
            pcre2_match_data *match_data;
            int matches;
            zend_string *subject = curr_func->common.function_name;

            if (!pc->re_method) {
                return 0;
            }

            match_data = pcre2_match_data_create_from_pattern(pc->re_method, NULL);
            if (!match_data) {
                return 0;
            }
            matches = pcre2_match(pc->re_method, (PCRE2_SPTR)ZSTR_VAL(subject), ZSTR_LEN(subject), 0, 0, match_data, NULL);
            pcre2_match_data_free(match_data);

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
        /* zend_execute_ex assignment removed - not needed in PHP 8.4 */
#else
        zend_execute  = _zend_execute;
#endif
        zend_execute_internal  = _zend_execute_internal;
        
        UNREGISTER_INI_ENTRIES();
        return SUCCESS;
    }


    HashTable *calculate_class_pointcuts (zend_class_entry *ce, int kind_of_advice) {
        pointcut **pc;
        HashPosition pos;
        HashTable *ht;
        ALLOC_HASHTABLE(ht);
        //No free because pointcuts are free in the aop_g(pointcuts)
        zend_hash_init(ht, 16, NULL, NULL ,0);


        zend_hash_internal_pointer_reset_ex(aop_g(pointcuts), &pos);
        zval *zv;
        while ((zv = zend_hash_get_current_data_ex(aop_g(pointcuts), &pos)) != NULL) {
            pc = (pointcut **)Z_PTR_P(zv);
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
        HashPosition pos;
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
        {
            zval *zv;
            while ((zv = zend_hash_get_current_data_ex(class_pointcuts, &pos)) != NULL) {
                pc = (pointcut **)Z_PTR_P(zv);
                if (pointcut_match_zend_function((*pc), curr_func, ex)) {
                    zval zv_insert;
                    ZVAL_PTR(&zv_insert, pc);
                    zend_hash_next_index_insert(ht, &zv_insert);
                }
                zend_hash_move_forward_ex (class_pointcuts, &pos);
            }
        }

        zend_hash_destroy(class_pointcuts);
        FREE_HASHTABLE(class_pointcuts);

        return ht;
    }
    HashTable *calculate_property_pointcuts (zval *object, zval *member, int kind AOP_KEY_D) {

        zval *tmp_member;
        HashTable *class_pointcuts;
        HashTable *ht;
        HashPosition pos;
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
        /* key (zend_literal) removed in PHP 8.4 */
    }


        zend_hash_internal_pointer_reset_ex(class_pointcuts, &pos);
        {
            zval *zv;
            while ((zv = zend_hash_get_current_data_ex(class_pointcuts, &pos)) != NULL) {
                pc = (pointcut **)Z_PTR_P(zv);
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
    if (!object || Z_TYPE_P(object) != IS_OBJECT) {
        return NULL;
    }
    object_cache *cache = get_object_cache(object);
    if (!cache) {
        return NULL;
    }
    if (cache->func == NULL) {
        ALLOC_HASHTABLE(cache->func);
        zend_hash_init(cache->func, 16, NULL, (dtor_func_t)free_pointcut_cache, 0);
    }
    return cache->func;

}

object_cache *get_object_cache (zval *object)
{
    int i;
    uint32_t handle;
    
    if (!object || Z_TYPE_P(object) != IS_OBJECT) {
        return NULL;
    }
    
    zend_object *obj = Z_OBJ_P(object);
    if (!obj) {
        return NULL;
    }
    
    handle = obj->handle;
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
        /* key (zend_literal) removed in PHP 8.4 */
        member_need_free = 1;
    }
    
    key_str = Z_STRVAL_P(member);
    key_len = Z_STRLEN_P(member);
    /* h no longer needed - zend_hash_str_* functions compute hash internally */
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
        zval_ptr_dtor(member);
        efree(member);
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
    if (object == NULL || (object && Z_TYPE_P(object) != IS_OBJECT)) {
        ht_object_cache = aop_g(function_cache);
        if (curr_func && curr_func->common.fn_flags & ZEND_ACC_STATIC) {
            key_str = (char *)emalloc (ZSTR_LEN(curr_func->common.scope->name) + ZSTR_LEN(curr_func->common.function_name) + 3);
            sprintf((char *)key_str, "%s::%s", ZSTR_VAL(curr_func->common.scope->name), ZSTR_VAL(curr_func->common.function_name));
            key_len = strlen (key_str);
        } else {
            if (!curr_func || !curr_func->common.function_name) {
                return NULL;
            }
            key_str = estrdup(ZSTR_VAL(curr_func->common.function_name));
            key_len = strlen(key_str);
        }
    } else {
        if (!object || Z_TYPE_P(object) != IS_OBJECT || !curr_func || !curr_func->common.function_name) {
            return NULL;
        }
        key_str = estrdup(ZSTR_VAL(curr_func->common.function_name));
        key_len = strlen(key_str);
        ht_object_cache = get_object_cache_func(object);
        if (!ht_object_cache) {
            efree(key_str);
            return NULL;
        }
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
