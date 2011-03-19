
#include "connectiondata.h"

/*
	Constructors
*/

ConnectionData::ConnectionData()
{
	port = -1;
}
ConnectionData::ConnectionData(QString _host, QString _usr, QString _pss, int _port)
{
	host = _host;
	usr = _usr;
	pss = _pss;
	port = _port;
}