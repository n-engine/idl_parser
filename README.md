# idl_parser v0.1
An idl parser who support lot of OMG IDL standard.
The parser will understand the IDL format and store all info inside vector variables.
Then it will be easy to do whatever with the data.

# Sample usage :
<pre><code>
#include "stdio.h"
#include "stdlib.h"
#include "iostream"

#include "idl_parser.h"

class MyParser : public IdlParser
{
public:
	MyParser(const String& idlFile) : IdlParser(idlFile) {}
	
	virtual String user_optimize()
	{
		String r;
		r << "// structure(s)\n";
		for(int i = 0; i < structs.size(); ++i)
		{
			r << "typedef struct " << structs[i].name << " {\n"
			for(int j = 0; j < structs[i].fields.size(); ++j)
			{
				const Variable_t& v = structs[i].fields[j];

				r << "    " << v.type.name << " " << v.name << ";\n"
			}
			r << "}" structs[i].name "; /** of of struct */\n"
		}
	}
};

int main()
{
	MyParser parser( "sample.idl" );

	/** do export the result code */
	std::cout << parser.code << std::endl;

	return 0;
}
</code></pre>
