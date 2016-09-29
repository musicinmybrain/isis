#ifndef QT5_COMMON_HPP
#define QT5_COMMON_HPP

#include <QImage>
#include "../../data/image.hpp"

namespace isis{
	struct Qt5Log {static const char *name() {return "Qt5";}; enum {use = _ENABLE_LOG};};
	struct Qt5Debug {static const char *name() {return "Qt5Debug";}; enum {use = _ENABLE_DEBUG};};

namespace qt5{

	typedef Qt5Debug Debug;
	typedef Qt5Log Runtime;

	template<typename HANDLE> void enableLog( LogLevel level )
	{
		ENABLE_LOG( Qt5Log, HANDLE, level );
		ENABLE_LOG( Qt5Debug, HANDLE, level );
	}

	QImage makeQImage(const data::ValueArrayBase &data,size_t line_length,data::scaling_pair scaling = data::scaling_pair() );
	QImage makeQImage(const data::ValueArrayBase& data, size_t line_length, const std::function<void (uchar *, size_t, const data::ValueArrayBase &)> &transfer_function);
}
}

#endif //QT5_COMMON_HPP
