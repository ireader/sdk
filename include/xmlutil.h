#ifndef _xmlutil_h_
#define _xmlutil_h_

#include <string>
#include <assert.h>

// simple xml encode
inline std::string& XmlEncode(std::string& xml, const char* source)
{
	// encode: & < > " '
	for(const char* p=source; *p; ++p)
	{
		switch(*p)
		{
		case '&':
			xml += "&amp;";
			break;
		case '<':
			xml += "&lt;";
			break;
		case '>':
			xml += "&gt;";
			break;
		case '\"':
			xml += "&quot;";
			break;
		case '\'':
			xml += "&apos;";
			break;
		default:
			xml += *p;
		}
	}

	return xml;
}

inline std::string& XmlTag(std::string& xml, const char* name, const char* value)
{
	assert(name && *name && value);
	xml += '<';
	xml += name;
	xml += '>';

	xml += value;

	xml += '<';
	xml += '/';
	xml += name;
	xml += '>';
	return xml;
}

inline std::string& XmlTag2(std::string& xml, const char* name, const char* value)
{
	std::string encodedValue;
	XmlEncode(encodedValue, value);
	return XmlTag(xml, name, encodedValue.c_str());
}

#endif /* !_xmlutil_h_ */
