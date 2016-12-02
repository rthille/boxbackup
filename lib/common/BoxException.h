// --------------------------------------------------------------------------
//
// File
//		Name:    BoxException.h
//		Purpose: Exception
//		Created: 2003/07/10
//
// --------------------------------------------------------------------------

#ifndef BOXEXCEPTION__H
#define BOXEXCEPTION__H

#include <exception>
#include <string>

// --------------------------------------------------------------------------
//
// Class
//		Name:    BoxException
//		Purpose: Exception
//		Created: 2003/07/10
//
// --------------------------------------------------------------------------
class BoxException : public std::exception
{
public:
	BoxException(const std::string& file, int line)
	: mFile(file),
	  mLine(line)
	{ }

	BoxException(const BoxException &rToCopy)
	: mFile(rToCopy.mFile), mLine(rToCopy.mLine)
	{ }

	~BoxException() throw ();
	
	virtual unsigned int GetType() const throw() = 0;
	virtual unsigned int GetSubType() const throw() = 0;
	bool IsType(unsigned int Type, unsigned int SubType)
	{
		return GetType() == Type && GetSubType() == SubType;
	}
	virtual const std::string& GetMessage() const = 0;

	const std::string& GetFile() const { return mFile; }
	const int          GetLine() const { return mLine; }

private:
	std::string mFile;
	int mLine;
};

#define EXCEPTION_IS_TYPE(exception_obj, type, subtype) \
	exception_obj.IsType(type::ExceptionType, type::subtype)

#endif // BOXEXCEPTION__H

