/**
 * @author ESTEVE Olivier
 * @copyright WTFPL 2.0
 *
 * @brief An idl parser who support lot of OMG IDL standard.
 * base code is extracted from my own "OO" interpreted language (which was used for my Phd thesis)
 * (c) ESTEVE Olivier - 2004.
 * 
 *  Supported preprocessor Directives :
 * ------------------------------------
 *	the parser supports the standard preprocessor directives, such as 
 *  #ifdef, #elif, #else, #endif, #include, #undef, and #define. 
 *  #if is not supported right now.
 * 
 * Types :
 * -------
 * Table 7-6: All IDL keywords > https://www.omg.org/spec/IDL/4.2/PDF
 * https://fast-dds.docs.eprosima.com/en/latest/fastddsgen/dataTypes/dataTypes.html
 * 
 * + Allowed type handled by the parser :
 * --------------------------------------
 * int8_t, int16_t, int32_t / int, int64_t
 * uint8_t, uint16_t, uint32_t, uint64_t
 * 
 * todo(s):
 * --------
 * handle all known type from the standard, eprosima, rti, ...
 * field/variable name must not contain any "type name" >> error
 * array
 * map
 * bitset
 * bitmask
 * 
 * unsupported:
 * ------------
 * In-line nested types are not supported.
 * prama #if is not supported right now.
 * array >> ie.: "char a[10];" (on the todo list)
 * 
 * Note on namespace (Module) :
 * ------------------------------
 * There is a limited support for namespace.
 * 
 * ref(s) :
 * https://www.omg.org/spec/IDL/4.2/PDF
 * https://fast-dds.docs.eprosima.com/en/latest/fastddsgen/dataTypes/dataTypes.html
 * https://d2vkrkwbbxbylk.cloudfront.net/rti-doc/510/ndds.5.1.0/doc/pdf/RTI_CoreLibrariesAndUtilities_UsersManual.pdf
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <iostream>     // std::cout, std::ios
#include <sstream>      // std::stringstream
#include <assert.h>

#include <vector>
#include <map>
#include "common.h"

#include "str.h"

#define Map std::map

#define N_VECTOR(type_) typedef Vector<type_> type_##_v;


/** 
 * pos/index must equal pos/index in structure : __internal_hash 
 */
enum TypeAndCommande_e {
	TYPE_START = 0,
	ID_VOID = TYPE_START,
	ID_OCTET,
	ID_INT8,
	ID_INT16,
	ID_SHORT,
	ID_INT32,
	ID_INT,
	ID_LONG,
	ID_INT64,
	ID_LONGLONG,
	ID_UINT8,
	ID_UINT16,
	ID_UINT32,
	ID_UINT64,
	ID_BOOL,
	ID_BOOLEAN,
	ID_CHAR,
	ID_FLOAT,
	ID_STRING,
	ID_DOUBLE,
	ID_SEQUENCE,
	ID_CONST,

	LAST_TYPE,

	// built-in base
	BASE_START = LAST_TYPE,
	ID_STRUCT = BASE_START,
	ID_MODULE,
	ID_TYPEDEF,
	LAST_BASE,

	TYPE_SPACER		= 1024,	// up to 1024 type before overflow
	BASE_SPACER		= 4096,	// up to 4096 base command before overflow
	
	USER_BASE_SPACER_TYPEDEF= 8192,	// up to 8192 typedef
	USER_BASE_SPACER_STRUCT= 16384	// up to 16384 struct
};

struct Enum_t
{
	Enum_t() : hash(0), type(0), name(), nameSpace(), body() {}
	hash_t hash; // hash(name)
	int type;		// check built-in base
	String name;
	String nameSpace;
	String body;
}; // Enum_t 
N_VECTOR(Enum_t)
struct Typedef_t
{
	Typedef_t() : hash(0), type(0), name(), baseName(), nameSpace(), size(-1) {}
	hash_t hash; 		// hash(name)
	int type;			// base type
	String name;		// new type name
	String baseName;	// base type name
	String nameSpace;
	int size; /** if type == SEQ > sequence with defined size, else 0 */
	/** ie.:
	 * typedef char T_Char
	 *         ^    ^
	 *         |    new type name(string)
	 *         °------------ base type(int) + base type name(string)
	 */
}; // Typedef_t 
N_VECTOR(Typedef_t)
struct Module_t
{
	Module_t() : hash(0), type(0), name(), body() {}
	hash_t hash; // hash(name)
	int type;		// check built-in base
	String name;
	String body;
}; // Module_t 
N_VECTOR(Module_t)
struct Variable_t
{
	Variable_t() : hash(0), type(), is_key(false), name(), struct_name(),
		fromNamespace() {}
	hash_t hash;	// hash(name)
	Typedef_t type;
	bool is_key;
	String name;
	String struct_name;
	String fromNamespace; /** in case the type is from another namespace */
}; // Variable_t
N_VECTOR(Variable_t)
struct Struct_t
{
	Struct_t() : hash(0), type(0), name(), nameSpace(), fields() {}
	hash_t hash; // hash(name)
	int type;		// check built-in base
	String name;
	String nameSpace;
	Variable_t_v fields; /** fields / champs */
}; // Struct_t 
N_VECTOR(Struct_t)
struct UDefine_t
{
	UDefine_t() : line() {}
	String line;
}; // UDefine_t
N_VECTOR(UDefine_t)

// -------------------------------------------------------------------------
// base class for idl parser
class Parser
{
public:
	Parser() :
		enums(),
		typedefs(),
		structs(),
		variables(),
		udefines(),
		modules(),
		nameSpace()
	{	}
	virtual ~Parser() {
		clear();
	}

	// -------------------------------------------------------------------------
	int expect_symbol(const char *src, char symbol);
	// -------------------------------------------------------------------------
	int read_block(const char *src, char *dest, int size, char from, char to);
	// -------------------------------------------------------------------------
	int read_token(const char *src, char *dest, int size, const char *symbols = NULL);
	// -------------------------------------------------------------------------
	static int read_name(const char *src, char *dest, int size);
	// -------------------------------------------------------------------------
	int get_symbol(const char *src, const char *symbols = NULL);
	// -------------------------------------------------------------------------
	static int skip_spaces(const char *src);
	// -------------------------------------------------------------------------
	static int is_builtin_type(const hash_t& hash, int& result);
	// -------------------------------------------------------------------------
	static int is_builtin_base(const hash_t& hash, int& result);
	// -------------------------------------------------------------------------
	int is_enum(const hash_t& hash, int& result);
	// -------------------------------------------------------------------------
	int is_struct(const hash_t& hash, int& result);
	// -------------------------------------------------------------------------
	int is_typedef(const hash_t& hash, int& result);
	// -------------------------------------------------------------------------
	// check on known type from user (struct for now)
	int is_user_base(const hash_t& hash, int& result);
	// -------------------------------------------------------------------------
	int read_digit(const char *src, char *dest, int size);
	// -------------------------------------------------------------------------
	Variable_t parse_variable(hash_t type, const char* struct_name,  const char *variables, const char* fromNamespace, bool is_key = false);
	// -------------------------------------------------------------------------
	void parse_command(hash_t command, const char* type, const char *variables);
	// -------------------------------------------------------------------------
	void parse_function(hash_t type, const char *name, const char *args,
			const char *body);
	// -------------------------------------------------------------------------
	void parse_struct(const hash_t& type, const char* name, const char* body);
	// -------------------------------------------------------------------------
	void parse_typedef(const char* body);
	// -------------------------------------------------------------------------
	int getType(const hash_t& hash);
	// -------------------------------------------------------------------------
	Typedef_t getRealType(const hash_t& hash);
	// -------------------------------------------------------------------------
	int getCommand(const hash_t& hash);
	// -------------------------------------------------------------------------
	int getBase(const hash_t& hash);
	// -------------------------------------------------------------------------
	static void minify(const char* code, String& result);
	// -------------------------------------------------------------------------
	const char* getName(hash_t hash);
	const char* type2Name(int type);
	// -------------------------------------------------------------------------
	inline void clear()
	{
		structs.clear();
		variables.clear();
		udefines.clear();
		modules.clear();
	}

	// -------------------------------------------------------------------------
	Enum_t_v enums;
	Typedef_t_v typedefs;
	Struct_t_v structs;
	Variable_t_v variables;
	Module_t_v modules;

	// dispatched command for easy access
	UDefine_t_v udefines;
	
	String nameSpace; /* current namespace "::" or '' == global */

}; // end of class Parser

/**
 * @brief TODO
 */
class IdlParser : public Parser
{
public:
	IdlParser() : Parser(), code(), defines(), linearize(0),
		generate_comment(1) {/** call String optimize(String code) */}
	IdlParser(const String& file);
	~IdlParser() { defines.clear(); code.clear(); }

	/**/
	virtual String user_optimize() { return ""; };
	String optimize(const char* file, const String& code);
	String getExtensions();

	// -------------------------------------------------------------------------
	String var2Real(const Variable_t& v);

	// -------------------------------------------------------------------------
	inline void define(const char *name,const char *value)
	{
		defines[name] = value;
	}

	// -------------------------------------------------------------------------
	inline void undef(const char *name)
	{
		//defines.remove(name);
		defines.erase(name);
	}

	// -------------------------------------------------------------------------
	inline int ifdef(const char *name)
	{
		return defines.find(name) != defines.end();
	}

	// -------------------------------------------------------------------------
	char *preprocessor(const char *name);

	// -------------------------------------------------------------------------
	char *preprocessor(const char* path,const char* file,char *data,int len);

	// -------------------------------------------------------------------------
	int parse(const char* src);

	// private - refactorize code --
	void refactorize();

	// -------------------------------------------------------------------------
	String code;
	
	// -------------------------------------------------------------------------
	Map<String, String> defines;
	Map<String, String> extensions;

	// last line source
	char line[1024];

	int linearize; // def : false
	int generate_comment; // default off

}; // end of class IdlParser


