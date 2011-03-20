#ifndef CONNECTIONDATA_H
#define CONNECTIONDATA_H

// Forward Class Defintions
class QString;

/*
  Holds the connection data for all server requests
*/

class ConnectionData
{
public:
	QString host;
	QString usr;
	QString pss;
	int port;

	ConnectionData();
	ConnectionData(QString host,
                       QString usr,
                       QString pss,
                       int port);
};

#endif
