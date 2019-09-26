/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2016 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Frank M. Kromann <frank@kromann.info>                        |
   +----------------------------------------------------------------------+
 */

/* $Id$ */

//makes VSCode stop complaining
#include "config.h"

#ifdef COMPILE_DL_MSSQL
#define HAVE_MSSQL 1
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_globals.h"
#include "ext/standard/php_standard.h"
#include "ext/standard/info.h"
#include "php_mssql.h"
#include "php_ini.h"

#if HAVE_MSSQL
#define SAFE_STRING(s) ((s)?(s):"")

#define MSSQL_ASSOC		1<<0
#define MSSQL_NUM		1<<1
#define MSSQL_BOTH		(MSSQL_ASSOC|MSSQL_NUM)

static int le_result, le_link, le_plink, le_statement;

static void php_mssql_get_column_content_with_type(mssql_link *mssql_ptr,int offset,zval *result, int column_type TSRMLS_DC);
static void php_mssql_get_column_content_without_type(mssql_link *mssql_ptr,int offset,zval *result, int column_type TSRMLS_DC);

static void _mssql_bind_hash_dtor(void *data);

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_connect, 0, 0, 0)
	ZEND_ARG_INFO(0, servername)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, newlink)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_close, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_select_db, 0, 0, 1)
	ZEND_ARG_INFO(0, database_name)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_query, 0, 0, 1)
	ZEND_ARG_INFO(0, query)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, batch_size)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_fetch_batch, 0, 0, 1)
	ZEND_ARG_INFO(0, result)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_rows_affected, 0, 0, 1)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_mssql_get_last_message, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_fetch_field, 0, 0, 1)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_INFO(0, field_offset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_fetch_array, 0, 0, 1)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_INFO(0, result_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_fetch_assoc, 0, 0, 1)
	ZEND_ARG_INFO(0, result_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_field_length, 0, 0, 1)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_data_seek, 0, 0, 2)
	ZEND_ARG_INFO(0, result_identifier)
	ZEND_ARG_INFO(0, row_number)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_result, 0, 0, 3)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_INFO(0, row)
	ZEND_ARG_INFO(0, field)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_min_error_severity, 0, 0, 1)
	ZEND_ARG_INFO(0, severity)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_init, 0, 0, 1)
	ZEND_ARG_INFO(0, sp_name)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_bind, 0, 0, 4)
	ZEND_ARG_INFO(0, stmt)
	ZEND_ARG_INFO(0, param_name)
	ZEND_ARG_INFO(1, var)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, is_output)
	ZEND_ARG_INFO(0, is_null)
	ZEND_ARG_INFO(0, maxlen)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_execute, 0, 0, 1)
	ZEND_ARG_INFO(0, stmt)
	ZEND_ARG_INFO(0, skip_results)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_free_statement, 0, 0, 1)
	ZEND_ARG_INFO(0, stmt)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mssql_guid_string, 0, 0, 1)
	ZEND_ARG_INFO(0, binary)
	ZEND_ARG_INFO(0, short_format)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ mssql_functions
*/
const zend_function_entry mssql_functions[] = {
	PHP_FE(mssql_connect,				arginfo_mssql_connect)
	PHP_FE(mssql_pconnect,				arginfo_mssql_connect)
	PHP_FE(mssql_close,					arginfo_mssql_close)
	PHP_FE(mssql_select_db,				arginfo_mssql_select_db)
	PHP_FE(mssql_query,					arginfo_mssql_query)
	PHP_FE(mssql_fetch_batch,			arginfo_mssql_fetch_batch)
	PHP_FE(mssql_rows_affected,			arginfo_mssql_rows_affected)
	PHP_FE(mssql_free_result,			arginfo_mssql_fetch_batch)
	PHP_FE(mssql_get_last_message,		arginfo_mssql_get_last_message)
	PHP_FE(mssql_num_rows,				arginfo_mssql_fetch_batch)
	PHP_FE(mssql_num_fields,			arginfo_mssql_fetch_batch)
	PHP_FE(mssql_fetch_field,			arginfo_mssql_fetch_field)
	PHP_FE(mssql_fetch_row,				arginfo_mssql_fetch_batch)
	PHP_FE(mssql_fetch_array,			arginfo_mssql_fetch_array)
	PHP_FE(mssql_fetch_assoc,			arginfo_mssql_fetch_assoc)
	PHP_FE(mssql_fetch_object,			arginfo_mssql_fetch_batch)
	PHP_FE(mssql_field_length,			arginfo_mssql_field_length)
	PHP_FE(mssql_field_name,			arginfo_mssql_field_length)
	PHP_FE(mssql_field_type,			arginfo_mssql_field_length)
	PHP_FE(mssql_data_seek,				arginfo_mssql_data_seek)
	PHP_FE(mssql_field_seek,			arginfo_mssql_fetch_field)
	PHP_FE(mssql_result,				arginfo_mssql_result)
	PHP_FE(mssql_next_result,			arginfo_mssql_fetch_assoc)
	PHP_FE(mssql_min_error_severity,	arginfo_mssql_min_error_severity)
	PHP_FE(mssql_min_message_severity,	arginfo_mssql_min_error_severity)
 	PHP_FE(mssql_init,					arginfo_mssql_init)
 	PHP_FE(mssql_bind,					arginfo_mssql_bind)
 	PHP_FE(mssql_execute,				arginfo_mssql_execute)
	PHP_FE(mssql_free_statement,		arginfo_mssql_free_statement)
 	PHP_FE(mssql_guid_string,			arginfo_mssql_guid_string)
	PHP_FE_END
};
/* }}} */

ZEND_DECLARE_MODULE_GLOBALS(mssql)
static PHP_GINIT_FUNCTION(mssql);

/* {{{ mssql_module_entry
*/
zend_module_entry mssql_module_entry = 
{
	STANDARD_MODULE_HEADER,
	"mssql", 
	mssql_functions, 
	PHP_MINIT(mssql), 
	PHP_MSHUTDOWN(mssql), 
	PHP_RINIT(mssql), 
	PHP_RSHUTDOWN(mssql), 
	PHP_MINFO(mssql), 
	NO_VERSION_YET,
	PHP_MODULE_GLOBALS(mssql),
	PHP_GINIT(mssql),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_MSSQL
ZEND_GET_MODULE(mssql)
#endif

#define CHECK_LINK(link) { if (link==NULL) { php_error_docref(NULL, E_WARNING, "A link to the server could not be established"); RETURN_FALSE; } }
#define CHECK_RES(res) { if (res==NULL) { RETURN_FALSE; } }

/* {{{ PHP_INI_DISP
*/
static PHP_INI_DISP(display_text_size)
{
	char *value;
//	TSRMLS_FETCH();
	
    if (type == PHP_INI_DISPLAY_ORIG && ini_entry->modified) {
		value = ZSTR_VAL(ini_entry->orig_value);
	} else if (ini_entry->value) {
		value = ZSTR_VAL(ini_entry->value);
	} else {
		value = NULL;
	}

	if (atoi(value) == -1) {
		PUTS("Server default");
	} else {
		php_printf("%s", value);
	}
}
/* }}} */

/* {{{ PHP_INI
*/
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("mssql.allow_persistent",		"1",	PHP_INI_SYSTEM,	OnUpdateBool,	allow_persistent,			zend_mssql_globals,		mssql_globals)
	STD_PHP_INI_ENTRY_EX("mssql.max_persistent",		"-1",	PHP_INI_SYSTEM,	OnUpdateLong,	max_persistent,				zend_mssql_globals,		mssql_globals,	display_link_numbers)
	STD_PHP_INI_ENTRY_EX("mssql.max_links",				"-1",	PHP_INI_SYSTEM,	OnUpdateLong,	max_links,					zend_mssql_globals,		mssql_globals,	display_link_numbers)
	STD_PHP_INI_ENTRY_EX("mssql.min_error_severity",	"10",	PHP_INI_ALL,	OnUpdateLong,	cfg_min_error_severity,		zend_mssql_globals,		mssql_globals,	display_link_numbers)
	STD_PHP_INI_ENTRY_EX("mssql.min_message_severity",	"10",	PHP_INI_ALL,	OnUpdateLong,	cfg_min_message_severity,	zend_mssql_globals,		mssql_globals,	display_link_numbers)
	/*
	  mssql.compatAbility_mode (with typo) was used for relatively long time.
	  Unless it is fixed the old version is also kept for compatibility reasons.
	*/
	STD_PHP_INI_BOOLEAN("mssql.compatability_mode",		"0",	PHP_INI_ALL,	OnUpdateBool,	compatibility_mode,			zend_mssql_globals,		mssql_globals)
	STD_PHP_INI_BOOLEAN("mssql.compatibility_mode",		"0",	PHP_INI_ALL,	OnUpdateBool,	compatibility_mode,			zend_mssql_globals,		mssql_globals)
	STD_PHP_INI_ENTRY_EX("mssql.connect_timeout",    	"5",	PHP_INI_ALL,	OnUpdateLong,	connect_timeout,			zend_mssql_globals,		mssql_globals,	display_link_numbers)
	STD_PHP_INI_ENTRY_EX("mssql.timeout",      			"60",	PHP_INI_ALL,	OnUpdateLong,	timeout,					zend_mssql_globals,		mssql_globals,	display_link_numbers)
	STD_PHP_INI_ENTRY_EX("mssql.textsize",   			"-1",	PHP_INI_ALL,	OnUpdateLong,	textsize,					zend_mssql_globals,		mssql_globals,	display_text_size)
	STD_PHP_INI_ENTRY_EX("mssql.textlimit",   			"-1",	PHP_INI_ALL,	OnUpdateLong,	textlimit,					zend_mssql_globals,		mssql_globals,	display_text_size)
	STD_PHP_INI_ENTRY_EX("mssql.batchsize",   			"0",	PHP_INI_ALL,	OnUpdateLong,	batchsize,					zend_mssql_globals,		mssql_globals,	display_link_numbers)
	STD_PHP_INI_BOOLEAN("mssql.datetimeconvert",  		"1",	PHP_INI_ALL,	OnUpdateBool,	datetimeconvert,			zend_mssql_globals,		mssql_globals)
	STD_PHP_INI_BOOLEAN("mssql.secure_connection",		"0",	PHP_INI_SYSTEM, OnUpdateBool,	secure_connection,			zend_mssql_globals,		mssql_globals)
	STD_PHP_INI_ENTRY_EX("mssql.max_procs",				"-1",	PHP_INI_ALL,	OnUpdateLong,	max_procs,					zend_mssql_globals,		mssql_globals,	display_link_numbers)
#ifdef HAVE_FREETDS
	STD_PHP_INI_ENTRY("mssql.charset",					"",		PHP_INI_ALL,	OnUpdateString,	charset,					zend_mssql_globals,		mssql_globals)
#endif
PHP_INI_END()
/* }}} */

/* error handler */
static int php_mssql_error_handler(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
//	TSRMLS_FETCH();

	if (severity >= MS_SQL_G(min_error_severity)) {
		php_error_docref(NULL, E_WARNING, "%s (severity %d)", dberrstr, severity);
	}
	return INT_CANCEL;  
}

/* {{{ php_mssql_message_handler
*/
/* message handler */
static int php_mssql_message_handler(DBPROCESS *dbproc, DBINT msgno,int msgstate, int severity,char *msgtext,char *srvname, char *procname,DBUSMALLINT line)
{
//	TSRMLS_FETCH();

	if (severity >= MS_SQL_G(min_message_severity)) {
		php_error_docref(NULL, E_WARNING, "message: %s (severity %d)", msgtext, severity);
	}
	if (MS_SQL_G(server_message)) {
		efree(MS_SQL_G(server_message));
		MS_SQL_G(server_message) = NULL;
	}
	MS_SQL_G(server_message) = estrdup(msgtext);
	return 0;
}
/* }}} */

/* {{{ _clean_invalid_results
*/
static int _clean_invalid_results(zval *v)
{
	if (Z_TYPE_P(v) == IS_RESOURCE) {
		zend_resource *res = Z_RES_P(v);
		if (res != NULL && res->type == le_result) {
			mssql_link *mssql_ptr = ((mssql_result *) res->ptr)->mssql_ptr;
			
			if (!mssql_ptr->valid) {
				return ZEND_HASH_APPLY_REMOVE;
			}
		}
	}
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

/* {{{ _free_result
*/
static void _free_result(mssql_result *result, int free_fields) 
{
	int i,j;

	if (result->data) {
		for (i=0; i<result->num_rows; i++) {
			if (result->data[i]) {
				for (j=0; j<result->num_fields; j++) {
					zval_dtor(&result->data[i][j]);
				}
				efree(result->data[i]);
			}
		}
		efree(result->data);
		result->data = NULL;
		result->blocks_initialized = 0;
	}
	
	if (free_fields && result->fields) {
		for (i=0; i<result->num_fields; i++) {
			efree(result->fields[i].name);
			efree(result->fields[i].column_source);
		}
		efree(result->fields);
	}
}
/* }}} */

/* {{{ _free_mssql_statement(zend_resource *res)
*/
static ZEND_RSRC_DTOR_FUNC(_free_mssql_statement)
{
	mssql_statement *statement = (mssql_statement *)res->ptr;

	if (statement->binds) {
		zend_hash_destroy(statement->binds);
		efree(statement->binds);
	}
	
	efree(statement);
}
/* }}} */

/* {{{ _free_mssql_result(zend_resource *res)
*/
static ZEND_RSRC_DTOR_FUNC(_free_mssql_result)
{
	mssql_result *result = (mssql_result *)res->ptr;

	_free_result(result, 1);
	dbcancel(result->mssql_ptr->link);
	efree(result);
}
/* }}} */

/* {{{ php_mssql_set_default_link
*/
static void php_mssql_set_default_link(zend_resource* link)
{
	if (MS_SQL_G(default_link) == link){
		return;
	}

	if (MS_SQL_G(default_link) != NULL) {
		zend_list_delete(MS_SQL_G(default_link));
	//	GC_REFCOUNT(MS_SQL_G(default_link))--;
	}
	MS_SQL_G(default_link) = link;
	GC_REFCOUNT(link)++;
}
/* }}} */

/* {{{ php_mssql_get_default_link
*/
static inline zend_resource* php_mssql_get_default_link()
{
	return MS_SQL_G(default_link);
}
/* }}} */

/* {{{ _close_mssql_link(zend_resource *res)
*/
static ZEND_RSRC_DTOR_FUNC(_close_mssql_link)
{
	mssql_link *mssql_ptr = (mssql_link *)res->ptr;

	mssql_ptr->valid = 0;
	zend_hash_apply(&EG(regular_list),(apply_func_t)_clean_invalid_results);
	dbclose(mssql_ptr->link);
	dbfreelogin(mssql_ptr->login);
	efree(mssql_ptr);
	MS_SQL_G(num_links)--;
}
/* }}} */

/* {{{ _close_mssql_plink(zend_resource *res)
*/
static ZEND_RSRC_DTOR_FUNC(_close_mssql_plink)
{
	mssql_link *mssql_ptr = (mssql_link *)res->ptr;

	dbclose(mssql_ptr->link);
	dbfreelogin(mssql_ptr->login);
	free(mssql_ptr);
	MS_SQL_G(num_persistent)--;
	MS_SQL_G(num_links)--;
}
/* }}} */

/* {{{ _mssql_bind_hash_dtor
*/
static void _mssql_bind_hash_dtor(void *data)
{
	mssql_bind *bind= (mssql_bind *) data;

   	zval_ptr_dtor(bind->zval);
}
/* }}} */

/* {{{ PHP_GINIT_FUNCTION
*/
static PHP_GINIT_FUNCTION(mssql)
{
	long compatibility_mode;

	mssql_globals->num_persistent = 0;
	mssql_globals->get_column_content = php_mssql_get_column_content_with_type;
	if (cfg_get_long("mssql.compatibility_mode", &compatibility_mode) == SUCCESS) {
		if (compatibility_mode) {
			mssql_globals->get_column_content = php_mssql_get_column_content_without_type;	
		}
	}
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
*/
PHP_MINIT_FUNCTION(mssql)
{
	REGISTER_INI_ENTRIES();

	le_statement = zend_register_list_destructors_ex(_free_mssql_statement, NULL, "mssql statement", module_number);
	le_result = zend_register_list_destructors_ex(_free_mssql_result, NULL, "mssql result", module_number);
	le_link = zend_register_list_destructors_ex(_close_mssql_link, NULL, "mssql link", module_number);
	le_plink = zend_register_list_destructors_ex(NULL, _close_mssql_plink, "mssql link persistent", module_number);
	mssql_module_entry.type = type;

	if (dbinit()==FAIL) {
		return FAILURE;
	}

	/* BEGIN MSSQL data types for mssql_bind */
	REGISTER_LONG_CONSTANT("MSSQL_ASSOC", MSSQL_ASSOC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MSSQL_NUM", MSSQL_NUM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MSSQL_BOTH", MSSQL_BOTH, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("SQLTEXT",SQLTEXT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLVARCHAR",SQLVARCHAR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLCHAR",SQLCHAR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLINT1",SQLINT1, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLINT2",SQLINT2, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLINT4",SQLINT4, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLBIT",SQLBIT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLFLT4",SQLFLT4, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLFLT8",SQLFLT8, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SQLFLTN",SQLFLTN, CONST_CS | CONST_PERSISTENT);
	/* END MSSQL data types for mssql_bind */

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
*/
PHP_MSHUTDOWN_FUNCTION(mssql)
{
	UNREGISTER_INI_ENTRIES();
#ifndef HAVE_FREETDS
	dbwinexit();
#else
	dbexit();
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
*/
PHP_RINIT_FUNCTION(mssql)
{
	MS_SQL_G(default_link) = NULL;
	MS_SQL_G(num_links) = MS_SQL_G(num_persistent);
	MS_SQL_G(appname) = estrndup("PHP 7", 5);
	MS_SQL_G(server_message) = NULL;
	MS_SQL_G(min_error_severity) = MS_SQL_G(cfg_min_error_severity);
	MS_SQL_G(min_message_severity) = MS_SQL_G(cfg_min_message_severity);
	if (MS_SQL_G(connect_timeout) < 1) MS_SQL_G(connect_timeout) = 1;
	if (MS_SQL_G(timeout) < 0) MS_SQL_G(timeout) = 60;
	if (MS_SQL_G(max_procs) != -1) {
		dbsetmaxprocs((TDS_SHORT)MS_SQL_G(max_procs));
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
*/
PHP_RSHUTDOWN_FUNCTION(mssql)
{
	efree(MS_SQL_G(appname));
	MS_SQL_G(appname) = NULL;
	if (MS_SQL_G(server_message)) {
		efree(MS_SQL_G(server_message));
		MS_SQL_G(server_message) = NULL;
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
*/
PHP_MINFO_FUNCTION(mssql)
{
	char buf[32];

	php_info_print_table_start();
	php_info_print_table_header(2, "MSSQL Support", "enabled");

	snprintf(buf, sizeof(buf), "%ld", MS_SQL_G(num_persistent));
	php_info_print_table_row(2, "Active Persistent Links", buf);
	snprintf(buf, sizeof(buf), "%ld", MS_SQL_G(num_links));
	php_info_print_table_row(2, "Active Links", buf);

	php_info_print_table_row(2, "Library version", MSSQL_VERSION);

	char *str_version;
	zend_resource *link_res = php_mssql_get_default_link();
	if(link_res == NULL){
		str_version = "Not connected";
	}else{
		mssql_link *mssql_ptr = (mssql_link *)zend_fetch_resource2(link_res, "MS SQL-Link", le_link, le_plink);
		long tds_ver = dbtds(mssql_ptr->link);
		switch(tds_ver){
			case DBTDS_UNKNOWN:
				str_version = "Unknown";
				break;
			case DBTDS_2_0:
			case DBTDS_3_4:
			case DBTDS_4_0:
				str_version = "4.0 or lower";
				break;
			case DBTDS_4_2:
				str_version = "4.2";
				break;
			case DBTDS_4_6:
			case DBTDS_4_9_5:
			case DBTDS_5_0:
				str_version = "Between 4.6 and 5.0";
				break;
			case DBTDS_7_0:
				str_version = "7.0";
				break;
			case DBTDS_7_1:
				str_version = "7.1 (aka 8.0)";
				break;
			case DBTDS_7_2:
				str_version = "7.2 (aka 9.0)";
				break;
			case DBTDS_7_3:
				str_version = "7.3";
				break;
			case DBTDS_7_4:
				str_version = "7.4";
				break;
			default:
				str_version = "Higher than 7.4";
		}
	}
	
	php_info_print_table_row(2, "TDS Protocol version", str_version);

	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();

}
/* }}} */

/* {{{ php_mssql_do_connect
*/
static void php_mssql_do_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
	char *host = NULL, *user = NULL, *passwd = NULL;
	size_t host_len = 0, user_len = 0, passwd_len = 0;
	zend_bool new_link = 0;
	char *hashed_details;
	int hashed_details_length;
	mssql_link mssql, *mssql_ptr;
	char buffer[40];

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|sssb", &host, &host_len, &user, &user_len, &passwd, &passwd_len, &new_link) == FAILURE) {
		return;
	}

	/* Limit strings to 255 chars to prevent overflow issues in underlying libraries */
	if(host_len>255) {
		host[255] = '\0';
	}
	if(user_len>255) {
		user[255] = '\0';
	}
	if(passwd_len>255) {
		passwd[255] = '\0';
	}

	switch(ZEND_NUM_ARGS())
	{
		case 0:
			/* defaults */
			hashed_details_length=5+3;
			hashed_details = (char *) emalloc(hashed_details_length+1);
			strcpy(hashed_details, "mssql___");
			break;
		case 1:
			hashed_details_length = spprintf(&hashed_details, 0, "mssql_%s__", host);
			break;
		case 2:
			hashed_details_length = spprintf(&hashed_details, 0, "mssql_%s_%s_", host, user);
			break;
		case 3:
		case 4:
			hashed_details_length = spprintf(&hashed_details, 0, "mssql_%s_%s_%s", host, user, passwd);
			break;
	}

	if (hashed_details == NULL) {
		php_error_docref(NULL, E_WARNING, "Out of memory");
		RETURN_FALSE;
	}

	dbsetlogintime(MS_SQL_G(connect_timeout));
	dbsettime(MS_SQL_G(timeout));

	/* set a DBLOGIN record */	
	if ((mssql.login = dblogin()) == NULL) {
		php_error_docref(NULL, E_WARNING, "Unable to allocate login record");
		RETURN_FALSE;
	}
	
	DBERRHANDLE(mssql.login, (EHANDLEFUNC) php_mssql_error_handler);
	DBMSGHANDLE(mssql.login, (MHANDLEFUNC) php_mssql_message_handler);

#ifndef HAVE_FREETDS
	if (MS_SQL_G(secure_connection)){
		DBSETLSECURE(mssql.login);
	}
	else {
#endif
		if (user) {
			DBSETLUSER(mssql.login,user);
		}
		if (passwd) {
			DBSETLPWD(mssql.login,passwd);
		}
#ifndef HAVE_FREETDS
	}
#endif

#ifdef HAVE_FREETDS
		if (MS_SQL_G(charset) && strlen(MS_SQL_G(charset))) {
			DBSETLCHARSET(mssql.login, MS_SQL_G(charset));
		}
#endif

	DBSETLAPP(mssql.login,MS_SQL_G(appname));
	mssql.valid = 1;

#ifndef HAVE_FREETDS
	DBSETLVERSION(mssql.login, DBVER60);
#endif
/*	DBSETLTIME(mssql.login, TIMEOUT_INFINITE); */

	if (!MS_SQL_G(allow_persistent)) {
		persistent=0;
	}
	/* TODO re-implement the persistent link logic in a better way? */
	{ /* non persistent */
		zend_resource *index_ptr;
		
		/* first we check the hash for the hashed_details key.  if it exists,
		 * it should point us to the right offset where the actual mssql link sits.
		 * if it doesn't, open a new mssql link, add it to the resource list,
		 * and add a pointer to it with hashed_details as the key.
		 */
		if ( !new_link && ((index_ptr = zend_hash_str_find_ptr(&EG(regular_list), hashed_details, hashed_details_length)) != NULL)) {
			zend_resource *p;

			if (index_ptr->type != le_index_ptr) {
				efree(hashed_details);
				dbfreelogin(mssql.login);
				RETURN_FALSE;
			}
			p = index_ptr->ptr;
			//TODO:better link validation
			if (p && p->ptr && (p->type==le_link || p->type==le_plink)) {
				
				GC_REFCOUNT(p)++;//Why is this needed????
				RETVAL_RES(p);

				php_mssql_set_default_link(p);
				dbfreelogin(mssql.login);
				efree(hashed_details);
				return;
			} else {
				zend_list_delete(p);
				zend_hash_str_del(&EG(regular_list), hashed_details, hashed_details_length);
			}
		}
		if (MS_SQL_G(max_links) != -1 && MS_SQL_G(num_links) >= MS_SQL_G(max_links)) {
			php_error_docref(NULL, E_WARNING, "Too many open links (%ld)", MS_SQL_G(num_links));
			efree(hashed_details);
			dbfreelogin(mssql.login);
			RETURN_FALSE;
		}
		
		if ((mssql.link=dbopen(mssql.login, host))==NULL) {
			php_error_docref(NULL, E_WARNING, "Unable to connect to server: %s", (host == NULL ? "" : host));
			efree(hashed_details);
			dbfreelogin(mssql.login);
			RETURN_FALSE;
		}

		if (DBSETOPT(mssql.link, DBBUFFER,"2")==FAIL) {
			efree(hashed_details);
			dbfreelogin(mssql.login);
			dbclose(mssql.link);
			RETURN_FALSE;
		}

#ifndef HAVE_FREETDS
		if (MS_SQL_G(textlimit) != -1) {
			snprintf(buffer, sizeof(buffer), "%li", MS_SQL_G(textlimit));
			if (DBSETOPT(mssql.link, DBTEXTLIMIT, buffer)==FAIL) {
				efree(hashed_details);
				dbfreelogin(mssql.login);
				dbclose(mssql.link);
				RETURN_FALSE;
			}
		}
#endif
		if (MS_SQL_G(textsize) != -1) {
			snprintf(buffer, sizeof(buffer), "SET TEXTSIZE %li", MS_SQL_G(textsize));
			dbcmd(mssql.link, buffer);
			dbsqlexec(mssql.link);
			dbresults(mssql.link);
		}

		/* add it to the list */
		mssql_ptr = (mssql_link *) emalloc(sizeof(mssql_link));
		memcpy(mssql_ptr, &mssql, sizeof(mssql_link));
		zend_resource *link_res;
		link_res = zend_register_resource(mssql_ptr, le_link);
		RETVAL_RES(link_res);
		
		/* add it to the hash */
		zend_resource new_index_ptr;
		new_index_ptr.ptr = (void *) link_res;
		new_index_ptr.type = le_index_ptr;
		if (zend_hash_str_update_mem(&EG(regular_list), hashed_details, hashed_details_length, (void *) &new_index_ptr, sizeof(zend_resource)) == NULL) {
			efree(hashed_details);
			RETURN_FALSE;
		}
		MS_SQL_G(num_links)++;
	}
	efree(hashed_details);
	php_mssql_set_default_link(Z_RES_P(return_value));
}
/* }}} */

/* {{{ proto int mssql_connect([string servername [, string username [, string password [, bool new_link]]]])
   Establishes a connection to a MS-SQL server */
PHP_FUNCTION(mssql_connect)
{
	php_mssql_do_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU,0);
}
/* }}} */

/* {{{ proto int mssql_pconnect([string servername [, string username [, string password [, bool new_link]]]])
   Establishes a persistent connection to a MS-SQL server */
PHP_FUNCTION(mssql_pconnect)
{
	php_mssql_do_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU,1);
}
/* }}} */

/* {{{ proto bool mssql_close([resource conn_id])
   Closes a connection to a MS-SQL server */
PHP_FUNCTION(mssql_close)
{
	//FIXME The logic originally was wrong... it kept some kind of internal reference counting...
	//the link would only be closed when every reference was closed... but you could close the same var multiple times...
	//better to just turn it into a nop and call it a day
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool mssql_select_db(string database_name [, resource conn_id])
   Select a MS-SQL database */
PHP_FUNCTION(mssql_select_db)
{
	char *db;
	zval *link_arg = NULL;
	zend_resource *link_res;
	size_t db_len;
	mssql_link  *mssql_ptr;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|r", &db, &db_len, &link_arg) == FAILURE) {
		return;
	}

	if (link_arg == NULL) {
		link_res = php_mssql_get_default_link();
		CHECK_LINK(link_res);
	} else {
		link_res = Z_RES_P(link_arg);
	}

	mssql_ptr = (mssql_link *)zend_fetch_resource2(link_res, "MS SQL-Link", le_link, le_plink);
	
	if (dbuse(mssql_ptr->link, db)==FAIL) {
		php_error_docref(NULL, E_WARNING, "Unable to select database:  %s", db);
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ php_mssql_get_column_content_with_type
*/
static void php_mssql_get_column_content_with_type(mssql_link *mssql_ptr,int offset,zval *result, int column_type  TSRMLS_DC)
{
	char *res_buf = NULL;

	if (dbdata(mssql_ptr->link,offset) == NULL && dbdatlen(mssql_ptr->link,offset) == 0) {
		ZVAL_NULL(result);
		return;
	}

	switch (column_type)
	{
		case SQLBIT:
		case SQLINT1:
		case SQLINT2:
		case SQLINT4:
		case SQLINTN: {	
			ZVAL_LONG(result, (long) anyintcol(offset));
			break;
		} 
		case SQLCHAR:
		case SQLVARCHAR:
		case SQLTEXT: {
			int length;
			char *data = charcol(offset);

			length=dbdatlen(mssql_ptr->link,offset);
#if ilia_0
			while (length>0 && data[length-1] == ' ') { /* nuke trailing whitespace */
				length--;
			}
#endif
			ZVAL_STRINGL(result, data, length); 
			break;
		}
		case SQLFLT4:
			ZVAL_DOUBLE(result, (double) floatcol4(offset));
			break;
		case SQLMONEY:
		case SQLMONEY4:
		case SQLMONEYN: {
			DBFLT8 num_buf;
			dbconvert(NULL, column_type, dbdata(mssql_ptr->link,offset), 8, SQLFLT8, (LPBYTE)&num_buf, -1);
			ZVAL_DOUBLE(result, num_buf);
			}
			break;
		case SQLFLT8:
			ZVAL_DOUBLE(result, (double) floatcol8(offset));
			break;
#ifdef SQLUNIQUE
		case SQLUNIQUE: {
#else
		case 36: {			/* FreeTDS hack */
#endif
			char *data = charcol(offset);

			/* uniqueidentifier is a 16-byte binary number */
			ZVAL_STRINGL(result, data, 16);
			}
			break;
		case SQLVARBINARY:
		case SQLBINARY:
		case SQLIMAGE: {
			int res_length = dbdatlen(mssql_ptr->link, offset);

			if (!res_length) {
				ZVAL_NULL(result);
			} else {
				ZVAL_STRINGL(result, (char *)dbdata(mssql_ptr->link, offset), res_length);
			}
		}
			break;
		case SQLNUMERIC:
		default: {
			if (dbwillconvert(column_type,SQLCHAR)) {
				DBDATEREC dateinfo;	
				int res_length = dbdatlen(mssql_ptr->link,offset);

				if (res_length == -1) {
					res_length = 255;
				}

				if ((column_type != SQLDATETIME && column_type != SQLDATETIM4) || MS_SQL_G(datetimeconvert)) {

					switch (column_type) {
						case SQLDATETIME :
						case SQLDATETIM4 :
							res_length += 20;
							break;
						case SQLMONEY :
						case SQLMONEY4 :
						case SQLMONEYN :
						case SQLDECIMAL :
						case SQLNUMERIC :
							res_length += 5;
						case 127 :
							res_length += 20;
							break;
					}

					if (!res_buf) res_buf = (char*)emalloc(res_length + 1);
					res_length = dbconvert(NULL,coltype(offset),dbdata(mssql_ptr->link,offset), res_length, SQLCHAR,res_buf,-1);
					res_buf[res_length] = '\0';
				} else {
					if (column_type == SQLDATETIM4) {
						DBDATETIME temp;

						dbconvert(NULL, SQLDATETIM4, dbdata(mssql_ptr->link,offset), -1, SQLDATETIME, (LPBYTE) &temp, -1);
						dbdatecrack(mssql_ptr->link, &dateinfo, &temp);
					} else {
						dbdatecrack(mssql_ptr->link, &dateinfo, (DBDATETIME *) dbdata(mssql_ptr->link,offset));
					}
			
					res_length = 19;
					spprintf(&res_buf, 0, "%d-%02d-%02d %02d:%02d:%02d" , dateinfo.year, dateinfo.month, dateinfo.day, dateinfo.hour, dateinfo.minute, dateinfo.second);
				}
		
				ZVAL_STRINGL(result, res_buf, res_length);
			} else {
				php_error_docref(NULL, E_WARNING, "column %d has unknown data type (%d)", offset, coltype(offset));
				ZVAL_FALSE(result);
			}
		}
	}
	if (res_buf) efree(res_buf);
}
/* }}} */

/* {{{ php_mssql_get_column_content_without_type
*/
static void php_mssql_get_column_content_without_type(mssql_link *mssql_ptr,int offset,zval *result, int column_type TSRMLS_DC)
{
	char* res_buf = NULL;
	if (dbdatlen(mssql_ptr->link,offset) == 0) {
		ZVAL_NULL(result);
		return;
	}

	if (column_type == SQLVARBINARY ||
		column_type == SQLBINARY ||
		column_type == SQLIMAGE) {
		DBBINARY *bin;
		int res_length = dbdatlen(mssql_ptr->link, offset);

		if (res_length == 0) {
			ZVAL_NULL(result);
			return;
		} else if (res_length < 0) {
			ZVAL_FALSE(result);
			return;
		}

		bin = ((DBBINARY *)dbdata(mssql_ptr->link, offset));

		ZVAL_STRINGL(result, (char*)bin, res_length);
	}
	else if  (dbwillconvert(coltype(offset),SQLCHAR)) {
		DBDATEREC dateinfo;	
		int res_length = dbdatlen(mssql_ptr->link,offset);

		if ((column_type != SQLDATETIME && column_type != SQLDATETIM4) || MS_SQL_G(datetimeconvert)) {

			switch (column_type) {
				case SQLDATETIME :
				case SQLDATETIM4 :
					res_length += 20;
					break;
				case SQLMONEY :
				case SQLMONEY4 :
				case SQLMONEYN :
				case SQLDECIMAL :
				case SQLNUMERIC :
					res_length += 5;
				case 127 :
					res_length += 20;
					break;
			}
			
			if (!res_buf) res_buf = (char *) emalloc(res_length+1);
			res_length = dbconvert(NULL,coltype(offset),dbdata(mssql_ptr->link,offset), res_length, SQLCHAR, res_buf, -1);
			res_buf[res_length] = '\0';
		} else {
			if (column_type == SQLDATETIM4) {
				DBDATETIME temp;

				dbconvert(NULL, SQLDATETIM4, dbdata(mssql_ptr->link,offset), -1, SQLDATETIME, (LPBYTE) &temp, -1);
				dbdatecrack(mssql_ptr->link, &dateinfo, &temp);
			} else {
				dbdatecrack(mssql_ptr->link, &dateinfo, (DBDATETIME *) dbdata(mssql_ptr->link,offset));
			}
			
			res_length = 19;
			spprintf(&res_buf, 0, "%d-%02d-%02d %02d:%02d:%02d" , dateinfo.year, dateinfo.month, dateinfo.day, dateinfo.hour, dateinfo.minute, dateinfo.second);
		}

		ZVAL_STRINGL(result, res_buf, res_length);
	//	ZVAL_EMPTY_STRING(result);
	//	Z_STRVAL_P(result) = res_buf;
	//	Z_STRLEN_P(result) = res_length;
	} else {
		php_error_docref(NULL, E_WARNING, "column %d has unknown data type (%d)", offset, coltype(offset));
		ZVAL_FALSE(result);
	}
	if (res_buf) efree(res_buf);
}
/* }}} */

/* {{{ _mssql_get_sp_result
*/
static void _mssql_get_sp_result(mssql_link *mssql_ptr, mssql_statement *statement TSRMLS_DC) 
{
	int i, num_rets, type;
	char *parameter;
	mssql_bind *bind;

	/* Now to fetch RETVAL and OUTPUT values*/
	num_rets = dbnumrets(mssql_ptr->link);

	if (num_rets!=0) {
		for (i = 1; i <= num_rets; i++) {
			parameter = (char*)dbretname(mssql_ptr->link, i);
			type = dbrettype(mssql_ptr->link, i);
						
			if (statement->binds != NULL) {	/*	Maybe a non-parameter sp	*/
				if (bind = (mssql_bind*)zend_hash_str_find_ptr(statement->binds, parameter, strlen(parameter))) {
					if (!dbretlen(mssql_ptr->link,i)) {
						ZVAL_NULL(bind->zval);
					}
					else {
						switch (type) {
							case SQLBIT:
							case SQLINT1:
							case SQLINT2:
							case SQLINT4:
								convert_to_long_ex(bind->zval);
								/* FIXME this works only on little endian machine !!! */
								Z_LVAL_P(bind->zval) = *((int *)(dbretdata(mssql_ptr->link,i)));
								break;
				
							case SQLFLT4:
							case SQLFLT8:
							case SQLFLTN:
							case SQLMONEY4:
							case SQLMONEY:
							case SQLMONEYN:
								convert_to_double_ex(bind->zval);
								Z_DVAL_P(bind->zval) = *((double *)(dbretdata(mssql_ptr->link,i)));
								break;
	
							case SQLCHAR:
							case SQLVARCHAR:
							case SQLTEXT:
							//	convert_to_string_ex(bind->zval);
								zval_ptr_dtor(bind->zval);
								ZVAL_STRINGL(bind->zval, dbretdata(mssql_ptr->link, i), dbretlen(mssql_ptr->link, i));
							//	Z_STRLEN_P(bind->zval) = dbretlen(mssql_ptr->link,i);
							//	Z_STRVAL_P(bind->zval) = estrndup(dbretdata(mssql_ptr->link,i),Z_STRLEN_P(bind->zval));
								break;
							/* TODO binary */
						}
					}
				}
				else {
					php_error_docref(NULL, E_WARNING, "An output parameter variable was not provided");
				}
			}
		}
	}
	if (statement->binds != NULL) {	/*	Maybe a non-parameter sp	*/
		if (bind = (mssql_bind*)zend_hash_str_find_ptr(statement->binds, "RETVAL", 6)) {
			if (dbhasretstat(mssql_ptr->link)) {
				convert_to_long_ex(bind->zval);
				Z_LVAL_P(bind->zval)=dbretstatus(mssql_ptr->link);
			}
			else {
				php_error_docref(NULL, E_WARNING, "stored procedure has no return value. Nothing was returned into RETVAL");
			}
		}
	}
}
/* }}} */

/* {{{ _mssql_fetch_batch
*/
static int _mssql_fetch_batch(mssql_link *mssql_ptr, mssql_result *result, int retvalue TSRMLS_DC) 
{
	int i, j = 0;
	char computed_buf[16];

	zend_bool old_version = (dbtds(mssql_ptr->link) < DBTDS_7_0);

	if (!result->have_fields) {
		for (i=0; i<result->num_fields; i++) {
			char *source = NULL;
			char *fname = (char *)dbcolname(mssql_ptr->link,i+1);
	
			if (*fname) {
				result->fields[i].name = estrdup(fname);
			} else {
				if (j>0) {
					snprintf(computed_buf,16,"computed%d",j);
				} else {
					strcpy(computed_buf,"computed");
				}
				result->fields[i].name = estrdup(computed_buf);
				j++;
			}
			result->fields[i].max_length = dbcollen(mssql_ptr->link,i+1);
			source = (char *)dbcolsource(mssql_ptr->link,i+1);
			if (source) {
				result->fields[i].column_source = estrdup(source);
			}
			else {
				result->fields[i].column_source = estrdup("");
			}
	
			result->fields[i].type = coltype(i+1);
			/* set numeric flag */
			switch (result->fields[i].type) {
				case SQLINT1:
				case SQLINT2:
				case SQLINT4:
				case SQLINTN:
				case SQLFLT4:
				case SQLFLT8:
				case SQLNUMERIC:
				case SQLDECIMAL:
					result->fields[i].numeric = 1;
					break;
				case SQLCHAR:
				case SQLVARCHAR:
				case SQLTEXT:
				default:
					result->fields[i].numeric = 0;
					break;
			}

			if(old_version && strlen(result->fields[i].name) == 30){
				php_error_docref(NULL, E_WARNING, "Column name '%s' possibly truncated due to TDS Protocol version.",result->fields[i].name);
			}
		}
		result->have_fields = 1;
	}

	i=0;
	if (!result->data) {
		result->data = (zval **) safe_emalloc(sizeof(zval *), MSSQL_ROWS_BLOCK*(++result->blocks_initialized), 0);
	}
	while (retvalue!=FAIL && retvalue!=NO_MORE_ROWS) {
		result->num_rows++;
		if (result->num_rows > result->blocks_initialized*MSSQL_ROWS_BLOCK) {
			result->data = (zval **) erealloc(result->data,sizeof(zval *)*MSSQL_ROWS_BLOCK*(++result->blocks_initialized));
		}
		result->data[i] = (zval *) safe_emalloc(sizeof(zval), result->num_fields, 0);
		for (j=0; j<result->num_fields; j++) {
			//INIT_ZVAL(result->data[i][j]);
			MS_SQL_G(get_column_content(mssql_ptr, j+1, &result->data[i][j], result->fields[j].type TSRMLS_CC));
			zval z = result->data[i][j];
			int z_len = Z_TYPE(z) == IS_STRING ? Z_STRLEN(z) : 0;
			if(old_version && (result->fields[j].type == SQLCHAR || result->fields[j].type == SQLVARCHAR) && z_len == 255){
				php_error_docref(NULL, E_WARNING, "Value on column '%s' possibly truncated due to TDS Protocol version.",result->fields[j].name);
			}
		}
		if (i<result->batchsize || result->batchsize==0) {
			i++;
			dbclrbuf(mssql_ptr->link,DBLASTROW(mssql_ptr->link)); 
			retvalue=dbnextrow(mssql_ptr->link);
		}
		else
			break;
		result->lastresult = retvalue;
	}
	if (result->statement && (retvalue == NO_MORE_RESULTS || retvalue == NO_MORE_RPC_RESULTS)) {
		_mssql_get_sp_result(mssql_ptr, result->statement TSRMLS_CC);
	}
	return i;
}
/* }}} */

/* {{{ proto int mssql_fetch_batch(resource result_index)
   Returns the next batch of records */
PHP_FUNCTION(mssql_fetch_batch)
{
	zval *result_arg = NULL;
	mssql_result *result;
	mssql_link *mssql_ptr;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &result_arg) == FAILURE) {
		return;
	}
	
	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	mssql_ptr = result->mssql_ptr;
	_free_result(result, 0);
	result->cur_row=result->num_rows=0;
	result->num_rows = _mssql_fetch_batch(mssql_ptr, result, result->lastresult TSRMLS_CC);

	RETURN_LONG(result->num_rows);
}
/* }}} */

/* {{{ proto resource mssql_query(string query [, resource conn_id [, int batch_size]])
   Perform an SQL query on a MS-SQL server database */
PHP_FUNCTION(mssql_query)
{
	char *query;
	zval *link_arg = NULL;
	zend_resource *link_res;
	size_t query_len;
	int retvalue, batchsize, num_fields;
	zend_long zbatchsize = 0;
	mssql_link *mssql_ptr;
	mssql_result *result;

	dbsettime(MS_SQL_G(timeout));
	batchsize = MS_SQL_G(batchsize);

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|rl", &query, &query_len, &link_arg, &zbatchsize) == FAILURE) {
		return;
	}

	if (NULL == link_arg) {
		link_res = php_mssql_get_default_link();
		CHECK_LINK(link_res);
	} else {
		link_res = Z_RES_P(link_arg);
	}

	if (ZEND_NUM_ARGS() == 3) {
		batchsize = (int) zbatchsize;
	}

	mssql_ptr = (mssql_link *)zend_fetch_resource2(link_res, "MS SQL-Link", le_link, le_plink);

	if (dbcmd(mssql_ptr->link, query)==FAIL) {
		php_error_docref(NULL, E_WARNING, "Unable to set query");
		RETURN_FALSE;
	}
	if (dbsqlexec(mssql_ptr->link)==FAIL || (retvalue = dbresults(mssql_ptr->link))==FAIL) {
		php_error_docref(NULL, E_WARNING, "Query failed");
		dbcancel(mssql_ptr->link);
		RETURN_FALSE;
	}
	
	/* Skip results not returning any columns */
	while ((num_fields = dbnumcols(mssql_ptr->link)) <= 0 && retvalue == SUCCEED) {
		retvalue = dbresults(mssql_ptr->link);
	}

	if (num_fields <= 0) {
		RETURN_TRUE;
	}

	retvalue=dbnextrow(mssql_ptr->link);	
	if (retvalue==FAIL) {
		dbcancel(mssql_ptr->link);
		RETURN_FALSE;
	}

	result = (mssql_result *) emalloc(sizeof(mssql_result));
	result->statement = NULL;
	result->num_fields = num_fields;
	result->blocks_initialized = 1;
	
	result->batchsize = batchsize;
	result->data = NULL;
	result->blocks_initialized = 0;
	result->mssql_ptr = mssql_ptr;
	result->cur_field=result->cur_row=result->num_rows=0;
	result->have_fields = 0;

	result->fields = (mssql_field *) safe_emalloc(sizeof(mssql_field), result->num_fields, 0);
	result->num_rows = _mssql_fetch_batch(mssql_ptr, result, retvalue TSRMLS_CC);
	
	RETURN_RES(zend_register_resource(result, le_result));
}
/* }}} */

/* {{{ proto int mssql_rows_affected(resource conn_id)
   Returns the number of records affected by the query */
PHP_FUNCTION(mssql_rows_affected)
{
	zval *link_arg = NULL;
	mssql_link *mssql_ptr;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &link_arg) == FAILURE) {
		return;
	}

	mssql_ptr = (mssql_link *)zend_fetch_resource2(Z_RES_P(link_arg), "MS SQL-Link", le_link, le_plink);

	RETURN_LONG(DBCOUNT(mssql_ptr->link));
}
/* }}} */

/* {{{ proto bool mssql_free_result(resource result_index)
   Free a MS-SQL result index */
PHP_FUNCTION(mssql_free_result)
{
	zval *result_arg = NULL;
	mssql_result *result;
	int retvalue;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &result_arg) == FAILURE) {
		return;
	}
	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	/* Release remaining results */
	do {
		dbcanquery(result->mssql_ptr->link);
		retvalue = dbresults(result->mssql_ptr->link);
	} while (retvalue == SUCCEED);

	zend_list_close(Z_RES_P(result_arg));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string mssql_get_last_message(void)
   Gets the last message from the MS-SQL server */
PHP_FUNCTION(mssql_get_last_message)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (MS_SQL_G(server_message)) {
		RETURN_STRING(MS_SQL_G(server_message));
	} else {
		RETURN_STRING("");
	}
}
/* }}} */

/* {{{ proto int mssql_num_rows(resource mssql_result_index)
   Returns the number of rows fetched in from the result id specified */
PHP_FUNCTION(mssql_num_rows)
{
	zval *result_arg = NULL;
	mssql_result *result;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &result_arg) == FAILURE) {
		return;
	}

	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_LONG(0);
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	RETURN_LONG(result->num_rows);
}
/* }}} */

/* {{{ proto int mssql_num_fields(resource mssql_result_index)
   Returns the number of fields fetched in from the result id specified */
PHP_FUNCTION(mssql_num_fields)
{
	zval *result_arg = NULL;
	mssql_result *result;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &result_arg) == FAILURE) {
		return;
	}

	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_LONG(0);
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	RETURN_LONG(result->num_fields);
}
/* }}} */

/* {{{ php_mssql_fetch_hash
*/
static void php_mssql_fetch_hash(INTERNAL_FUNCTION_PARAMETERS, int result_type)
{
	zval *result_arg = NULL;
	mssql_result *result;
	int i;
	zend_long resulttype = 0;

	switch (result_type) {
		case MSSQL_NUM:
		case MSSQL_ASSOC:
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &result_arg) == FAILURE) {
				return;
			}
			break;
		case MSSQL_BOTH:
			if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &result_arg, &resulttype) == FAILURE) {
				return;
			}
			result_type = (resulttype > 0 && (resulttype & MSSQL_BOTH)) ? resulttype : result_type;
			break;
		default:
			return;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	if (MS_SQL_G(server_message)) {
		efree(MS_SQL_G(server_message));
		MS_SQL_G(server_message) = NULL;
	}

	if (result->cur_row >= result->num_rows) {
		RETURN_FALSE;
	}
	
	array_init(return_value);
	
	for (i=0; i<result->num_fields; i++) {
		if (Z_TYPE(result->data[result->cur_row][i]) != IS_NULL) {
			if (Z_TYPE(result->data[result->cur_row][i]) == IS_STRING) {
				zval data;

				if (result_type & MSSQL_NUM) {
					ZVAL_COPY(&data,&(result->data[result->cur_row][i]));
					add_index_zval(return_value, i, &data);
				}
				
				if (result_type & MSSQL_ASSOC) {
					ZVAL_COPY(&data,&(result->data[result->cur_row][i]));
					add_assoc_zval(return_value, result->fields[i].name, &data);
				}
			}
			else if (Z_TYPE(result->data[result->cur_row][i]) == IS_LONG) {
				if (result_type & MSSQL_NUM)
					add_index_long(return_value, i, Z_LVAL(result->data[result->cur_row][i]));
				
				if (result_type & MSSQL_ASSOC)
					add_assoc_long(return_value, result->fields[i].name, Z_LVAL(result->data[result->cur_row][i]));
			}
			else if (Z_TYPE(result->data[result->cur_row][i]) == IS_DOUBLE) {
				if (result_type & MSSQL_NUM)
					add_index_double(return_value, i, Z_DVAL(result->data[result->cur_row][i]));
				
				if (result_type & MSSQL_ASSOC)
					add_assoc_double(return_value, result->fields[i].name, Z_DVAL(result->data[result->cur_row][i]));
			}
		}
		else
		{
			if (result_type & MSSQL_NUM)
				add_index_null(return_value, i);
			if (result_type & MSSQL_ASSOC)
				add_assoc_null(return_value, result->fields[i].name);
		}
	}
	result->cur_row++;
}
/* }}} */

/* {{{ proto array mssql_fetch_row(resource result_id)
   Returns an array of the current row in the result set specified by result_id */
PHP_FUNCTION(mssql_fetch_row)
{
	php_mssql_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, MSSQL_NUM);
}
/* }}} */

/* {{{ proto object mssql_fetch_object(resource result_id)
   Returns a pseudo-object of the current row in the result set specified by result_id */
PHP_FUNCTION(mssql_fetch_object)
{
	php_mssql_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, MSSQL_ASSOC);
	if (Z_TYPE_P(return_value)==IS_ARRAY) {
		object_and_properties_init(return_value, ZEND_STANDARD_CLASS_DEF_PTR, Z_ARRVAL_P(return_value));
	}
}
/* }}} */

/* {{{ proto array mssql_fetch_array(resource result_id [, int result_type])
   Returns an associative array of the current row in the result set specified by result_id */
PHP_FUNCTION(mssql_fetch_array)
{
	php_mssql_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, MSSQL_BOTH);
}
/* }}} */

/* {{{ proto array mssql_fetch_assoc(resource result_id)
   Returns an associative array of the current row in the result set specified by result_id */
PHP_FUNCTION(mssql_fetch_assoc)
{
	php_mssql_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, MSSQL_ASSOC);
}
/* }}} */

/* {{{ proto bool mssql_data_seek(resource result_id, int offset)
   Moves the internal row pointer of the MS-SQL result associated with the specified result identifier to pointer to the specified row number */
PHP_FUNCTION(mssql_data_seek)
{
	zval *result_arg = NULL;
	zend_long offset;
	mssql_result *result;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &result_arg, &offset) == FAILURE) {
		return;
	}

	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		return;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	if (offset < 0 || offset >= result->num_rows) {
		php_error_docref(NULL, E_WARNING, "Bad row offset");
		RETURN_FALSE;
	}
	
	result->cur_row = offset;
	RETURN_TRUE;
}
/* }}} */

/* {{{ php_mssql_get_field_name
*/
static char *php_mssql_get_field_name(int type)
{
	switch (type) {
		case SQLBINARY:
		case SQLVARBINARY:
			return "blob";
			break;
		case SQLCHAR:
		case SQLVARCHAR:
			return "char";
			break;
		case SQLTEXT:
			return "text";
			break;
		case SQLDATETIME:
		case SQLDATETIM4:
		case SQLDATETIMN:
			return "datetime";
			break;
		case SQLDECIMAL:
		case SQLFLT4:
		case SQLFLT8:
		case SQLFLTN:
			return "real";
			break;
		case SQLINT1:
		case SQLINT2:
		case SQLINT4:
		case SQLINTN:
			return "int";
			break;
		case SQLNUMERIC:
			return "numeric";
			break;
		case SQLMONEY:
		case SQLMONEY4:
		case SQLMONEYN:
			return "money";
			break;
		case SQLBIT:
			return "bit";
			break;
		case SQLIMAGE:
			return "image";
			break;
#ifdef SQLUNIQUE
		case SQLUNIQUE:
			return "uniqueidentifier";
			break;
#endif
		default:
			return "unknown";
			break;
	}
}
/* }}} */

/* {{{ proto object mssql_fetch_field(resource result_id [, int offset])
   Gets information about certain fields in a query result */
PHP_FUNCTION(mssql_fetch_field)
{
	zval *result_arg = NULL;
	zend_long field_offset = -1;
	mssql_result *result;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &result_arg, &field_offset) == FAILURE) {
		return;
	}
	
	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);
	
	if (field_offset==-1) {
		field_offset = result->cur_field;
		result->cur_field++;
	}
	
	if (field_offset<0 || field_offset >= result->num_fields) {
		if (ZEND_NUM_ARGS()==2) { /* field specified explicitly */
			php_error_docref(NULL, E_WARNING, "Bad column offset");
		}
		RETURN_FALSE;
	}

	object_init(return_value);

	add_property_string(return_value, "name",result->fields[field_offset].name);
	add_property_long(return_value, "max_length",result->fields[field_offset].max_length);
	add_property_string(return_value, "column_source",result->fields[field_offset].column_source);
	add_property_long(return_value, "numeric", result->fields[field_offset].numeric);
	add_property_string(return_value, "type", php_mssql_get_field_name(result->fields[field_offset].type));
}
/* }}} */

/* {{{ proto int mssql_field_length(resource result_id [, int offset])
   Get the length of a MS-SQL field */
PHP_FUNCTION(mssql_field_length)
{
	zval *result_arg = NULL;
	zend_long field_offset = -1;
	mssql_result *result;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &result_arg, &field_offset) == FAILURE) {
		return;
	}
	
	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	if (field_offset==-1) {
		field_offset = result->cur_field;
		result->cur_field++;
	}
	
	if (field_offset<0 || field_offset >= result->num_fields) {
		if (ZEND_NUM_ARGS()==2) { /* field specified explicitly */
			php_error_docref(NULL, E_WARNING, "Bad column offset");
		}
		RETURN_FALSE;
	}

	RETURN_LONG(result->fields[field_offset].max_length);
}
/* }}} */

/* {{{ proto string mssql_field_name(resource result_id [, int offset])
   Returns the name of the field given by offset in the result set given by result_id */
PHP_FUNCTION(mssql_field_name)
{
	zval *result_arg = NULL;
	zend_long field_offset = -1;
	mssql_result *result;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &result_arg, &field_offset) == FAILURE) {
		return;
	}
	
	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);
	
	if (field_offset==-1) {
		field_offset = result->cur_field;
		result->cur_field++;
	}
	
	if (field_offset<0 || field_offset >= result->num_fields) {
		if (ZEND_NUM_ARGS()==2) { /* field specified explicitly */
			php_error_docref(NULL, E_WARNING, "Bad column offset");
		}
		RETURN_FALSE;
	}

	RETURN_STRINGL(result->fields[field_offset].name, strlen(result->fields[field_offset].name));
}
/* }}} */

/* {{{ proto string mssql_field_type(resource result_id [, int offset])
   Returns the type of a field */
PHP_FUNCTION(mssql_field_type)
{
	zval *result_arg = NULL;
	zend_long field_offset = -1;
	mssql_result *result;
	char* type;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &result_arg, &field_offset) == FAILURE) {
		return;
	}

	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);
	
	if (field_offset==-1) {
		field_offset = result->cur_field;
		result->cur_field++;
	}
	
	if (field_offset<0 || field_offset >= result->num_fields) {
		if (ZEND_NUM_ARGS()==2) { /* field specified explicitly */
			php_error_docref(NULL, E_WARNING, "Bad column offset");
		}
		RETURN_FALSE;
	}

	type = php_mssql_get_field_name(result->fields[field_offset].type);
	RETURN_STRINGL(type, strlen(type));
}
/* }}} */

/* {{{ proto bool mssql_field_seek(resource result_id, int offset)
   Seeks to the specified field offset */
PHP_FUNCTION(mssql_field_seek)
{
	zval *result_arg = NULL;
	zend_long field_offset;
	mssql_result *result;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &result_arg, &field_offset) == FAILURE) {
		return;
	}
	
	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);
	
	if (field_offset<0 || field_offset >= result->num_fields) {
		php_error_docref(NULL, E_WARNING, "Bad column offset");
		RETURN_FALSE;
	}

	result->cur_field = field_offset;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string mssql_result(resource result_id, int row, mixed field)
   Returns the contents of one cell from a MS-SQL result set */
PHP_FUNCTION(mssql_result)
{
	zval *field, *result_arg = NULL;
	zend_long row;
	int field_offset=0;
	mssql_result *result;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rlz", &result_arg, &row, &field) == FAILURE) {
		return;
	}

	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	if (row < 0 || row >= result->num_rows) {
		php_error_docref(NULL, E_WARNING, "Bad row offset (%ld)", row);
		RETURN_FALSE;
	}

	switch(Z_TYPE_P(field)) {
		case IS_STRING: {
			int i;

			for (i=0; i<result->num_fields; i++) {
				if (!strcasecmp(result->fields[i].name, Z_STRVAL_P(field))) {
					field_offset = i;
					break;
				}
			}
			if (i>=result->num_fields) { /* no match found */
				php_error_docref(NULL, E_WARNING, "%s field not found in result", Z_STRVAL_P(field));
				RETURN_FALSE;
			}
			break;
		}
		default:
			convert_to_long_ex(field);
			field_offset = Z_LVAL_P(field);
			if (field_offset<0 || field_offset>=result->num_fields) {
				php_error_docref(NULL, E_WARNING, "Bad column offset specified");
				RETURN_FALSE;
			}
			break;
	}

	*return_value = result->data[row][field_offset];
	zval_copy_ctor(return_value);
}
/* }}} */

/* {{{ proto bool mssql_next_result(resource result_id)
   Move the internal result pointer to the next result */
PHP_FUNCTION(mssql_next_result)
{
	zval *result_arg = NULL;
	int retvalue;
	mssql_result *result;
	mssql_link *mssql_ptr;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &result_arg) == FAILURE) {
		return;
	}

	if ((NULL == result_arg) || (NULL == Z_RES_P(result_arg))) {
		RETURN_FALSE;
	}

	result = (mssql_result *)zend_fetch_resource(Z_RES_P(result_arg), "MS SQL-result", le_result);
	CHECK_RES(result);

	mssql_ptr = result->mssql_ptr;
	retvalue = dbresults(mssql_ptr->link);

	while (dbnumcols(mssql_ptr->link) <= 0 && retvalue == SUCCEED) {
		retvalue = dbresults(mssql_ptr->link);
	}

	if (retvalue == FAIL) {
		RETURN_FALSE;
	}
	else if (retvalue == NO_MORE_RESULTS || retvalue == NO_MORE_RPC_RESULTS) {
		if (result->statement) {
			_mssql_get_sp_result(mssql_ptr, result->statement TSRMLS_CC);
		}
		RETURN_FALSE;
	}
	else {
		_free_result(result, 1);
		result->cur_row=result->num_fields=result->num_rows=0;
		dbclrbuf(mssql_ptr->link,DBLASTROW(mssql_ptr->link));
		retvalue = dbnextrow(mssql_ptr->link);

		result->num_fields = dbnumcols(mssql_ptr->link);
		result->fields = (mssql_field *) safe_emalloc(sizeof(mssql_field), result->num_fields, 0);
		result->have_fields = 0;
		result->num_rows = _mssql_fetch_batch(mssql_ptr, result, retvalue TSRMLS_CC);
		RETURN_TRUE;
	}

}
/* }}} */


/* {{{ proto void mssql_min_error_severity(int severity)
   Sets the lower error severity */
PHP_FUNCTION(mssql_min_error_severity)
{
	zend_long severity;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &severity) == FAILURE) {
		return;
	}

	MS_SQL_G(min_error_severity) = severity;
}

/* }}} */

/* {{{ proto void mssql_min_message_severity(int severity)
   Sets the lower message severity */
PHP_FUNCTION(mssql_min_message_severity)
{
	zend_long severity;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &severity) == FAILURE) {
		return;
	}

	MS_SQL_G(min_message_severity) = severity;
}
/* }}} */

/* {{{ proto int mssql_init(string sp_name [, resource conn_id])
   Initializes a stored procedure or a remote stored procedure  */
PHP_FUNCTION(mssql_init)
{
	char *sp_name;
	size_t sp_name_len;
	zval *link_arg = NULL;
	zend_resource *link_res;
	mssql_link *mssql_ptr;
	mssql_statement *statement;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|r", &sp_name, &sp_name_len, &link_arg) == FAILURE) {
		return;
	}

	if (link_arg == NULL) {
		link_res = php_mssql_get_default_link();
		CHECK_LINK(link_res);
	} else {
		link_res = Z_RES_P(link_arg);
	}

	mssql_ptr = (mssql_link *)zend_fetch_resource2(link_res, "MS SQL-Link", le_link, le_plink);
	
	if (dbrpcinit(mssql_ptr->link, sp_name,0)==FAIL) {
		php_error_docref(NULL, E_WARNING, "unable to init stored procedure");
		RETURN_FALSE;
	}

	statement=NULL;
	statement = ecalloc(1,sizeof(mssql_statement));
	statement->link = mssql_ptr;
	statement->executed=FALSE;

	statement->id = 0;
	RETURN_RES(zend_register_resource(statement, le_statement));
	//zend_list_insert(statement,le_statement TSRMLS_CC);
	
	//RETURN_RESOURCE(statement->id);
}
/* }}} */

/* {{{ proto bool mssql_bind(resource stmt, string param_name, mixed var, int type [, bool is_output [, bool is_null [, int maxlen]]])
   Adds a parameter to a stored procedure or a remote stored procedure  */
PHP_FUNCTION(mssql_bind)
{
	char *param_name;
	size_t param_name_len;
	int datalen;
	int status = 0;
	zend_long type = 0, maxlen = -1;
	zval *stmt, *var;
	zend_bool is_output = 0, is_null = 0;
	mssql_link *mssql_ptr;
	mssql_statement *statement;
	mssql_bind bind,*bindp;
	LPBYTE value = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rsz/l|bbl", &stmt, &param_name, &param_name_len, &var, &type, &is_output, &is_null, &maxlen) == FAILURE) {
		return;
	}

	if (ZEND_NUM_ARGS() == 7 && !is_output) {
		maxlen = -1;
	}
	
	statement = (mssql_statement *)zend_fetch_resource(Z_RES_P(stmt), "MS SQL-Statement", le_statement);
	CHECK_RES(statement);
	
	mssql_ptr=statement->link;

	/* modify datalen and maxlen according to dbrpcparam documentation */
	if ( (type==SQLVARCHAR) || (type==SQLCHAR) || (type==SQLTEXT) )	{	/* variable-length type */
		if (is_null) {
			maxlen=0;
			datalen=0;
		} else {
			convert_to_string_ex(var);
			datalen = Z_STRLEN_P(var);
			value = (LPBYTE)Z_STRVAL_P(var);
		}
	} else {
		/* fixed-length type */
		if (is_null)	{
			datalen=0;
		} else {
			datalen=-1;
		}
		maxlen=-1;

		switch (type) {
			case SQLFLT4:
			case SQLFLT8:
			case SQLFLTN:
				convert_to_double_ex(var);
				value = (LPBYTE)(&Z_DVAL_P(var));
				break;

			case SQLBIT:
			case SQLINT1:
			case SQLINT2:
			case SQLINT4:
				convert_to_long_ex(var);
				value = (LPBYTE)(&Z_LVAL_P(var));
				break;

			default:
				php_error_docref(NULL, E_WARNING, "unsupported type");
				RETURN_FALSE;
				break;
		}
	}
	
	if (is_output) {
		status=DBRPCRETURN;
	}
	
	/* hashtable of binds */
	if (! statement->binds) {
		ALLOC_HASHTABLE(statement->binds);
		zend_hash_init(statement->binds, 13, NULL, _mssql_bind_hash_dtor, 0);
	}

	if (zend_hash_str_exists(statement->binds, param_name, param_name_len)) {
		RETURN_FALSE;
	}
	else {
		memset((void*)&bind,0,sizeof(mssql_bind));
		bindp = zend_hash_str_add_mem(statement->binds, param_name, param_name_len, &bind, sizeof(bind));
		if( NULL == bindp ) RETURN_FALSE;
		bindp->zval=var;
		zval_add_ref(var);
	
		/* no call to dbrpcparam if RETVAL */
		if ( strcmp("RETVAL", param_name)!=0 ) {						
			if (dbrpcparam(mssql_ptr->link, param_name, (BYTE)status, type, maxlen, datalen, (LPBYTE)value)==FAIL) {
				php_error_docref(NULL, E_WARNING, "Unable to set parameter");
				RETURN_FALSE;
			}
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto mixed mssql_execute(resource stmt [, bool skip_results = false])
   Executes a stored procedure on a MS-SQL server database */
PHP_FUNCTION(mssql_execute)
{
	zval *stmt;
	zend_bool skip_results = 0;
	int retvalue, retval_results;
	mssql_link *mssql_ptr;
	mssql_statement *statement;
	mssql_result *result;
	int num_fields;
	int batchsize;
	int exec_retval;

	batchsize = MS_SQL_G(batchsize);

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|b", &stmt, &skip_results) == FAILURE) {
		return;
	}

	statement = (mssql_statement *)zend_fetch_resource(Z_RES_P(stmt), "MS SQL-Statement", le_statement);
	CHECK_RES(statement);

	mssql_ptr=statement->link;
	exec_retval = dbrpcexec(mssql_ptr->link);

	if (exec_retval == FAIL || dbsqlok(mssql_ptr->link) == FAIL) {
		php_error_docref(NULL, E_WARNING, "stored procedure execution failed");

		if (exec_retval == FAIL) {
			dbcancel(mssql_ptr->link);
		}

		RETURN_FALSE;
	}

	retval_results=dbresults(mssql_ptr->link);

	if (retval_results==FAIL) {
		php_error_docref(NULL, E_WARNING, "could not retrieve results");
		dbcancel(mssql_ptr->link);
		RETURN_FALSE;
	}

	/* The following is just like mssql_query, fetch all rows from the first result 
	 *	set into the row buffer. 
 	 */
	result=NULL;
	if (retval_results == SUCCEED) {
		if (skip_results) {
			do {
				dbcanquery(mssql_ptr->link);
				retval_results = dbresults(mssql_ptr->link);
			} while (retval_results == SUCCEED);
		}
		else {
			/* Skip results not returning any columns */
			while ((num_fields = dbnumcols(mssql_ptr->link)) <= 0 && retval_results == SUCCEED) {
				retval_results = dbresults(mssql_ptr->link);
			}
			if ((num_fields = dbnumcols(mssql_ptr->link)) > 0) {
				retvalue = dbnextrow(mssql_ptr->link);
				result = (mssql_result *) emalloc(sizeof(mssql_result));
				result->batchsize = batchsize;
				result->blocks_initialized = 1;
				result->data = (zval **) safe_emalloc(sizeof(zval *), MSSQL_ROWS_BLOCK, 0);
				result->mssql_ptr = mssql_ptr;
				result->cur_field=result->cur_row=result->num_rows=0;
				result->num_fields = num_fields;
				result->have_fields = 0;

				result->fields = (mssql_field *) safe_emalloc(sizeof(mssql_field), num_fields, 0);
				result->statement = statement;
				result->num_rows = _mssql_fetch_batch(mssql_ptr, result, retvalue TSRMLS_CC);
			}
		}
	}
	if (retval_results == NO_MORE_RESULTS || retval_results == NO_MORE_RPC_RESULTS) {
		_mssql_get_sp_result(mssql_ptr, statement TSRMLS_CC);
	}
	
	if (result==NULL) {
		RETURN_TRUE;	/* no recordset returned ...*/
	}
	else {
		RETURN_RES(zend_register_resource(result, le_result));
	}
}
/* }}} */

/* {{{ proto bool mssql_free_statement(resource result_index)
   Free a MS-SQL statement index */
PHP_FUNCTION(mssql_free_statement)
{
	zval *stmt;
	mssql_statement *statement;
	int retvalue;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &stmt) == FAILURE) {
		return;
	}

	if ((NULL == stmt) || (NULL == Z_RES_P(stmt))) {
		RETURN_FALSE;
	}
	
	statement = (mssql_statement *)zend_fetch_resource(Z_RES_P(stmt), "MS SQL-Statement", le_statement);
	CHECK_RES(statement);

	/* Release remaining results */
	do {
		dbcanquery(statement->link->link);
		retvalue = dbresults(statement->link->link);
	} while (retvalue == SUCCEED);

	zend_list_close(Z_RES_P(stmt));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string mssql_guid_string(string binary [,bool short_format])
   Converts a 16 byte binary GUID to a string  */
PHP_FUNCTION(mssql_guid_string)
{
	char *binary;
	size_t binary_len;
	zend_bool sf = 0;
	char buffer[32+1] = { 0 };
	char buffer2[36+1] = { 0 };

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|b", &binary, &binary_len, &sf) == FAILURE) {
		return;
	}

	dbconvert(NULL, SQLBINARY, (BYTE*) binary, MIN(16, binary_len), SQLCHAR, buffer, -1);

	if (sf) {
		php_strtoupper(buffer, 32);
		RETURN_STRING(buffer);
	}
	else {
		int i;
		/* FIXME this works only on little endian machine */
		for (i=0; i<4; i++) {
			buffer2[2*i] = buffer[6-2*i];
			buffer2[2*i+1] = buffer[7-2*i];
		}
		buffer2[8] = '-';
		for (i=0; i<2; i++) {
			buffer2[9+2*i] = buffer[10-2*i];
			buffer2[10+2*i] = buffer[11-2*i];
		}
		buffer2[13] = '-';
		for (i=0; i<2; i++) {
			buffer2[14+2*i] = buffer[14-2*i];
			buffer2[15+2*i] = buffer[15-2*i];
		}
		buffer2[18] = '-';
		for (i=0; i<4; i++) {
			buffer2[19+i] = buffer[16+i];
		}
		buffer2[23] = '-';
		for (i=0; i<12; i++) {
			buffer2[24+i] = buffer[20+i];
		}
		buffer2[36] = 0;

		php_strtoupper(buffer2, 36);
		RETURN_STRING(buffer2);
	}
}
/* }}} */

#endif
