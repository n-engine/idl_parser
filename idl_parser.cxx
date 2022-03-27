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
 * variable name must not contain any "type name" >> error
 * array
 * 
 * unsupported:
 * ------------
 * In-line nested types are not supported.
 * prama #if is not supported right now.
 * array >> ie.: "char a[10];" (on the todo list)
 * 
 * ref(s) :
 * https://www.omg.org/spec/IDL/4.2/PDF
 * https://fast-dds.docs.eprosima.com/en/latest/fastddsgen/dataTypes/dataTypes.html
 * https://d2vkrkwbbxbylk.cloudfront.net/rti-doc/510/ndds.5.1.0/doc/pdf/RTI_CoreLibrariesAndUtilities_UsersManual.pdf
 */
#include "idl_parser.h"

/**
 * Note for maintainer :
 * use the built in function (read_XXX) when possible
 * so the code can be ported without much effort to 'C' or any low level language
 */

#define error			printf
#define MY_DEBUG(...)	std::cout << " @ " << __LINE__ << " : " << __VA_ARGS__ << std::endl
#define TRACE_ERROR		MY_DEBUG
#define TRACE_WARNING	MY_DEBUG
#define TRACE_DEBUG		MY_DEBUG

#define N_ALLOCATE(type,size_)		malloc(size_)
#define N_DEALLOCATE	free
#define ne_assert		assert

#define minify_code \
	/* Minify code */ \
	while(*s) \
	{ \
		/* skip // comments */ \
		if(*s == '/' && *(s + 1) == '/') \
		{ while(*s && *s != '\n') { s++; } } \
		/* skip comments */  \
		else if(*s == '/' && *(s + 1) == '*') \
		{ \
			while(*s && (*s != '*' || *(s + 1) != '/')) { s++; } \
			s += 2; \
		} \
		/* don't change strings */ \
		else if(*s == '"') \
		{ \
			*d++ = *s++; \
			while(*s && (*s != '"' || *(s - 1) == '\\')) *d++ = *s++; \
			if(*s) *d++ = *s++; \
		} \
		/* skip \r symbols */ \
		else if(*s == '\r') { s++; } \
		/* change tab to space */ \
		else if(*s == '\t') { s++; *d++ = ' ';  } \
		/* double space */ \
		else if(*s == ' ' && *(s + 1) == ' ') { s++; } \
		/* double LF */ \
		else if(*s == '\n' && *(s + 1) == '\n') { s++; } \
		/* all other string is cloned */ \
		else { *d++ = *s++; } \
	} \
	*d = '\0';


uchar* getFile(const char* fileName, ulong& bufferSize)
{
	FILE* fp = fopen(fileName, "rb");
	bufferSize = 0;
	uchar* data = 0;
	
	if(!fp)
		return data;

	int uiCur = ftell(fp);
	fseek(fp, 0, SEEK_END);
	bufferSize = (ulong)ftell(fp);
	fseek(fp, uiCur, SEEK_SET);

	data = (uchar*)N_ALLOCATE(uchar, bufferSize);
	fread(data,1,bufferSize,fp);
	fclose(fp);

	return data;
}

char *IdlParser::preprocessor(const char *name)
{
	char path[1024] = {0};
	strncpy(path,name,sizeof(path));
	char *p = path + strlen(path);
	while(p > path && (*p != '/' && *p != '\\')) p--;
	*p = '\0';

	MY_DEBUG("Preprocess name: " << name );

	ulong bufferSize	= 0;
	uchar* BufferData	= 0;

	BufferData = getFile(name, bufferSize);
	int size = bufferSize;

	if (!BufferData)
	{
		TRACE_ERROR("Can't load file: " << name);
		ne_assert(0);
	}

	char *data = (char*)N_ALLOCATE(char,(bufferSize+1));
	memset(data, 0x0, sizeof(char) * bufferSize+1);

	memcpy(data,BufferData,bufferSize);
	data[bufferSize] = '\0';
	N_DEALLOCATE(BufferData);

	return preprocessor( path, name, data, strlen(data) );
}

// ----------------------------------------------------------------------------
#define SZ(str) (sizeof(str)-1)
#define EVAL(ID,entity_) \
 {ID_##ID,entity_,SZ(entity_),getHash(entity_),}
#undef CONST
#undef VOID
#undef BOOL
#undef INT
#undef UINT
#undef UINT8
#undef UINT16
#undef UINT32
#undef UINT64
static struct internal_hash_t {
	int id; // ie.: ID_VOID - must be the same as the offset inside __internal_hash
	const char* name;
	const int size; // size of string name
	const hash_t hash;
} __internal_hash[] = {
	// type / declaration -----------------------------------------------------
	EVAL(VOID,	"void"),		//
	EVAL(OCTET,	"octet"),		//
	EVAL(INT8,	"int8_t"),		//
	EVAL(INT16,	"int16_t"),		//
	EVAL(SHORT,	"short"),		//
	EVAL(INT32,	"int32_t"),		//
	EVAL(INT,	"int"),			//
	EVAL(LONG,	"long"),		//
	EVAL(INT64,	"int64_t"),		//
	EVAL(LONGLONG,"long long"),	// !special case, take care
	EVAL(UINT8, "uint8_t"),		//
	EVAL(UINT16,"uint16_t"),	//
	EVAL(UINT32,"uint32_t"),	//
	EVAL(UINT64,"uint64_t"),	//
	EVAL(BOOL,	"bool"),		//
	EVAL(BOOLEAN,"boolean"),	//
	EVAL(CHAR,	"char"),		//
	EVAL(FLOAT,	"float"),		// 
	EVAL(STRING,"string"),		// 
	EVAL(DOUBLE,"double"),		// 
	EVAL(SEQUENCE,"sequence"),	// 
	EVAL(CONST,	"const"),		// find a better way : > "delcaration" before type
								// for now, just assume the keyword after const is a right type.

	// Base -------------------------------------------------------------------
	EVAL(STRUCT,"struct"),		// 
	EVAL(MODULE,"module"),		// 
	EVAL(TYPEDEF,"typedef"),	// 
	/** TODO: 
	 * 'map'
	 * 'bitset'
	 * 'bitmask'
	 * ...
	 * ref(s) :
	 * https://www.omg.org/spec/IDL/4.2/PDF
	 * Table 7-6: All IDL keywords
	 */

}; // internal_hash_t

// ----------------------------------------------------------------------------
void Parser::minify(const char* code, String &result)
{
	std::istringstream buff(code);
	std::string line;

	while (std::getline(buff, line))
	{
		if (!line.size()) continue;
		result << line.c_str() << "\n";
	}

	result.replace("\n\n", "");		// double return
	result.replace(" \n \n ", "");	// space return return space
	result.replace("\n \n", "");	// return space return
	result.replace("  ", " ");		// double space
}

// ----------------------------------------------------------------------------
const char* Parser::getName(hash_t hash)
{
	int i;

	// built in
	for (i = 0; i < ARRAY_SIZE(__internal_hash); ++i)
	{
		if (hash == __internal_hash[i].hash)
			return __internal_hash[i].name;
	}

	// user type
	for (i = 0; i < typedefs.size(); ++i)
	{
		if (hash == typedefs[i].hash)
			return typedefs[i].name;
	}
	// struct
	for (i = 0; i < structs.size(); ++i)
	{
		if (hash == structs[i].hash)
			return structs[i].name;
	}

	//ne_assert(0);

	return "";
}

// todo: type can be a define
/** refactorize the search method */
const char* Parser::type2Name(int type)
{
	// user type
	if (type >= USER_BASE_SPACER_STRUCT)	return structs[type - USER_BASE_SPACER_STRUCT].name;
	if (type >= USER_BASE_SPACER_TYPEDEF)	return typedefs[type - USER_BASE_SPACER_TYPEDEF].name;
	// base type
	if (type >= BASE_SPACER)		return __internal_hash[type - BASE_SPACER].name;
	// builtin type
	if (type >= TYPE_SPACER )		return __internal_hash[type - TYPE_SPACER].name;
	//ne_assert(0);

	return "";
}

int Parser::getType(const hash_t& hash)
{
	int result=-1;

	if ( is_builtin_type(hash, result)) return result;
	if ( is_builtin_base(hash, result)) return result;
	if ( is_user_base(hash, result)) return result;

	return result;
}

Typedef_t Parser::getRealType(const hash_t& hash)
{
	Typedef_t t;
	int i;

	// built-in type
	for (i = 0; i < LAST_TYPE; ++i)
	{
		if (hash == __internal_hash[i].hash)
		{
			t.hash = hash;
			t.name = __internal_hash[i].name;
			t.type = i+TYPE_SPACER;
			return t;
		}
	}

	// typedef
	for (i = 0; i < typedefs.size(); ++i)
	{
		error("type: %s %x\n", typedefs[i].name.c_str(), typedefs[i].hash );

		if (typedefs[i].hash == hash)
		{
			t = typedefs[i];

			if ( t.baseName.size() )
			{
				hash_t hash = getHash( t.baseName );
				// keep base info (original type + size
				int tt = t.type;
				int ts = t.size;
				t = getRealType( hash );
				t.type = tt;
				t.size = ts;
			}
			return t;
		}
	}
	
	// structs
	for (i = 0; i < structs.size(); ++i)
	{
		// error("type: %s %x\n", structs[i].name.c_str(), structs[i].hash );

		if (structs[i].hash == hash)
		{
			t.hash = hash;
			t.name = structs[i].name;
			t.nameSpace = nameSpace;
			t.baseName = t.name; // user type
			t.type = ID_STRUCT + USER_BASE_SPACER_STRUCT;
			return t;
		}
	}
	
	error("unknown type: %x\n",hash);

	return t;
}

int Parser::getBase(const hash_t& hash)
{
	int result;
	if (is_builtin_base(hash, result)) return result;
	ne_assert(0);
	return -1;
}

int Parser::is_builtin_type(const hash_t& hash, int& result)
{
	result = -1;
	// built-in type
	for (int i = 0; i < LAST_TYPE; ++i)
	{
		if (hash == __internal_hash[i].hash)
		{
			result = i+TYPE_SPACER;
			return 1;
		}
	}
	return 0;
}

int Parser::is_builtin_base(const hash_t& hash, int& result)
{
	result = -1;
	// built-in type
	for (int i = BASE_START; i < LAST_BASE; ++i)
	{
		if (hash == __internal_hash[i].hash)
		{
			result = i+BASE_SPACER;
			return 1;
		}
	}
	return 0;
}

int Parser::is_struct(const hash_t& hash, int& result)
{
	result = -1;

	for (int i = 0; i < structs.size(); ++i)
	{
		if (structs[i].hash == hash)
		{
			result = i+USER_BASE_SPACER_STRUCT;
			return 1;
		}
	}
	return 0;
}

int Parser::is_typedef(const hash_t& hash, int& result)
{
	result = -1;

	for (int i = 0; i < typedefs.size(); ++i)
	{
		if (typedefs[i].hash == hash)
		{
			result = i+USER_BASE_SPACER_TYPEDEF;
			return 1;
		}
	}
	return 0;
}

int Parser::is_user_base(const hash_t& hash, int& result)
{
	if (is_typedef(hash, result)) return 1;
	if (is_struct(hash, result)) return 1;
	return 0;
}

inline
String remove_namespace(const String& name, String& nameSpace)
{
	/** ::name1::name2 */
	String s(name);
	nameSpace = "";
	s.replace("::", "/");
	String_v mar = Explode(s, '/' );
	
	/** "0: name1 1: name2" */
	if ( mar.size() >= 2 )
	{
		nameSpace = mar[0];
		s = String::basename(mar[1]);
	}

	return s;
}

void Parser::parse_struct(const hash_t& type, const char* name, const char* body)
{
	String sbody;
	char variable[4096];
	Variable_t v;
	Struct_t str;
	str.hash = getHash(name);		// hash(name)
	str.type = getBase(type);		// check built-in base
	str.name = name;
	str.nameSpace = nameSpace; // current namespace
	const char* s = body;

	minify(body, sbody);

	MY_DEBUG("Storing struct : name: '" << name << "' body: '" << sbody << '\'');

	s += skip_spaces(s);
	while( *s )
	{
		String fromNameSpace;
		bool is_key = false;
		s += read_block(s, variable, sizeof(variable), 0, ';');
		s += skip_spaces(s);
		MY_DEBUG("variable : '" << variable << "'");
		
		/***/
		String_v mar = Explode(variable, ' ');

		if ( mar.size() >= 2 )
		{
			hash_t type;
			String varName;

			if ( mar.size() >= 3 )
			{
				/**
				 * mar[0] == key
				 * mar[1] == type
				 * mar[2] == field name
				 */
				if ( mar[0] == "@key" )
				{
					/* remove any namespace from type string */
					mar[0] = remove_namespace( mar[0], fromNameSpace );
					TRACE_DEBUG( "mar[0] >>> " << mar[0] );
					type = getHash( mar[1] );
					varName = mar[2];
					is_key = true;
				}
				else
				{
					TRACE_ERROR("Unknwon type for variable: " << mar[0] );
				}
			}
			else
			{
				/**
				 * mar[0] == type
				 * mar[1] == field name
				 */
				/* remove any namespace from type string */
				mar[0] = remove_namespace( mar[0], fromNameSpace );
				TRACE_DEBUG( "mar[0] >>> " << mar[0] );
				type = getHash( mar[0] );
				varName = mar[1];
			}

			v = parse_variable( type /* var type */,
				name /* struct  name */,
				varName,
				fromNameSpace,
				is_key
			);
			
			str.fields.push_back( v );
		}
	}

	structs.push_back( str );
}

void Parser::parse_typedef(const char* body)
{
	TRACE_DEBUG( "Typedef: '" << body << "'" );
	String input(body);
	input.replace(", ", ","); // seq<name, size>

	String_v mar = Explode( input, ' ' );
	Typedef_t typeDef;
	hash_t b_hash = 0;
	int result=0;

	if ( mar.size() >= 2 )
	{
		b_hash = getHash( mar[0] );
	}

	if ( is_builtin_type(b_hash, result) || 
			is_user_base(b_hash, result) )
	{
		String baseTypeName( type2Name(result) );
		String newTypeName( mar[1] );
		TRACE_DEBUG("Storing typedef > baseTypeName: '" << baseTypeName << 
			"' newTypeName: '" << newTypeName << "'" );

		/** store new type */
		b_hash = getHash( newTypeName );
		typeDef.hash = b_hash;
		typeDef.type = result;
		typeDef.name = newTypeName;
		typeDef.baseName = baseTypeName;
		typeDef.nameSpace = nameSpace;
		typedefs.push_back( typeDef );
	}
	// TODO: rewrite me this sh** ....
	else if ( mar[0].find("sequence") != String::npos )
	{
		String tStr(mar[0]);
		String_v mar2;
		/** TODO handle sequence */
		TRACE_ERROR("Type is a sequence: '" << mar[0] << "'");
		
		tStr.replace("<"," ");
		tStr.replace(">"," ");
		mar2 = Explode(tStr.c_str(), ' ');

		/**
		 * a sequence can be :
		 * sequence<type> name
		 * sequence<type,size> name
		 * 
		 * mar[0] == sequence<type>
		 * mar[1] == name
		 * mar2[1] == type name
		 */
		b_hash = getHash( mar[1] );
		typeDef.hash = b_hash;
		// hackish
		typeDef.type = ID_SEQUENCE + TYPE_SPACER;
		typeDef.name = mar[1];
		typeDef.baseName = mar2[1];
		typeDef.size = 0;
		typeDef.nameSpace = nameSpace;

		/** check for sequence size */
		mar2 = Explode( mar[0].c_str(), ',' );
		if ( mar2.size() >= 2 )
		{
			/**
			 * mar2[0] == type
			 * mar2[1] == size
			 */
			String_v mar3;
			mar2[0].replace("<"," "); /** 'sequence type' */
			mar2[1].replace(">","");
			mar3 = Explode(mar2[0], ' ' );
			TRACE_DEBUG( "mar2 > 0: '" << mar2[0] << "' 1:" << mar2[1] );

			if ( mar3.size() == 2 )
			{
				Typedef_t t;
				/**
				 * mar3[0] == sequence
				 * mar3[1] == type
				 */
				 t = getRealType(getHash(mar3[1]));
				 t.type = ID_SEQUENCE + TYPE_SPACER;
				 typeDef.baseName = t.name;
				 TRACE_DEBUG( "mar3 > : " << t.name );
			}
			typeDef.size = atol( mar2[1].c_str() );
			
			TRACE_DEBUG( "typeDef.size > : " << typeDef.size );
		}
		typedefs.push_back( typeDef );
	}
	else
	{
		TRACE_ERROR("Unknown type: " << mar[0] );
	}
}

// ----------------------------------------------------------------------------
Variable_t Parser::parse_variable(
	hash_t type, const char* struct_name, const char *name,
	const char* fromNamespace, bool is_key )
{
	Variable_t v;

	v.type = getRealType( type ); // std type or user type

	printf("variable %s type: %s", name, v.type.name.c_str() );

	v.hash = getHash(name);

	v.name = name;
	v.is_key = is_key;
	v.struct_name = struct_name;
	v.fromNamespace = fromNamespace;

	// TODO: check if the namespace is known

	if ( struct_name )
	{
		MY_DEBUG("Storing variable : " << struct_name << " > " << name << 
			" (isKey: " << (is_key ? 1 : 0) << ") fromNamespace: " << fromNamespace
		);
	}
	else
	{
		MY_DEBUG("Storing variable(global) : " << name << 
			" (isKey: " << (is_key ? 1 : 0) << ") fromNamespace: " << fromNamespace
		);
	}

	variables.push_back( v );

	return v;
}

// -----------------------------------------------------------------------------

int IdlParser::parse(const char* src)
{
	const char *s = src;

	while (*s)
	{
		s += skip_spaces(s);

		// read current line
		const char *l = s;
		while (l > src && *(l - 1) != '\n') l--;
		MY_DEBUG("read_block(" << l << ")");
		read_block(l, line, sizeof(line), 0, '\n');

		// end of source
		if (*s == '\0')
		{
			break;
		}
		else if (*s == ';')
		{
			s++;
			continue;
		}
		// braces
		else if (*s == '{')
		{
			s++;
			s += parse(s);
			continue;
		}
		else if (*s == '}')
		{
			s++;
			break;
		}
		// check next symbol
		else if ( (isalpha(*s) == 0) && 
			(strchr("_:", *s) == NULL) )
		{
			TRACE_ERROR("Parser::parse(): unknown symbol '" << *s << "'");
		}

		// read next token
		char buf[256];
		MY_DEBUG("read_token()");
		s += read_token(s, buf, sizeof(buf));
		if (buf[0] == '\0') break;

		const hash_t b_hash = getHash(buf);

		printf("token: %s\n", buf);

		int result = 0;

		// variable or function (can be built-in type or user type)
		if ( is_builtin_type(b_hash, result) || 
			is_user_base(b_hash, result) )
		{
			MY_DEBUG("\t>> Builtin");
			// save position
			const char *begin = s;
			char name[1024];
			s += read_name(s, name, sizeof(name));

			MY_DEBUG("Built-in type ('" << name << "')");

			{
				MY_DEBUG("\t\t>> variable");
				s = begin;
				char variables[4096];
				MY_DEBUG("read_block()");
				s += read_block(s, variables, sizeof(variables), 0, ';');
				MY_DEBUG("parse_variable()");
				parse_variable(b_hash, "", variables, "" );
			}
		}
		else if (Parser::is_builtin_base(b_hash, result))
		{
			TRACE_DEBUG("\t>> BuiltinBase << " << buf);

			switch ((result-BASE_SPACER))
			{
				case ID_TYPEDEF:
				{
					char typedefs[4096];
					char name[1024];
					s += skip_spaces(s);
					TRACE_DEBUG("Builtin typedef: '" << s << "'");
					s += read_block(s, typedefs, sizeof(typedefs), 0, ';');
					parse_typedef(typedefs);
				}
				break;
				case ID_STRUCT:
				{
					char name[1024];
					s += read_name(s, name, sizeof(name));
					s += expect_symbol(s, '{');
					uint size = strlen(s) + 1;
					char *dest = (char*)N_ALLOCATE(char,size);
					memset(dest, 0x0, sizeof(char) * size);

					TRACE_DEBUG("struct read_block()");
					s += read_block(s, dest, size, 0, '}');
					parse_struct(b_hash, name, dest);
					N_DEALLOCATE(dest);

					if (*s == ';') s++;
				}
				break;

				case ID_MODULE:
				{
					char name[1024];
					s += read_name(s, name, sizeof(name));
					TRACE_DEBUG( "module name : " << name );
					nameSpace = name;

					s += parse(s);
				}
				break;
			}
		}
		// check if it's a define
		else if ( ifdef(buf) )
		{
			char variables[4096];
			TRACE_DEBUG("read_block()");
			s += read_block(s, variables, sizeof(variables), 0, ')');

			UDefine_t d;
			d.line = buf;
			d.line += variables;
			d.line += ");";
			udefines.push_back( d );
		}
		// unknown token
		else
		{
			TRACE_ERROR("Parser::parse(): unknown token \""<< buf <<"\"");
		}
	}

	return s - src;
}

// -----------------------------------------------------------------------------

char *IdlParser::preprocessor(const char* path, 
	const char* file,char *data,int len)
{
	MY_DEBUG("Preprocess code: " << data );

	int size = len;
	const char* name = file;
	char *s = data;		// source string
	char *d = data;		// dest string

	minify_code

	s = data;
	d = data;

	//MY_DEBUG( "Minified code: '" << data << '\'' );

	int_v defines;
	int define_ok = 1;
	int current_line = 0;

	// preprocessor
	while(*s)
	{
		// line counter
		if(*s == '\n')
			current_line++;

		if(*s == '#')
		{
			char buf[256] = { 0 };

			// read current line
			read_block(s,line,sizeof(line),0,'\n');
			s++;

			s += read_token(s,buf,sizeof(buf));

			MY_DEBUG("Parsing block: " << buf );

			// #ifdef ---------------------------------------------------------
			if (!strcmp(buf, "ifdef"))
			{
				char name[256] = { 0 };
				s += read_name(s, name, sizeof(name));
				defines.push_back(ifdef(name));
				define_ok = 1;
				for (int i = 0; i < defines.size(); ++i)
				{
					if (defines[i] == 0)
					{
						define_ok = 0;
						break;
					}
				}
				continue;
			}
			// #elif ----------------------------------------------------------
			if (!strcmp(buf, "elif"))
			{
				TRACE_ERROR("Directive '#elif' is not supported, skipping.");
				while (*s && *s != '\n') s++;
				continue;
			}
			// #ifndef --------------------------------------------------------
			else if(!strcmp(buf,"ifndef"))
			{
				char name[256] = {0};
				s += read_name(s,name,sizeof(name));
				defines.push_back(!ifdef(name));
				define_ok = 1;
				for(int i = 0; i < defines.size(); ++i)
				{
					if(defines[i] == 0)
					{
						define_ok = 0;
						break;
					}
				}
				continue;
			}
			// #else ----------------------------------------------------------
			else if(!strcmp(buf,"else"))
			{
				//MY_DEBUG( "#else block - size : " << defines.size() );
				if(defines.size() < 1)
				{
					N_DEALLOCATE(data);
					TRACE_ERROR("IdlParser::preprocessor(): #else is before "
						"#ifdef, #ifndef, #if");
				}

				int v = !defines[ defines.size() - 1 ];

				//MY_DEBUG("Previous define : " << v);
				defines[defines.size() - 1] = v;

				define_ok = 1;
				for(int i = 0; i < defines.size(); ++i)
				{
					if(defines[i] == 0)
					{
						//MY_DEBUG("define ok == 0");
						define_ok = 0;
						break;
					}
				}
				continue;
			}
			// #endif ---------------------------------------------------------
			else if(!strcmp(buf,"endif"))
			{
				if(defines.size() < 1)
				{
					N_DEALLOCATE(data);
					TRACE_ERROR("IdlParser::preprocessor(): #endif is before "
						"#ifdef or #ifndef");
				}
				//defines.erase( defines.end() );
				defines.pop_back();
				define_ok = 1;
				for(int i = 0; i < defines.size(); ++i)
				{
					if(defines[i] == 0)
					{
						//MY_DEBUG("Define == 0");
						define_ok = 0;
						break;
					}
				}
				continue;
			}
			// #if ------------------------------------------------------------
			else if (!strcmp(buf, "if"))
			{
				TRACE_ERROR("Directive '#if' is not supported, skipping.");
				while (*s && *s != '\n') s++;
				continue;
			}
			// #define --------------------------------------------------------
			else if(!strcmp(buf,"define"))
			{
				char name[256] = {0};
				char value[1024] = {0};
				s += read_name(s,name,sizeof(name));

				if (*s == '\n')
				{
					value[0] = '\0';
					s++;
				}
				else
					s += read_block(s,value,sizeof(value),0,'\n');

				if(define_ok)
				{
					define(name,value);
				}
				
				continue;
			}
			// #undef ---------------------------------------------------------
			else if(!strcmp(buf,"undef"))
			{
				char name[256] = {0};
				s += read_name(s,name,sizeof(name));
				if(define_ok)
				{
					undef(name);
				}
				continue;
			}
			// #pragma ---------------------------------------------------------
			else if(!strcmp(buf,"pragma"))
			{
				char name[256] = {0};
				char value[1024] = {0};
				s += read_name(s,name,sizeof(name));

				// TODO : handle pragma:
				// 'keylist' : <data-type-name> <key>*
				// 'cats', 'stac'
				// http://download.ist.adlinktech.com/docs/Vortex/html/ospl/IDLPreProcGuide/keys.html
				s += read_block(s,value,sizeof(value),0,'\n');

				continue;
			}
			// #include -------------------------------------------------------
			else if(!strcmp(buf,"include"))
			{
				char name[256] = {0};
				char c = get_symbol(s,"\"<");
				if(c == '"') s += read_block(s,name,sizeof(name),'"','"');
				else if(c == '<') s += read_block(s,name,sizeof(name),'<','>');
				if(define_ok)
				{
					char *include = preprocessor(name);
					if(include == NULL)
					{
						char buf[1024] = {0};
						sprintf(buf,"%s/%s",path,name);
						include = preprocessor(buf);
					}

					if(include == NULL) 
					{
						error("IdlParser::preprocessor(): can't find "
							"\"%s\" file\n",name);
					}

					int include_size = strlen(include);
					char *ndata = (char*)N_ALLOCATE(char, (size + include_size + 1));
					memset(ndata, 0x0, sizeof(char) * (size + include_size + 1));

					memset(ndata,0,size + include_size + 1);

					memcpy(ndata,data,d - data);
					memcpy(ndata + (d - data),include,include_size);
					memcpy(
						ndata + (d - data) + include_size, s, 
						size - (s - data) + 1);

					s = ndata + (d - data) + include_size;
					d = s;

					N_DEALLOCATE( include );
					N_DEALLOCATE( data );
					data = ndata;

					size += include_size;
				}
				continue;
			}
			// unknown preprocessor token -------------------------------------
			else
			{
				N_DEALLOCATE(data);
				error("IdlParser::preprocessor(): unknown prepocessor "
					"token \"#%s\"\n",buf);
			}
		}
		else
		{
			// clear line
			line[0] = '\0';
		}

		// preprocessor defines
		define("__FILE__",String("\"") + String(name) + ":" + 
			String(current_line) + "\"");
		define("__LINE__",String(current_line));

		if(define_ok == 0)
		{
			//MY_DEBUG("--- Skip previous");
			s++;
		}
		else
		{
			//MY_DEBUG("--- Handle previous");
			// don't change symbols
			if(*s == '\'' && *(s + 1) == '\\' && *(s + 2) && *(s + 3) == '\'')
			{
				for(int i = 0; i < 4; ++i) *d++ = *s++;
			}
			else if(*s == '\'' && *(s + 2) != '\\' && *(s + 2) == '\'')
			{
				for(int i = 0; i < 3; ++i) *d++ = *s++;
			}
			else
			{
				// don't change strings
				int is_string = 0;
				if(*s == '"') is_string = 1;

				*d = *s++;

				// defined words replacement
				if(d > data && strchr(" \n,.=:;()[]{}<>+-*/%!&|^\"'",*d))
				{
					char *t = d - 1;
					while(t >= data && 
						strchr(" \n,.=:;()[]{}<>+-*/%!&|^\"'",*t) == NULL)
					{
						t--;
					}
					t++;
					if(t < d)
					{
						for(Map<String,String>::iterator it =
							this->defines.begin(); it != this->defines.end();
							++it)
						{
							if(it->first.size() == d - t && 
								!strncmp(it->first,t,d - t))
							{
								const char *define = it->second;

								if ( define && *define != '0' )
								{
									int define_size = strlen(define);
									char *ndata = 
										(char*)N_ALLOCATE(char,(size + define_size + 1));
									memset(ndata,0,
										size + define_size + 1);

									memcpy(ndata,data,t - data);
									memcpy(ndata +
										(t - data),define,define_size);
									*(ndata + (t - data) + define_size) = *d;
									memcpy(ndata + (t - data) + 
									  define_size + 1,s,size - (s - data) + 1);

									s = ndata + (t - data) + define_size + 1;
									d = s - 1;

									N_DEALLOCATE(data);
									data = ndata;

									size += define_size;
								}
								else
								{
									*t = *d;
									d = t;
								}
								break;
							}
						}
					}
				} // end of the replacement

				d++;

				if(is_string)
				{
					while(*s && (*s != '"' || *(s - 1) == '\\'))
						*d++ = *s++;
					if(*s) *d++ = *s++;
				}
			}
		}
	}
	*d = '\0';

	if(defines.size() != 0)
	{
		N_DEALLOCATE(data);

		for (Map<String, String>::iterator it =
			this->defines.begin(); it != this->defines.end();
			++it)
		{
			const char *define = it->second;
			MY_DEBUG( "define: " << define );
		}
		error( "IdlParser::prepocessor(): bad preprocessor options\n" );
	}

	return data;
}

// -----------------------------------------------------------------------------
int Parser::skip_spaces(const char *src)
{
	const char *s = src;
	while (*s && strchr(" \n", *s)) s++;
	return s - src;
}

// -----------------------------------------------------------------------------
int Parser::get_symbol(const char *src, const char *symbols)
{
	const char *s = src;
	s += skip_spaces(s);
	if (symbols != NULL)
	{
		if ((*s == '\0' || strchr(symbols, *s) == NULL))
		{
			error("Parser::get_symbol(): bad '%c' symbol expecting "
				"%s\n", *s, symbols);
		}
	}
	return *s;
}

// -----------------------------------------------------------------------------
int Parser::read_name(const char *src, char *dest, int size)
{
	MY_DEBUG("read_name(" << src << ")");

	const char *s = src;
	s += skip_spaces(s);

	MY_DEBUG("read_name() >> skip_spaces(" << s << ")");

	if (dest == NULL)
	{
		error("Parser::read_name(): dest is NULL\n");
	}

	if (*s == '\0' || (isalpha(*s) == 0 && strchr("_:", *s) == NULL))
	{
		error("Parser::read_name(): bad name: '%s'\n", s);
	}

	if (*s == '\n')
		return s - src;

	while (*s && (isdigit(*s) || isalpha(*s) ||
		strchr("_:", *s)))
	{
		if (--size < 1)
		{
			error("Parser::read_name(): buffer overflow\n");
		}
		*dest++ = *s++;
	}

	*dest = '\0';

	return s - src;
}

// -----------------------------------------------------------------------------
int Parser::read_digit(const char *src, char *dest, int size)
{
	const char *s = src;
	s += skip_spaces(s);
	if (dest == NULL)
		error("Parser::read_digit(): dest is NULL\n");
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		*dest++ = *s++;
		*dest++ = *s++;
		while (*s && (isdigit(*s) || strchr("aAbBcCdDeEfF", *s)))
		{
			if (--size < 1)
				error("Parser::read_digit(): buffer overflow\n");
			*dest++ = *s++;
		}
	}
	else
	{
		while (*s)
		{
			if (--size < 1)
				error("Parser::read_digit(): buffer overflow\n");
			if (*s == '.' && isdigit(*(s - 1))) *dest++ = *s++;
			else if ((*s == 'e' || *s == 'E') && isdigit(*(s - 1)))
				*dest++ = *s++;
			else if ((*s == '+' || *s == '-') && (*(s - 1) == 'e' ||
				*(s - 1) == 'E')) *dest++ = *s++;
			else if ((*s == 'f' || *s == 'F') && isdigit(*(s - 1)))
				*dest++ = *s++;
			else if (isdigit(*s)) *dest++ = *s++;
			else break;
		}
	}
	*dest = '\0';
	return s - src;
}

// -----------------------------------------------------------------------------

int Parser::read_token(
	const char *src, char *dest, int size, const char *symbols)
{
	const char *s = src;
	s += skip_spaces(s);

	if (*s == '\0')
	{
		if (dest) *dest = '\0';
		return 0;
	}

	if (dest == NULL) size = 1000000;

	if (symbols)
	{
		while (*s && strchr(symbols, *s))
		{
			if (--size < 1)
				error("Parser::read_token(): buffer overflow\n");
			if (dest) *dest++ = *s++;
			else s++;
		}
	}
	else
	{
		while (*s && (isalpha(*s) || isdigit(*s) || strchr("_:", *s)))
		{
			if (--size < 1)
				error("Parser::read_token(): buffer overflow\n");
			if (dest) *dest++ = *s++;
			else s++;
		}
	}

	if (dest) *dest = '\0';

	return s - src;
}

// -----------------------------------------------------------------------------

int Parser::read_block(
	const char *src, char *dest, int size, char from, char to)
{
	const char *s = src;
	while (*s && *s == ' ') s++;

	// fix the block -------------------------------------
	if ((*s == ' ') && (*(s + 1) == '{') && (*(s + 2) == '\n'))
		s++;

	if (*s == '\0')
	{
		if (dest)
			*dest = '\0';
		return 0;
	}

	int is_string = 0;
	int braces = 0;
	//int escapde = 0;
	int counter = 0;

	if (*s == from)
	{
		if (*s == '"') is_string = 1;
		if (*s == '(') braces++;
		//if (*s == '\\') escapde++;
		counter++;
		s++;
	}

	// if zero target
	if (dest == NULL)
		size = 1000000;

	while (*s)
	{
		if (--size < 1)
		{
			error("Parser::read_block(): buffer overflow\n");
		}
		if (is_string == 0)
		{
			if (*s == '"')
			{
				is_string = 1;
			}
			else
			{
				if (*s == '(')
				{
					braces++;
				}
				else if (*s == ')')
				{
					if (--braces < 0)
					{
						error("Parser::read_block(): some errors with "
							"counts of '(' and ')' symbols\n");
					}
				}
				// escape
				if ( *s == '\\' && *(s + 1) == '\n')
				{
					s++; s++;// ignore and continue
				}
				if (from && to)
				{
					if (*s == from)
					{
						counter++;
					}
					else if (*s == to)
					{
						if (--counter < 0)
						{
							error("Parser::read_block(): some errors "
								"with counts of '%c' and '%c' symbols\n", from, to);
						}
					}
				}
			}
			if (*s == to && counter == 0 && braces == 0)
			{
				s++;
				break;
			}
			if (dest) {
				*dest++ = *s++;
			}
			else {
				s++;
			}
		}
		else if (is_string == 1)
		{
			if (*s == '"' && *(s - 1) != '\\')
			{
				is_string = 0;
				if (*s == to)
				{
					s++;
					break;
				}
			}
			if (dest)
			{
				*dest++ = *s++;
			}
			else
			{
				s++;
			}
		}
	}

	if (dest) *dest = '\0';

	return s - src;
}

int Parser::expect_symbol(const char *src, char symbol)
{
	const char *s = src;
	s += skip_spaces(s);
	if (*s != symbol)
	{
		if (*s == '\0')
		{
			error("Parser::expect_symbol(): end of string\n");
		}
		else if (symbol == '\0')
		{
			error("Parser::expect_symbol(): bad '%c' symbol\n", *s);
		}
		else
		{
			error("Parser::expect_symbol(): bad '%c' symbol expecting "
				"'%c'\n", *s, symbol);
		}
	}

	if (*s)
	{
		s++;
	}

	return s - src;
}

String IdlParser::var2Real(const Variable_t& v)
{
	String s;

	if ( v.fromNamespace.size() )
	{
		s << "::" << v.fromNamespace << "::" << v.type.name << " " << v.name <<";\n";
	}
	else
	{
		s << v.type.name << " " <<v.name <<";\n";
	}

	return s;
}

String IdlParser::optimize(const char* filename, const String& code)
{
	int i,j, found_entry_point=0;
	String str;
	int sz = code.size();
//	Function_t* entryPoint;

	if ( code )
	{
		MY_DEBUG("optimize filename: " << filename << " - code: '" << code << "' ");
	}
	else
	{
		MY_DEBUG("optimize filename: " << filename );
	}

	char *data = (char*)N_ALLOCATE(char, (code.size() + 1));
	memset(data, 0x0, sizeof(char) * code.size() + 1);
	memcpy(data, code.c_str(), sz);
	data[sz] = '\0';

	// preprocess (defines)
	char* rdata = preprocessor(".", filename, data, strlen(data) );

	// parse code and store all supported info
	parse(rdata);
	
	str = user_optimize();

	//ne_assert( defines.size() );

	N_DEALLOCATE( rdata );

	return str;
}

IdlParser::IdlParser(const String& file) : 
	Parser(), code(), defines(), linearize(0), generate_comment(1)
{
	char* str = preprocessor(file);
	code = optimize(file, str);
	N_DEALLOCATE(str);
}
