#ifndef _XMLParser_h_
#define _XMLParser_h_

#include <list>
#include <stack>
#include <string>
#include "tinyxml.h"

#ifndef stricmp
	#if defined(_WIN32) || defined(_WIN64)
		#define stricmp		_stricmp
	#else
		#define stricmp strcasecmp
	
	#endif
#endif

class XMLParser
{
public:
	XMLParser(const char* xml)
	{
		m_doc.Parse(xml);
		const TiXmlElement* root = m_doc.RootElement();
		if(root)
			m_context.push(root);
	}

	bool Valid() const
	{
		return !m_context.empty();
	}

	const char* GetEncoding() const
	{
		const static std::string s_empty;
		const TiXmlElement* root = m_doc.RootElement();
		const TiXmlNode* sibling = root->PreviousSibling();
		while(sibling)
		{
			const TiXmlDeclaration* dec = sibling->ToDeclaration();
			if(dec)
			{
				const char* encoding = dec->Encoding();
				if(encoding && *encoding)
					return encoding;
			}
			sibling = sibling->PreviousSibling();
		}
		return s_empty.c_str();
	}

public:
	bool Foreach(const char* xpath) const
	{
		const TiXmlElement* node = FindFirst(xpath);
		if(NULL == node)
			return false;

		m_context.push(node);
		return true;
	}

	bool Next() const
	{
		const TiXmlElement* node = m_context.top();
		assert(node);

		const char* name = node->Value();
		assert(name);
		const TiXmlElement* next = node->NextSiblingElement(name);

		m_context.pop(); // pop up previous node
		assert(!m_context.empty());

		if(!next)
			return false;

		m_context.push(next); // push new top node
		return true;
	}

	void End()
	{
		m_context.pop();
		assert(!m_context.empty());
	}

public:
	const char* GetValue(const char* xpath) const
	{
		const static std::string s_empty;
		const TiXmlElement* node = FindFirst(xpath);
		if(NULL == node)
			return NULL;
		const char* p = node->GetText();
		return p?p:s_empty.c_str();
	}

	const char* GetValue(const char* xpath, const char* defaultValue) const
	{
		const static std::string s_empty;
		const TiXmlElement* node = FindFirst(xpath);
		if(NULL == node)
			return defaultValue;
		const char* p = node->GetText();
		return p?p:s_empty.c_str();
	}

	bool GetValue(const char* xpath, std::string& value) const
	{
		const TiXmlElement* node = FindFirst(xpath);
		if(NULL == node)
			return false;
		const char* text = node->GetText();
		value.assign(NULL==text?"":text);
		return true;
	}

	bool GetValue(const char* xpath, int& value) const
	{
		const char* text = GetValue(xpath);
		if(NULL == text)
			return false;
		value = atoi(text);
		return true;
	}

	bool GetValue(const char* xpath, bool& value) const
	{
		const char* text = GetValue(xpath);
		if(NULL == text)
			return false;
		value = (0 == stricmp(text, "true"));
		return true;
	}

public:
	const char* GetAttribute(const char* xpath, const char* name) const
	{
		const TiXmlElement* node = FindFirst(xpath);
		if(NULL == node)
			return NULL;
		return node->Attribute(name);
	}

	bool GetAttribute(const char* xpath, const char* name, std::string& value) const
	{
		const char* text = GetAttribute(xpath, name);
		if(NULL == text)
			return false;
		value = text;
		return true;
	}

	bool GetAttribute(const char* xpath, const char* name, int& value) const
	{
		const char* text = GetAttribute(xpath, name);
		if(NULL == text)
			return false;
		value = atoi(text);
		return true;
	}

	bool GetAttribute(const char* xpath, const char* name, bool& value) const
	{
		const char* text = GetAttribute(xpath, name);
		if(NULL == text)
			return false;
		value = (0==stricmp(text, "true"));
		return true;
	}

public:
	int GetCode() const
	{
		if(!Valid())
			return -1;

		const TiXmlElement* root = m_doc.RootElement();
		const TiXmlElement* node = root->FirstChildElement("return");
		if(NULL == node)
			return -1;

		const char* text = node->GetText();
		if(NULL == text)
			return -1;

		int r = atoi(text);
		return r;
	}

	const char* GetVersion() const
	{
		const TiXmlElement* root = m_doc.RootElement();
		if(NULL == root)
			return NULL;
		return root->Attribute("version");
	}

	const char* GetCommand() const
	{
		const TiXmlElement* root = m_doc.RootElement();
		if(NULL == root)
			return NULL;
		return root->Attribute("command");
	}

protected:
	const TiXmlElement* Top() const
	{
		assert(!m_context.empty());
		return m_context.top();
	}

	const TiXmlElement* FindFirst(const char* xpath) const
	{
		assert(xpath);
		if(0 == strncmp(xpath, "//", 2))
		{
			// un-implement
			return NULL;
		}
		else if('/' == *xpath)
		{
			const TiXmlElement* root = m_doc.RootElement();
			return (const TiXmlElement*)GetFirstElement(root, xpath+1);
		}
		else
		{
			return (const TiXmlElement*)GetFirstElement(Top(), xpath);
		}
	}

private:
	static const TiXmlNode* GetFirstElement(const TiXmlNode* node, const char* xpath)
	{
		assert(node && xpath);
		if(0 == strncmp("..", xpath, 2)) // parent node
		{
			xpath += 2;
			if(0 == *xpath)
				return node->Parent();

			if('/' != *xpath)
				return NULL;
			return GetFirstElement(node->Parent(), xpath+1);
		}
		else if('.' == *xpath) // current node
		{
			xpath += 1;
			if(0 == *xpath)
				return node;

			if('/' != *xpath)
				return NULL;
			return GetFirstElement(node, xpath+1);
		}
		else
		{
			const char* p = strchr(xpath, '/');
			if(NULL == p)
			{
				return node->FirstChildElement(xpath);
			}
			else
			{
				std::string name(xpath, p-xpath);
				const TiXmlElement* child = node->FirstChildElement(name);
				if(NULL == child)
					return NULL;
				return GetFirstElement(child, p+1);
			}
		}
	}

private:
	TiXmlDocument m_doc;
	typedef std::stack<const TiXmlElement*> TContext;
	mutable TContext m_context;
};

#endif /* !_XMLParser_h_ */
